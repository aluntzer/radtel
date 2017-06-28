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

#include <net.h>
#include <ack.h>

	

/**
 * @brief acknowledge backend capabilities command
 */

void ack_capabilities(void)
{
	gsize pkt_size;

	struct packet *pkt;

	struct capabilities *c;


	pkt_size = sizeof(struct packet) + sizeof(struct capabilities);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);
	
	pkt->service   = CMD_CAPABILITIES;
	pkt->data_size = sizeof(struct capabilities);


	c = (struct capabilities *) pkt->data;


	/* now fill capabilites somehow (would call backend plugin here) */
	c->freq_min_hz		= 137000000000;
	c->freq_max_hz		= 180000000000;
	c->freq_inc_hz		= 400000;
	c->bw_max_hz		= 5000000;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= 2;
	c->bw_max_bins		= 64;
	c->bw_max_bin_div_lin	= 0;
	c->bw_max_bin_div_rad2	= 0;



	pkt_set_data_crc16(pkt);
	
	pkt_hdr_to_net_order(pkt);
	
	g_message("Sending capabilities");
	net_send((void *) pkt, pkt_size); 

	/* clean up */	
	g_free(pkt);
}
