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



/**
 * @brief acknowledge backend spec_acq_enable command
 */

void ack_spec_acq_enable(void)
{
	gsize pkt_size;

	struct packet *pkt;


	pkt_size = sizeof(struct packet);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_SPEC_ACQ_ENABLE;
	pkt->data_size = 0;


	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	g_message("Sending SPEC ACQ ENABLE");
	net_send((void *) pkt, pkt_size);

cleanup:
	g_free(pkt);
}