/**
 * @file    net/acks/ack_spec_data.c
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
#include <string.h>

#include <ack.h>


/**
 * @brief send spectral data
 *
 * @note the caller must take care to clean the spectral data supplied
 */

void ack_spec_data(uint16_t trans_id, struct spec_data *s)
{
	gsize pkt_size;
	gsize data_size;

	struct packet *pkt;


	data_size = sizeof(struct spec_data)
		    + s->n * sizeof(uint32_t);

	pkt_size = sizeof(struct packet) + data_size;

	pkt = g_malloc(pkt_size);

	pkt->service   = PR_SPEC_DATA;
	pkt->trans_id  = trans_id;
	pkt->data_size = data_size;

	memcpy(pkt->data, s, data_size);

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	g_debug("Transmitting spectral data");

	net_send((void *) pkt, pkt_size);

	/* clean up packet */
	g_free(pkt);
}
