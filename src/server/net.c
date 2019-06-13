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

/* this number must be large enough to handle multiple subsequent submissions
 * of packets for a connection
 */
#define SERVER_CON_POOL_SIZE 16

/* max allowed client */
#define SERVER_CON_MAX 64

/* privilege range */
#define PRIV_DEFAULT	0
#define PRIV_CONTROL	1
#define PRIV_FULL	2



/* client connection data */
struct con_data {
	GSocketConnection *con;
	GInputStream *istream;
	gsize nbytes;
	gint priv;
	gchar *nick;
	gboolean new;
	gboolean kick;
	GMutex lock;
	GCancellable *ca;

	GThreadPool *pool;
};

struct thread {
	gchar *buf;
	gsize bytes;
};

/* tracks client connections */
static GList *con_list;

static GMutex netlock;
static GMutex netlock_big;
static GMutex listlock;
static GMutex finalize;


/**
 * @brief get the total size of a packet
 * @note this only used when we peek into the stream, when the header byte order
 *	 conversion is not yet done
 */

static gsize get_pkt_size_peek(struct packet *pkt)
{
	return (gsize) g_ntohl(pkt->data_size) + sizeof(struct packet);
}


/**
 * @brief returns the host address as string
 */

static gchar *net_get_host_string(GSocketConnection *con)
{
	GSocketAddress *addr;
	GInetAddress   *iaddr;


	if (!G_IS_SOCKET_CONNECTION(con))
		return NULL;

	if (!g_socket_connection_is_connected(con))
		return NULL;


	addr = g_socket_connection_get_remote_address(con, NULL);
	iaddr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(addr));

	return g_inet_address_to_string(iaddr);
}


/**
 * @brief generate a chat message string
 *
 * @param msg the message
 * @param c the connection (may be NULL)
 *
 * @returns an allocated buffer holding the string, clean using g_free()
 *
 * @note if c is NULL, it is assumed the server generated the message
 */

static gchar *net_server_msg_nick(const gchar *msg, struct con_data *c)
{
	gchar *buf;
	gchar *nick;
	gchar *col;


	col  = "#FF0000";
	nick = "A hollow voice says";

	if (c) {
		col  = "#7F9F7F";
		nick = c->nick;
	}

	buf = g_strdup_printf("<tt><span foreground='%s'>"
			      "%s:</span></tt> %s\n",
			      col, nick, msg);


	return buf;
}

static gboolean net_push_station_single(gpointer data)
{
	gchar *buf;

	buf = g_strdup_printf("Your are connected to %s\n",
			      server_cfg_get_station());

	net_server_direct_message(buf, data);

	g_free(buf);

	return G_SOURCE_REMOVE;
}


static gboolean net_push_motd_single(gpointer data)
{
	gchar *buf;
	gchar *motd;


	motd = server_cfg_get_motd();
	if (!motd)
		goto exit;


	buf = g_strdup_printf("The MOTD is: \n\n%s\n\n", motd);

	net_server_direct_message(buf, data);

	g_free(buf);
	g_free(motd);

exit:
	return G_SOURCE_REMOVE;
}

static void net_push_motd_update(void)
{
	gchar *buf;
	gchar *motd;


	motd = server_cfg_get_motd();
	if (!motd)
		return;

	buf = g_strdup_printf("The MOTD has been updated and now reads: "
			      "\n\n%s\n\n", motd);

	net_server_broadcast_message(buf, NULL);

	g_free(buf);
	g_free(motd);
}


/**
 * @brief distribute a list of users to all clients
 */

static gboolean net_push_userlist_cb(gpointer data)
{
	GList *elem;

	struct con_data *c;

	gchar **msgs;
	gchar *buf;
	gchar *tmp;

	gint i;
	gint msgcnt = 0;

	gchar *msg = NULL;


	g_mutex_lock(&listlock);

	msgs = g_malloc(g_list_length(con_list) * sizeof(gchar *));

	for (elem = con_list; elem; elem = elem->next) {

		c = (struct con_data *) elem->data;

		tmp = msg;

		switch (c->priv) {
		case PRIV_FULL:
			buf = g_strdup_printf("<tt><span foreground='#FF0000'>"
					      "%s</span></tt>\n", c->nick);
			break;
		case PRIV_CONTROL:
			buf = g_strdup_printf("<tt><span foreground='#FFFF00'>"
					      "%s</span></tt>\n", c->nick);
			break;
		default:
			buf = g_strdup_printf("<tt><span foreground='#7F9F7F'>"
					      "%s</span></tt>\n", c->nick);
			break;
		}
		msg = g_strconcat(buf, tmp, NULL);

		g_free(buf);
		g_free(tmp);

		if (c->new) {
			c->new = FALSE;

			msgs[msgcnt++] = g_strdup_printf("<tt><span foreground='#F1C40F'>"
							 "%s</span></tt> joined",
							 c->nick);

			g_print("%s joined\n", c->nick);

		}
	}

	g_mutex_unlock(&listlock);

	for (i = 0; i < msgcnt; i++) {
		net_server_broadcast_message(msgs[i], NULL);
		g_free(msgs[i]);
	}

	g_free(msgs);

	if (msg) {
		ack_userlist(PKT_TRANS_ID_UNDEF, (guchar *) msg, strlen(msg));
		g_free(msg);
	}


	return G_SOURCE_REMOVE;
}



/**
 * @brief if connected, disconnect the socket
 */

static void try_disconnect_socket(struct con_data *c)
{
	gboolean ret = TRUE;

	GSocket *s;
	GError *error = NULL;


	if (!G_IS_SOCKET_CONNECTION(c->con))
		return;

	if (!g_socket_connection_is_connected(c->con))
		return;

	s = g_socket_connection_get_socket(c->con);

	if (g_socket_is_connected(s))
		ret = g_socket_close(s, &error);


	if (!ret) {
		if (error) {
			g_warning("%s:%d %s", __func__, __LINE__, error->message);
			g_clear_error(&error);
		}
	}

	/* drop initial reference */
	g_clear_object(&c->con);
}


/**
 * @brief initiate a connection drop
 */

static void drop_con_begin(struct con_data *c)
{

	gchar *str;

	if (!c->con)
		return;

	if (!G_IS_OBJECT(c->con))
		return;

	g_mutex_lock(&listlock);

	str = net_get_host_string(c->con);
	g_message("Initiating disconnect for %s (%s)", str, c->nick);
	g_free(str);

	con_list = g_list_remove(con_list, c);

	/* signal operations to stop */
	g_cancellable_cancel(c->ca);

 	g_thread_pool_free(c->pool, TRUE, FALSE);
	c->pool = NULL;

	try_disconnect_socket(c);

	g_mutex_unlock(&listlock);
}


/**
 * @brief finalize a connection drop
 *
 * @note the connection must already be removed from con_list at this point!
 */

static void drop_con_finalize(struct con_data *c)
{
	gchar *buf;


	g_mutex_lock(&finalize);

	if (!c) {
		g_warning("c is NULL");
		goto unlock;
	}
	if (c->con) {
		if (G_IS_OBJECT(c->con)) {
			g_warning("c->con still holds references");
			goto unlock;
		}
	}

	if (!c->nick) {
		g_warning("double-free attempt");
		goto unlock;
	}

	if (c->kick) {
		buf = g_strdup_printf("I kicked <tt><span foreground='#F1C40F'>"
				      "%s</span></tt> for being a lazy bum "
				      "(client input saturated or timed out)",
				      c->nick);

		g_print("%s was kicked\n", c->nick);
	} else {
		buf = g_strdup_printf("<tt><span foreground='#F1C40F'>"
				      "%s</span></tt> disconnected",
				      c->nick);
		g_print("%s disconnected\n", c->nick);
	}

	net_server_broadcast_message(buf, NULL);
	net_push_userlist_cb(NULL);

	if (G_IS_OBJECT(c->istream))
		g_clear_object(&c->istream);

	if (G_IS_OBJECT(c->ca))
		g_clear_object(&c->ca);

	if (c->nick) {
		g_free(c->nick);
		c->nick = NULL;
	}

	g_free(buf);
	g_free(c);

unlock:

	g_mutex_unlock(&finalize);
}


/**
 * @brief send data on a connection
 */

static void do_send(gpointer data, gpointer user_data)
{
	gboolean ret;

	GOutputStream *os;

	struct thread *th;
	struct con_data *c;



	th = (struct thread *)   data;
	c  = (struct con_data *) user_data;


	if (!G_IS_IO_STREAM(c->con))
		goto exit;

	g_object_ref(c->con);
	os = g_io_stream_get_output_stream(G_IO_STREAM(c->con));

	g_mutex_lock(&c->lock);

	ret = g_output_stream_write_all(os, th->buf, th->bytes, NULL, c->ca,
					NULL);

	if (!ret)
		c->kick = TRUE;

	g_mutex_unlock(&c->lock);

	if (G_IS_OBJECT(c->con))
		g_object_unref(c->con);

exit:
	/* if this was the last reference, call finalize */
	if (!G_IS_OBJECT(c->con))
		drop_con_finalize(c);

	g_free(th->buf);
	g_free(th);
}



/**
 * @brief send a packet on a connection
 *
 * @returns 0 on success, otherwise error
 */

static gboolean net_send_internal(struct con_data *c, const char *pkt, gsize nbytes)
{
	gboolean ret;

	struct thread *th;

	GError *error = NULL;


	if (!c->pool)
		return FALSE;

	if (c->kick)
		return FALSE;

	if (!G_IS_SOCKET_CONNECTION(c->con)) {
		g_warning("%s:%d: supplied argument is not a socket connection",
			  __func__, __LINE__);
		return FALSE;
	}

	if (g_thread_pool_get_num_threads(c->pool) >= SERVER_CON_POOL_SIZE) {

		gchar *str;

		str = net_get_host_string(c->con);

		g_message("Will kick client %s connected from %s: "
			  "dropped pkt, out of threads: %d of %d in use",
			  c->nick, str,
			  g_thread_pool_get_num_threads(c->pool),
			  SERVER_CON_POOL_SIZE);

		c->kick = TRUE;

		g_free(str);

		return FALSE;
	}


	th = g_malloc(sizeof(struct thread));

	th->bytes = nbytes;
	th->buf   = g_memdup(pkt, nbytes);

	ret = g_thread_pool_push(c->pool, (gpointer) th, &error);

	if (!ret) {

		if (error) {
			g_warning("%s:%d %s", __func__, __LINE__,
				  error->message);

			g_clear_error(&error);
		}

		g_free(th->buf);
		g_free(th);
	}


	return ret;
}


/**
 * @brief process the buffered network input
 *
 * XXX while it working flawlessly, this is still a very nasty function
 *     and needs refactoring, but I'll leave it be for now
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


	c = (struct con_data *) user_data;

	istream  = G_INPUT_STREAM(source_object);
	bistream = G_BUFFERED_INPUT_STREAM(source_object);

	if (g_cancellable_is_cancelled(c->ca))
		return;


pending:
	if (!G_IS_OBJECT(c->con))
		return;

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

#if 1 /* careful there, this has abuse potential :) */
		/* here's your shit back */
		if (net_send_single(c, buf, nbytes) < 0)
			goto error;
#endif


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

	pkt_hdr_to_host_order(pkt);

	/* verify packet payload */
	if (CRC16((guchar *) pkt->data, pkt->data_size) == pkt->data_crc16)  {
		if (process_pkt(pkt, (gboolean) c->priv, c))
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
			  CRC16((guchar *) pkt->data, pkt->data_size), pkt->data_crc16);
	}


drop_pkt:
	g_message("Error occured in %s, dropping input buffer and packet.",
		  __func__);

	g_free(pkt);

	ret = g_buffered_input_stream_fill_finish(bistream, res, &error);

	if (ret < 0)
		goto error;

	ret = g_buffered_input_stream_get_available(bistream);
	g_bytes_unref(g_input_stream_read_bytes(istream, ret, NULL, &error));

	c->nbytes = 0;

	cmd_invalid_pkt(PKT_TRANS_ID_UNDEF);

exit:
	if (!G_IS_OBJECT(c->con))
		return;

	/* continue buffering */
	g_buffered_input_stream_fill_async(bistream,
					   g_buffered_input_stream_get_buffer_size(bistream),
					   G_PRIORITY_DEFAULT,
					   c->ca,
					   net_buffer_ready,
					   c);
	return;


error:
	g_message("Error occured in %s, initiating connection drop", __func__);

	if (error) {
		g_warning("%s:%d %s", __func__, __LINE__, error->message);
		g_clear_error(&error);

	}

	drop_con_begin(c);

	/* if this was the last reference, call finalize */
	if (!G_IS_OBJECT(c->con))
		drop_con_finalize(c);

	return;

}


/**
 * @brief see if anyone has control if not, assign  to current connection
 */

static void assign_default_priv(struct con_data *c)
{
	gint priv = PRIV_DEFAULT;

	GList *elem;

	struct con_data *item;


	g_mutex_lock(&listlock);

	for (elem = con_list; elem; elem = elem->next) {

		item = (struct con_data *) elem->data;

		if (item->priv)
			priv = PRIV_CONTROL;
	}

	if (priv == PRIV_DEFAULT)
		c->priv = PRIV_CONTROL;

	g_mutex_unlock(&listlock);
}


/**
 * @brief begin reception of client data
 */

static void begin_reception(struct con_data *c)
{
	gsize bufsize;

	GInputStream *istream;
	GBufferedInputStream *bistream;

	/* set up as buffered input stream */
	istream = g_io_stream_get_input_stream(G_IO_STREAM(c->con));
	c->istream = g_buffered_input_stream_new(istream);

	bistream = G_BUFFERED_INPUT_STREAM(c->istream);
	bufsize = g_buffered_input_stream_get_buffer_size(bistream);

	g_object_ref(c->con);

	g_buffered_input_stream_fill_async(bistream, bufsize,
					   G_PRIORITY_DEFAULT, c->ca,
					   net_buffer_ready, c);
}

/**
 * @brief setup connection details
 */

static void setup_connection(struct con_data *c)
{
	gchar *str;

	str = net_get_host_string(c->con);

	c->nick = g_strdup_printf("UserUnknown (%s)", str);
	c->new = TRUE;
	c->kick = FALSE;
	c->ca = g_cancellable_new();
	g_mutex_init(&c->lock);

	c->pool = g_thread_pool_new(do_send, (gpointer) c,
				    SERVER_CON_POOL_SIZE, FALSE, NULL);

	g_socket_set_keepalive(g_socket_connection_get_socket(c->con), TRUE);

	g_free(str);
}


/**
 * @brief handle an incoming connection
 */

static gboolean net_incoming(GSocketService    *service,
			     GSocketConnection *connection,
			     GObject           *source_object,
			     gpointer           user_data)
{
	gchar *str;

	struct con_data *c;



	if (g_list_length(con_list) > SERVER_CON_MAX) {
		g_warning("Number of active connections exceeds "
			  "%d, dropped incoming", SERVER_CON_MAX);
		g_object_unref(connection);
		return FALSE;
	}

	c = g_malloc0(sizeof(struct con_data));

	/* reference, so it is not dropped by glib */
	c->con = g_object_ref(connection);

	setup_connection(c);
	assign_default_priv(c);
	begin_reception(c);

	/* add to list of connections for outgoing data */
	g_mutex_lock(&listlock);
	con_list = g_list_append(con_list, c);
	g_mutex_unlock(&listlock);

	/* push usernames and messages after 1 seconds, so the incoming
	 * connections have time to configure theirs
	 */
	g_timeout_add_seconds(1, net_push_station_single, c);
	g_timeout_add_seconds(1, net_push_motd_single, c);
	g_timeout_add_seconds(1, net_push_userlist_cb, NULL);

	str = net_get_host_string(c->con);
	g_message("Received connection from %s", str);
	g_free(str);


	return FALSE;
}


/**
 * @brief send a packet to single client
 *
 * @returns <0 on error
 */

gint net_send_single(gpointer ref, const char *pkt, gsize nbytes)
{
	gint ret;

	struct con_data *c;


	c = (struct con_data *) ref;

	g_mutex_lock(&netlock);

	ret = net_send_internal(c, pkt, nbytes);

	g_mutex_unlock(&netlock);

	return ret;
}


/**
 * @brief send a packet to all connected clients
 *
 * @returns <0 on error
 */

gint net_send(const char *pkt, gsize nbytes)
{
	int ret = 0;

	GList *elem;

	struct con_data *c;
	struct con_data *drop = NULL;


	g_mutex_lock(&netlock_big);

	g_mutex_lock(&listlock);

	for (elem = con_list; elem; elem = elem->next) {

		c = (struct con_data *) elem->data;

		if (!G_IS_OBJECT(c->con))
			continue;

		if (c->kick) {
			drop = c;
			continue;
		}

		ret |= net_send_single(c, pkt, nbytes);
	}

	g_mutex_unlock(&listlock);

	/* drop one per cycle */
	if (drop)
		drop_con_begin(drop);

	g_mutex_unlock(&netlock_big);

	return ret;
}

/**
 * @brief assign control privilege level to connection
 */


static void net_server_reassign_control_internal(gpointer ref, gint lvl)
{
	GList *elem;

	struct con_data *c;
	struct con_data *item;

	struct con_data *p = NULL;

	gchar *msg;
	gchar *str;


	c = (struct con_data *) ref;

	g_mutex_lock(&listlock);
	for (elem = con_list; elem; elem = elem->next) {
		item = (struct con_data *) elem->data;

		if (item->priv <= lvl) {
			item->priv = PRIV_DEFAULT;
		} else {
			p = item;
			break;
		}
	}
	g_mutex_unlock(&listlock);

	str = net_get_host_string(c->con);

	if (!p) {
		c->priv = lvl;
		msg = g_strdup_printf("Reassigned control to %s "
				      "(connected from %s)",
				      c->nick, str);
	} else if (p == c) {
		c->priv = lvl;
		msg = g_strdup_printf("%s (connected from %s) changed their "
				      "own privilege level",
				      c->nick, str);
	} else {
		gchar *h;

		h = net_get_host_string(c->con);

		msg = g_strdup_printf("Failed to reassign control to %s "
				      "(connected from %s), as "
				      "%s (connected from %s) holds a higher "
				      "level of privilege",
				      c->nick, str, p->nick, h);
		g_free(h);
	}


	net_server_broadcast_message(msg, NULL);
	net_push_userlist_cb(NULL);

	g_free(msg);
	g_free(str);
}



/**
 * @brief escalate to maximum privilege level
 */

void net_server_iddqd(gpointer ref)
{
	net_server_reassign_control_internal(ref, PRIV_FULL);
}


/**
 * @brief assign control privilege level to connection
 */

void net_server_reassign_control(gpointer ref)
{
	net_server_reassign_control_internal(ref, PRIV_CONTROL);
}

/**
 * @brief drop to lowest priviledge on connection
 */

void net_server_drop_priv(gpointer ref)
{
	net_server_reassign_control_internal(ref, PRIV_DEFAULT);
}

/**
 * @brief set the nickname for a connection
 */

void net_server_set_nickname(const gchar *nick, gpointer ref)
{
	gchar *old;
	gchar *buf;

	struct con_data *c;


	c = (struct con_data *) ref;


	if (!strlen(nick)) {
		g_message("Rejected nickname of zero length");
		return;
	}


	old = c->nick;

	c->nick = g_strdup(nick);

	if (!c->new) {
		buf = g_strdup_printf("<tt><span foreground='#F1C40F'>%s</span>"
				      "</tt> is now known as "
				      "<tt><span foreground='#F1C40F'>%s</span>"
				      "</tt> ", old, c->nick);

		net_server_broadcast_message(buf, NULL);

		g_free(buf);
	}

	net_push_userlist_cb(NULL);

	g_free(old);
}



int net_server_parse_msg(const gchar *msg, gpointer ref)
{
	struct con_data *c;


	c = (struct con_data *) ref;

	/* ignore if not fully priviledged */
	if (c->priv < PRIV_FULL)
		return -1;

	/* shorter than our only supported command word */
	if (strlen(msg) < 5)
	       return -1;

	if (strncmp(msg, "!motd", 5))
		return -1;

	/* stupidly set the motd */
	server_cfg_set_motd(&msg[5]);

	net_push_motd_update();

	return 0;
}


/**
 * @brief broadcast a text message to all clients
 */

void net_server_broadcast_message(const gchar *msg, gpointer ref)
{
	gchar *buf;

	struct con_data *c;


	c = (struct con_data *) ref;

	buf = net_server_msg_nick(msg, c);

	cmd_message(PKT_TRANS_ID_UNDEF, (guchar *) buf, strlen(buf));

	g_free(buf);
}


/**
 * @brief send a text message to a client
 *
 */

void net_server_direct_message(const gchar *msg, gpointer ref)
{
	gchar *buf;

	struct con_data *c;
	struct packet *pkt;


	c = (struct con_data *) ref;

	buf = net_server_msg_nick(msg, NULL);

	pkt = cmd_message_gen(PKT_TRANS_ID_UNDEF, (uint8_t *) buf, strlen(buf));

	net_send_single(c, (void *) pkt, pkt_size_get(pkt));

	/* clean up */
	g_free(pkt);
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
