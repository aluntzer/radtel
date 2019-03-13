/**
 * @file    net/cmds/cmd_moveto_azel.c
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


void cmd_moveto_azel(uint16_t trans_id, double az, double el)
{
	gsize pkt_size;

	struct packet *pkt;

	struct moveto *m;


	pkt_size = sizeof(struct packet) + sizeof(struct moveto);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_MOVETO_AZEL;
	pkt->trans_id  = trans_id;
	pkt->data_size = sizeof(struct moveto);


	m = (struct moveto *) pkt->data;

	/* convert to integer arcseconds */
	m->az_arcsec = (int32_t) (az * 3600.0);
	m->el_arcsec = (int32_t) (el * 3600.0);


	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	g_debug("Sending command moveto AZ/EL %g/%g", az, el);
	net_send((void *) pkt, pkt_size);

	/* clean up */
	g_free(pkt);
}

