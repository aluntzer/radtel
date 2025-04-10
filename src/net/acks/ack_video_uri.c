/**
 * @file    net/acks/ack_video_uri.c
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
 */

#include <glib.h>

#include <ack.h>
#include <protocol.h>


struct packet *ack_video_uri_gen(uint16_t trans_id, const uint8_t *uri,
				 uint16_t len)
{
	gsize pkt_size;
	gsize data_size;

	struct video_uri *c;
	struct packet *pkt;


	if (len != strlen(uri))
	       	return NULL;

	/* all strings terminate with \0 char, we transport that as well! */
	data_size = sizeof(struct video_uri) + (len + 1) * sizeof(uint8_t);

	pkt_size = sizeof(struct packet) + data_size;

	pkt = g_malloc(pkt_size);

	pkt->service   = PR_VIDEO_URI;
	pkt->trans_id  = trans_id;
	pkt->data_size = data_size;

	c = (struct video_uri *) pkt->data;

	c->len = len;
	memcpy(c->video_uri, uri, len + 1);

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	return pkt;
}


void ack_video_uri(uint16_t trans_id, const uint8_t *uri, uint16_t len)
{
	struct packet *pkt;


	pkt = ack_video_uri_gen(trans_id, uri, len);
	if (!pkt)
		return;

	g_debug("Sending video_uri: %s", uri);

	net_send((void *) pkt, pkt_size_get(pkt));

	/* clean up */
	g_free(pkt);
}
