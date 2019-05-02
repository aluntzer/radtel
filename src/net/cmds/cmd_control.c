/**
 * @file    net/cmds/cmd_control.c
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

#include <cmd.h>


void cmd_control(uint16_t trans_id, const uint8_t *digest, uint16_t len)
{
	gsize pkt_size;
	gsize data_size;

	struct control *c;
	struct packet *pkt;


	if (len != strlen(digest))
	       	return;

	/* all strings terminate with \0 char, we transport that as well! */
	data_size = sizeof(struct control) + (len + 1) * sizeof(uint8_t);

	pkt_size = sizeof(struct packet) + data_size;

	pkt = g_malloc(pkt_size);

	pkt->service   = PR_CONTROL;
	pkt->trans_id  = trans_id;
	pkt->data_size = data_size;

	c = (struct control *) pkt->data;

	c->len = len;
	memcpy(c->digest, digest, data_size);

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	g_debug("Requesting telescope control");

	net_send((void *) pkt, pkt_size);

	/* clean up */
	g_free(pkt);
}
