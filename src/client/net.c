/**
 * @file    client/net.c
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
 * @todo master/slave
 */

#include <net.h>
#include <cmd.h>
#include <pkt_proc.h>


#include <gio/gio.h>
#include <glib.h>


/* server connection data */
struct con_data {
	GSocketConnection *con;
	gsize nbytes;
} server_con;




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
 * @brief drop a connection
 */

static void drop_connection(struct con_data *c)
{
	gboolean ret;

	GError *error = NULL;


	ret = g_socket_close(g_socket_connection_get_socket(c->con), &error);

	if (!ret) {
		if (error) {
			g_warning("%s", error->message);
			g_clear_error(&error);
		}
	}

	g_warning("Dropped connection to server!");
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

		if (pkt_size < MAX_PAYLOAD_SIZE) {
			g_message("Increasing input buffer to packet size\n");
			g_buffered_input_stream_set_buffer_size(bistream,
								pkt_size);
			goto exit;
		} else {
			goto drop_pkt;
		}
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
		process_pkt(pkt);
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


	/* Attempt to drop the current packet at this point. Since there may
	 * be more data in the pipeline, we may subsequently receive a couple
	 * of invalid packets.
	 */

drop_pkt:
	g_message("Error occured, dropping input buffer and packet.");

	g_free(pkt);

	ret = g_buffered_input_stream_fill_finish(bistream, res, &error);
	if (ret < 0)
		goto error;

	g_bytes_unref(g_input_stream_read_bytes(istream, ret, NULL, &error));

	c->nbytes = 0;

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
		g_error ("%s", error->message);
		g_clear_error (&error);
	}
	drop_connection(c);
	return;
}


/**
 * @brief configure client data reception
 */

static void net_setup_recv(GSocketConnection *con)
{
	gsize bufsize;

	GInputStream *istream;
	GBufferedInputStream *bistream;



	server_con.con = con;
	server_con.nbytes = 0;
	/* set up as buffered input stream with default size
	 * the server will tell us the maximum packet size on connect
	 */

	istream = g_io_stream_get_input_stream(G_IO_STREAM(con));
	istream = g_buffered_input_stream_new(istream);

	bistream = G_BUFFERED_INPUT_STREAM(istream);

	bufsize = g_buffered_input_stream_get_buffer_size(bistream);

	g_socket_set_keepalive(g_socket_connection_get_socket(con), TRUE);


	g_buffered_input_stream_fill_async(bistream, bufsize,
					   G_PRIORITY_DEFAULT, NULL,
					   net_buffer_ready, &server_con);

}


/**
 * @brief send a packet to the server
 */

gint net_send(const char *pkt, size_t nbytes)
{
	gssize ret;

	GError *error = NULL;

	GIOStream *stream;
	GOutputStream *ostream;


	g_debug("Sending packet of %d bytes", nbytes);

	stream = G_IO_STREAM(server_con.con);

	if (!stream) {
		g_warning("Remote not connected, cannot send packet");
		return -1;
	}

	ostream = g_io_stream_get_output_stream(stream);


	if (g_io_stream_is_closed(stream)) {
		g_message("Error sending packet: stream closed\n");
		return -1;
	}


	if (!g_socket_connection_is_connected(server_con.con)) {
		g_message("Error sending packet: socket not connected\n");
		return -1;
	}

	ret = g_output_stream_write(ostream, pkt, nbytes, NULL, &error);

	if (ret < 0) {
		if (error) {
			g_error("%s", error->message);
			g_clear_error (&error);
		}
	}

	return ret;
}


/**
 * initialise client networking
 *
 * @note requires signal server to be initialised
 */

int net_client_init(void)
{
	GSocketClient *client;
	GSocketConnection *con;

	GSettings *s;

	GError *error = NULL;

	gchar *host;

	guint16 port;


	s = g_settings_new("org.uvie.radtel.config");
	if (!s)
		return 0;

	host = g_settings_get_string(s, "server-addr");
	if (!host) {
		g_warning("No host address specified!");
		return 0;
	}

	port = (guint16) g_settings_get_uint(s, "server-port");


	client = g_socket_client_new();
	con = g_socket_client_connect_to_host(client, host, port, NULL, &error);

	if (error) {
		g_warning("%s\n", error->message);
		g_clear_error(&error);
		return -1;
	}

	net_setup_recv(con);

	g_message("Client started");

	cmd_capabilities(PKT_TRANS_ID_UNDEF);
	cmd_getpos_azel(PKT_TRANS_ID_UNDEF);



	return 0;
}
