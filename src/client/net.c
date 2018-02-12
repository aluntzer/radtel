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
#include <cmd_proc.h>


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
 */

static void net_buffer_ready(GObject *source_object, GAsyncResult *res,
			     gpointer user_data)
{
	gsize nbytes = 0;
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

	/* update stream byte count */
	g_buffered_input_stream_peek_buffer(bistream, &c->nbytes);

	pkt_hdr_to_host_order(pkt);

	/* verify packet payload */
	if (CRC16(pkt->data, pkt->data_size) == pkt->data_crc16)  {
		process_cmd_pkt(pkt);
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

	g_bytes_unref(g_input_stream_read_bytes(istream, ret, NULL, &error));

	c->nbytes = 0;

	cmd_invalid_pkt();

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

void net_send(const char *pkt, gsize nbytes)
{
	gssize ret;

	GError *error = NULL;

	GIOStream *stream;
	GOutputStream *ostream;


	g_message("Sending packet of %d bytes", nbytes);

	stream = G_IO_STREAM(server_con.con);
	ostream = g_io_stream_get_output_stream(stream);


	if (g_io_stream_is_closed(stream)) {
		g_message("Error sending packet: stream closed\n");
		return;
	}


	if (!g_socket_connection_is_connected(server_con.con)) {
		g_message("Error sending packet: socket not connected\n");
		return;
	}

	ret = g_output_stream_write(ostream, pkt, nbytes, NULL, &error);

	if (ret < 0) {
		if (error) {
			g_error("%s", error->message);
			g_clear_error (&error);
		}
	}
}


#include <signals.h>

void handle_cmd_success_event1(gpointer instance)
{
	g_message("Event \"cmd-success\" signalled (1)");
}

void handle_cmd_success_event2(gpointer instance)
{
	g_message("Event \"cmd-success\" signalled (2)");
	cmd_capabilities();
}

void handle_cmd_capabilities_event(gpointer instance, const struct capabilities *c)
{
	g_message("Event \"cmd-capabilities\" signalled");

	g_message("c->freq_min_hz %lu", c->freq_min_hz);
	g_message("c->freq_max_hz %lu", c->freq_max_hz);
	g_message("c->freq_inc_hz %d", c->freq_inc_hz);
	g_message("c->bw_max_hz %d", c->bw_max_hz);
	g_message("c->bw_max_div_lin %d", c->bw_max_div_lin);
	g_message("c->bw_max_div_rad2 %d", c->bw_max_div_rad2);
	g_message("c->bw_max_bins %d", c->bw_max_bins);
	g_message("c->bw_max_bin_div_lin %d", c->bw_max_bin_div_lin);
	g_message("c->bw_max_bin_div_rad2 %d", c->bw_max_bin_div_rad2);


}

/**
 * initialise client networking
 *
 * @note requires signal server to be initialised
 */

int net_client_init(void)
{
	gboolean ret;

	GSocketClient *client;
	GSocketConnection *con;

	GError *error = NULL;


	client = g_socket_client_new();
#if 0
	con = g_socket_client_connect_to_host(client,
					      (gchar*)"radtel.astro.univie.ac.at",
					      2345, NULL, &error);
#else
	con = g_socket_client_connect_to_host(client,
					      (gchar*)"localhost",
					      2345, NULL, &error);
#endif
	if (error) {
		g_warning("%s\n", error->message);
		g_clear_error(&error);
		return -1;
	}

	net_setup_recv(con);


	g_message("Client started");

	g_signal_connect(sig_get_instance(), "cmd-success",
			  (GCallback) handle_cmd_success_event1,
			  NULL);

	g_signal_connect(sig_get_instance(), "cmd-success",
			  (GCallback) handle_cmd_success_event2,
			  NULL);
	g_signal_connect(sig_get_instance(), "cmd-capabilities",
			  (GCallback) handle_cmd_capabilities_event,
			  NULL);



#if 0
	cmd_capabilities();
	cmd_recalibrate_pointing();
	cmd_park_telescope();
	cmd_moveto_azel(20., 20.);
#endif

	cmd_spec_acq_start(1420400000, 1420900000, 1, 1, 1, 2);



	return 0;
}
