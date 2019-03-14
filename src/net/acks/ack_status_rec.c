/**
 * @file    acks/ack_status_rec.c
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



/**
 * @brief acknowledge recording status
 */

void ack_status_rec(uint16_t trans_id, struct status *c)
{
	gsize pkt_size;

	struct packet *pkt;


	pkt_size = sizeof(struct packet) + sizeof(struct status);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_STATUS_REC;
	pkt->trans_id  = trans_id;
	pkt->data_size = sizeof(struct status);


	memcpy(pkt->data, c, sizeof(struct status));

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	g_debug("Sending REC status");
	net_send((void *) pkt, pkt_size);

	/* clean up */
	g_free(pkt);
}
