/**
 * @file    acks/ack_status_acq.c
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



struct packet *ack_status_acq_gen(uint16_t trans_id, struct status *c)
{
	gsize pkt_size;

	struct packet *pkt;


	pkt_size = sizeof(struct packet) + sizeof(struct status);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_STATUS_ACQ;
	pkt->trans_id  = trans_id;
	pkt->data_size = sizeof(struct status);


	memcpy(pkt->data, c, sizeof(struct status));

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);


	return pkt;
}



/**
 * @brief acknowledge acquisition status
 */

void ack_status_acq(uint16_t trans_id, struct status *c)
{
	struct packet *pkt;


	pkt = ack_status_acq_gen(trans_id, c);

	g_debug("Sending ACQ status");
	net_send((void *) pkt, pkt_size_get(pkt));

	/* clean up */
	g_free(pkt);
}
