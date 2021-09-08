/**
 * @file    client/proc/proc_pr_capabilities_load.c
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

#include <protocol.h>
#include <signals.h>



void proc_pr_capabilities_load(struct packet *pkt)
{
	gsize pkt_data_size;
	const struct capabilities_load *c;


	g_debug("Server sent capabilities_load");

	c = (const struct capabilities_load *) pkt->data;

	pkt_data_size = sizeof(struct capabilities_load)
			+ c->n_hor * sizeof(struct local_horizon);

	if (pkt->data_size != pkt_data_size) {
		g_message("\tcapabilities_load payload size mismatch %d != %d",
			  pkt_data_size, pkt->data_size);
		return;
	}


	sig_pr_capabilities_load(c);
}
