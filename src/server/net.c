/**
 * @file    server/net.c
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief server networking
 *
 * @note we typically don't check for NULL pointers since we rely on glib to
 *	 work properly...
 *
 * @todo some cleanup, the logic is pretty confusing
 *
 */

#include <net.h>
#include <cfg.h>
#include <cmd.h>
#include <ack.h>
#include <pkt_proc.h>

#include <gio/gio.h>
#include <glib.h>


/* client connection data */
struct con_data {
	GSocketConnection *con;
	gsize nbytes;
	gboolean priv;
	gchar *nick;
	gboolean new;
	gboolean kick;
};

/* tracks client connections */
static GList *con_list;

static GMutex netlock;


/**
 * @brief get the total size of a packet
 * @note this only used when we peek into the stream, when the header byte order
 *	 conversion is not yet done
 */

static gsize get_pkt_size_peek(struct packet *pkt)
{
	return (gsize) g_ntohl(pkt->data_size) + sizeof(struct packet);
}


static gboolean net_push_userlist_cb(gpointer data)
{
	GList *elem;

	struct con_data *c;

	gchar *buf;
	gchar *tmp;

	gchar *msg = NULL;



	for (elem = con_list; elem; elem = elem->next) {

		c = (struct con_data *) elem->data;

		tmp = msg;

		if (c->priv)
			buf = g_strdup_printf("<tt><span foreground='#FF0000'>"
					      "%s</span></tt>\n", c->nick);
		else
			buf = g_strdup_printf("<tt><span foreground='#7F9F7F'>"
					      "%s</span></tt>\n", c->nick);

		msg = g_strconcat(buf, tmp, NULL);

		g_free(buf);
		g_free(tmp);

		if (c->new) {
			c->new = FALSE;

			buf = g_strdup_printf("<tt><span foreground='#F1C40F'>"
					      "%s</span></tt> joined",
					      c->nick);

			net_server_broadcast_message(buf, NULL);
			g_free(buf);
		}
	}


	if (msg) {
		ack_userlist(PKT_TRANS_ID_UNDEF, msg, strlen(msg));
		g_free(msg);
	}

	return G_SOURCE_REMOVE;
}


/**
 * @brief drop a connection
 */

static void drop_connection(struct con_data *c)
{
	gchar *buf;

	if (c->kick) {
		buf = g_strdup_printf("I kicked <tt><span foreground='#F1C40F'>"
				      "%s</span></tt> for being a lazy bum",
				      c->nick);
	} else {
		buf = g_strdup_printf("<tt><span foreground='#F1C40F'>"
				      "%s</span></tt> disconnected",
				      c->nick);
	}

	net_server_broadcast_message(buf, NULL);

	g_free(buf);
	g_free(c->nick);

	g_object_unref(c->con);

	con_list = g_list_remove(con_list, c);

	net_push_userlist_cb(NULL);
}


/**
 * @brief send a packet on a connection
 */

static gint net_send_internal(struct con_data *c, const char *pkt, gsize nbytes)
{
	gssize ret = 0;

	guint to;

	GError *error = NULL;

	GIOStream *stream;
	GOutputStream *ostream;


	stream = G_IO_STREAM(c->con);

	if (!G_IS_IO_STREAM(stream))
		return -1;

	if (c->kick) {
		GSocket *socket;

		socket = g_socket_connection_get_socket(c->con);

		if (!g_socket_is_closed(socket)) {
			ret = g_socket_close(socket, &error);
			g_warning("Closed socket on timed out client");

			if (!ret) {
				if (error) {
					g_warning("%s", error->message);
					g_clear_error(&error);
				}
			}
		}

		return -1;

	}


	ostream = g_io_stream_get_output_stream(stream);


	if (g_io_stream_is_closed(stream)) {
		g_message("Error sending packet: stream closed\n");
		return -1;
	}


	if (!g_socket_connection_is_connected(c->con)) {
		g_message("Error sending packet: socket not connected\n");
		return -1;
	}

	/* set a 1 second socket timeout to detect broken connections */
	g_socket_set_timeout(g_socket_connection_get_socket(c->con), 1);
	ret = g_output_stream_write_all(ostream, pkt, nbytes, NULL, NULL, &error);
	/* set back to infinite */
	g_socket_set_timeout(g_socket_connection_get_socket(c->con), 0);

	if (!ret) {
		if (error) {
			g_warning("%s", error->message);
			g_clear_error (&error);
		}

		c->kick = TRUE;

	}

	return ret;
}


/**
 * @brief process the buffered network input
 *
 * @note looks like none of these can be used to detect a disconnect
 *	 of a client:
 *	 g_io_stream_is_closed()
 *	 g_socket_connection_is_connected()
 *	 g_socket_is_closed(g_socket_connection_get_socket()
 *	 g_socket_is_connected()
 *	 g_socket_get_available_bytes()
 *	 tried with glib 2.52.2+9+g3245eba16-1)
 *	 we'll just record the amount of bytes in the stream and check
 *	whether it changed or not. if delta == 0 -> disconnected
 *
 * XXX after fixing up all bugs, this function has really nasty jump logic and
 *     needs rework asap
 */

static void net_buffer_ready(GObject *source_object, GAsyncResult *res,
			     gpointer user_data)
{
	gsize nbytes;
	gsize pkt_size;

	gssize ret;

	const char *buf;

	struct packet *pkt = NULL;
	struct con_data *c;

	GInputStream *istream;
	GBufferedInputStream *bistream;

	GError *error = NULL;
	GBytes *b;


	c = (struct con_data *) user_data;

	istream  = G_INPUT_STREAM(source_object);
	bistream = G_BUFFERED_INPUT_STREAM(source_object);


pending:
	buf = g_buffered_input_stream_peek_buffer(bistream, &nbytes);

	if (nbytes == c->nbytes) {
		g_message("No new bytes in client stream, dropping connection");
		goto error;
	}

	c->nbytes = nbytes;


	/* enough bytes to hold a packet? */
	if (nbytes < sizeof(struct packet)) {
		g_message("Input stream smaller than packet size.");
		goto exit;
	}


	/* check if the packet is complete/valid */
	pkt_size = get_pkt_size_peek((struct packet *) buf);

	if (pkt_size > g_buffered_input_stream_get_buffer_size(bistream)) {
		g_message("Packet of %ld bytes is larger than input buffer of "
			  "%ld bytes.", pkt_size,
			  g_buffered_input_stream_get_buffer_size(bistream));

	if (net_send(buf, nbytes) < 0)
		goto error;


		if (pkt_size < MAX_PAYLOAD_SIZE) {
			g_message("Increasing input buffer to packet size\n");
			g_buffered_input_stream_set_buffer_size(bistream,
								pkt_size);
		}
		goto drop_pkt;
	}


	if (pkt_size > nbytes) {
		g_message("Packet (%ld bytes) incomplete, %ld bytes in stream",
			  pkt_size, nbytes);
		goto exit;
	}


	/* we have a packet */

	/* the packet buffer will be released in the command processor */
	pkt = g_malloc(pkt_size);

	/* extract packet for command processing */
	nbytes = g_input_stream_read(istream, (void *) pkt,
				     pkt_size, NULL, &error);

	if (nbytes <= 0)
		goto error;

#if 0
	/* update stream byte count */
	g_buffered_input_stream_peek_buffer(bistream, &c->nbytes);
#endif
	pkt_hdr_to_host_order(pkt);

	/* verify packet payload */
	if (CRC16(pkt->data, pkt->data_size) == pkt->data_crc16)  {
		if (process_pkt(pkt, c->priv, c))
			goto drop_pkt;

		/* valid packets were free'd */
		pkt = NULL;

		c->nbytes = 0;

		g_buffered_input_stream_peek_buffer(bistream, &nbytes);

		if (nbytes)
			goto pending;

		goto exit;

	} else {
		g_message("Invalid CRC16 %x %x",
			  CRC16(pkt->data, pkt->data_size), pkt->data_crc16);
	}


drop_pkt:
	g_message("Error occured, dropping input buffer and packet.");

	g_free(pkt);

	ret = g_buffered_input_stream_fill_finish(bistream, res, &error);
	if (ret < 0)
		goto error;

	ret = g_buffered_input_stream_get_available(bistream);
	g_bytes_unref(g_input_stream_read_bytes(istream, ret, NULL, &error));

	c->nbytes = 0;

	cmd_invalid_pkt(PKT_TRANS_ID_UNDEF);

exit:

	/* continue buffering */
	g_buffered_input_stream_fill_async(bistream,
			   g_buffered_input_stream_get_buffer_size(bistream),
			   G_PRIORITY_DEFAULT,
			   NULL,
			   net_buffer_ready,
			   c);
	return;


error:
	g_message("Error occured, dropping connection");
	if (error) {
		g_warning("%s", error->message);
		g_clear_error(&error);

	}
	drop_connection(c);
	return;

}


/**
 * @brief handle an incoming connection
 */

static gboolean net_incoming(GSocketService    *service,
			     GSocketConnection *connection,
			     GObject           *source_object,
			     gpointer           user_data)
{
	gsize bufsize;

	GSocketAddress *addr;
	GInetAddress   *iaddr;

	GInputStream *istream;
	GBufferedInputStream *bistream;

	GList *elem;

	struct con_data *c;
	struct con_data *item;

	gboolean has_priv = FALSE;

	gchar *str;


	c = g_malloc0(sizeof(struct con_data));

	addr = g_socket_connection_get_remote_address(connection, NULL);
	iaddr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(addr));
	str = g_inet_address_to_string(iaddr);

	g_message("Received incoming connection from %s", str);

	c->nick = g_strdup_printf("UserUnknown (%s)", str);
	c->new = TRUE;
	c->kick = FALSE;

	g_free(str);


	/* set up as buffered input stream */
	istream = g_io_stream_get_input_stream(G_IO_STREAM(connection));
	istream = g_buffered_input_stream_new(istream);

	bistream = G_BUFFERED_INPUT_STREAM(istream);

	bufsize = g_buffered_input_stream_get_buffer_size(bistream);

	/* reference, so it is not dropped by glib */
	c->con = g_object_ref(connection);

	g_socket_set_keepalive(g_socket_connection_get_socket(c->con), TRUE);

	/* add to list of connections */
	con_list = g_list_append(con_list, c);

	/* see if anyone has control, if not, assign  to current connection */
	for (elem = con_list; elem; elem = elem->next) {
		item = (struct con_data *) elem->data;
		if (item->priv)
			has_priv = TRUE;
	}

	if (!has_priv)
		c->priv = TRUE;


	/* push new username after 1 seconds, so they have time to configure
	 * theirs
	 */
	g_timeout_add_seconds(1, net_push_userlist_cb, NULL);

	g_buffered_input_stream_fill_async(bistream, bufsize,
					   G_PRIORITY_DEFAULT, NULL,
					   net_buffer_ready, c);


	return FALSE;
}



void net_server_reassign_control(gpointer ref)
{
	GSocketAddress *addr;
	GInetAddress   *iaddr;

	GList *elem;

	struct con_data *c;
	struct con_data *item;

	gchar *msg;
	gchar *str;

	c = (struct con_data *) ref;

	for (elem = con_list; elem; elem = elem->next) {
		item = (struct con_data *) elem->data;
		item->priv = FALSE;
	}

	c->priv = TRUE;

	addr = g_socket_connection_get_remote_address(c->con, NULL);
	iaddr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(addr));
	str = g_inet_address_to_string(iaddr);
	msg = g_strdup_printf("Reassigned control to %s (connected from %s)",
			      c->nick, str);

	net_server_broadcast_message(msg, NULL);
	net_push_userlist_cb(NULL);

	g_free(msg);
	g_free(str);
}



/**
 * @brief send a packet to single client
 *
 * @returns <0 on error
 */

gint net_send_single(gpointer ref, const char *pkt, gsize nbytes)
{
	gint ret;

	GList *elem;

	struct con_data *c;


	c = (struct con_data *) ref;

	g_debug("Sending packet of %d bytes", nbytes);

	g_mutex_lock(&netlock);

	ret = net_send_internal(c, pkt, nbytes);

	g_mutex_unlock(&netlock);

	return ret;
}


/**
 * @brief se3nd a packet to all connected clients
 *
 * @returns <0 on error
 */

gint net_send(const char *pkt, gsize nbytes)
{
	GList *elem;

	struct con_data *item;



	g_debug("Broadcasting packet of %d bytes", nbytes);

	for (elem = con_list; elem; elem = elem->next) {
		item = (struct con_data *) elem->data;
		net_send_single(item, pkt, nbytes);
	}


	return 0;
}


void net_server_set_nickname(const gchar *nick, gpointer ref)
{
	gchar *old;
	gchar *buf;

	struct con_data *c;


	c = (struct con_data *) ref;


	old = c->nick;

	c->nick = g_strdup(nick);

	if (!c->new) {
		buf = g_strdup_printf("<tt><span foreground='#F1C40F'>%s</span></tt> "
				      "is now known as "
				      "<tt><span foreground='#F1C40F'>%s</span></tt> ",
				      old, c->nick);

		net_server_broadcast_message(buf, NULL);

		g_free(buf);
	}

	net_push_userlist_cb(NULL);

	g_free(old);
}


void net_server_broadcast_message(const gchar *msg, gpointer ref)
{
	gchar *buf;
	gchar *user;


	struct con_data *c;


	c = (struct con_data *) ref;


	if (!c) {
		buf = g_strdup_printf("<tt><span foreground='#FF0000'>"
				      "A hollow voice says:</span></tt> %s\n",
				      msg);
	} else {
		buf = g_strdup_printf("<tt><span foreground='#7F9F7F'>"
				      "%s:</span></tt> %s\n",
				      c->nick, msg);
	}


	cmd_message(PKT_TRANS_ID_UNDEF, buf, strlen(buf));

	g_free(buf);
}



/**
 * initialise server networking
 */

int net_server(void)
{
	gboolean ret;

	guint16 port;

	GMainLoop *loop;
	GSocketService *service;

	GError *error = NULL;


	port = server_cfg_get_port();
	if (!port)
		port = DEFAULT_PORT;

	service = g_socket_service_new();

	ret = g_socket_listener_add_inet_port(G_SOCKET_LISTENER(service),
					      port, NULL, &error);
	if (!ret) {
		if (error) {
			g_error("%s", error->message);
			g_clear_error(&error);
		}

		return -1;
	}

	g_signal_connect(service, "incoming", G_CALLBACK(net_incoming), NULL);

	g_socket_service_start(service);


	loop = g_main_loop_new(NULL, FALSE);

	g_message("Server started on port %d", port);

	g_main_loop_run(loop);

	/* stop service when leaving main loop */
	g_socket_service_stop (service);


	return 0;
}
