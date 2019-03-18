/**
 * @file    client/proc/proc_pr_moveto_azel.c
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


/**
 * @brief process ack moveto AZEL
 */

void proc_pr_moveto_azel(struct packet *pkt)
{
	double az;
	double el;


	struct moveto *m;


	g_debug("Server sent ACK moveto AZEL");

	if (pkt->data_size != sizeof(struct moveto)) {
		g_message("\tmoveto payload size mismatch %d != %d",
			  sizeof(struct moveto), pkt->data_size);
		return;
	}


	m = (struct moveto *) pkt->data;

	/* convert arcseconds to degrees */
	az = (double) m->az_arcsec / 3600.0;
	el = (double) m->el_arcsec / 3600.0;

	sig_pr_moveto_azel(az, el);

	return;
}
