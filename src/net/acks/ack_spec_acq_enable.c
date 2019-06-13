/**
 * @file    acks/ack_spec_acq_enable.c
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


struct packet *ack_spec_acq_enable_gen(uint16_t trans_id)
{
	gsize pkt_size;

	struct packet *pkt;


	pkt_size = sizeof(struct packet);

	pkt = g_malloc(pkt_size);

	pkt->service   = PR_SPEC_ACQ_ENABLE;
	pkt->trans_id  = trans_id;
	pkt->data_size = 0;


	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	return pkt;
}


/**
 * @brief acknowledge backend spec_acq_enable command
 */

void ack_spec_acq_enable(uint16_t trans_id)
{
	struct packet *pkt;


	pkt = ack_spec_acq_enable_gen(trans_id);

	g_debug("Sending SPEC ACQ ENABLE");
	net_send((void *) pkt, pkt_size_get(pkt));

	g_free(pkt);
}
