/**
 * @file    acks/ack_getpos_azel.c
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
#include <net_common.h>


struct packet *ack_getpos_azel_gen(uint16_t trans_id, struct getpos *pos)
{
	gsize pkt_size;

	struct packet *pkt;


	pkt_size = sizeof(struct packet) + sizeof(struct getpos);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_GETPOS_AZEL;
	pkt->trans_id  = trans_id;
	pkt->data_size = sizeof(struct getpos);

	memcpy(pkt->data, pos, pkt->data_size);

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	return pkt;
}


/**
 * @brief acknowledge backend getpos_azel command
 *
 * @note the caller is responsible for freeing pos
 */

void ack_getpos_azel(uint16_t trans_id, struct getpos *pos)
{
	struct packet *pkt;


	pkt = ack_getpos_azel_gen(trans_id, pos);

	g_debug("Sending AZEL");
	net_send((void *) pkt, pkt_size_get(pkt));

	g_free(pkt);
}
