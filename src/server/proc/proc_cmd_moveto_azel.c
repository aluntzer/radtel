/**
 * @file    server/proc/proc_cmd_moveto_azel.c
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
#include <backend.h>


/**
 * @brief process command moveto AZEL
 */

void proc_cmd_moveto_azel(struct packet *pkt)
{
	double az;
	double el;

	gsize pkt_size;

	struct moveto *m;


	g_message("Client requested moveto AZEL");

	if (pkt->data_size != sizeof(struct moveto)) {
		cmd_invalid_pkt();
		return;
	}


	m = (struct moveto *) pkt->data;

	/* convert arcseconds to degrees */
	az = (double) m->az_arcsec / 3600.0;
	el = (double) m->el_arcsec / 3600.0;

	
	if(be_moveto_azel(az, el))
		cmd_fail();

	cmd_success();
}
