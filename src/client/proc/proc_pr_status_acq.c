/**
 * @file    client/proc/proc_pr_status_acq.c
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



void proc_pr_status_acq(struct packet *pkt)
{
	const struct status *s;


	g_debug("Server sent status_acq");

	if (pkt->data_size != sizeof(struct status)) {
		g_message("\tstatus payload size mismatch %d != %d",
			  sizeof(struct status), pkt->data_size);
		return;
	}

	s = (const struct status *) pkt->data;


	sig_pr_status_acq(s);
}
