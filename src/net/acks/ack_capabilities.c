/**
 * @file    acks/ack_capabilities.c
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
 * @brief acknowledge backend capabilities command
 */

struct packet *ack_capabilities_gen(uint16_t trans_id, struct capabilities *c)
{
	gsize pkt_size;
	gsize data_size;

	struct packet *pkt;


	/* data size is packet plus horizon profile arrays */
	data_size = sizeof(struct capabilities)
		    + c->n_hor * sizeof(struct local_horizon);

	pkt_size = sizeof(struct packet) + data_size;


	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_CAPABILITIES;
	pkt->trans_id  = trans_id;
	pkt->data_size = data_size;


	memcpy(pkt->data, c, data_size);

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	return pkt;
}


void ack_capabilities(uint16_t trans_id, struct capabilities *c)
{
	struct packet *pkt;


	pkt = ack_capabilities_gen(trans_id, c);

	g_debug("Sending capabilities");
	net_send((void *) pkt, pkt_size_get(pkt));

	/* clean up */
	g_free(pkt);
}
