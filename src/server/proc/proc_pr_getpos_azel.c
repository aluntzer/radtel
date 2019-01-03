/**
 * @file    server/proc/proc_pr_getpos_azel.c
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
#include <backend.h>


void proc_pr_getpos_azel(struct packet *pkt)
{
	double az;
	double el;

	struct getpos pos;


	g_message("Client requested AZEL, acknowledging");


	if (be_getpos_azel(&az, &el)) {
		ack_fail(pkt->trans_id);
		return;
	}

	pos.az_arcsec = (typeof(pos.az_arcsec)) (az * 3600.0);
	pos.el_arcsec = (typeof(pos.el_arcsec)) (el * 3600.0);

	ack_getpos_azel(pkt->trans_id, &pos);
}
