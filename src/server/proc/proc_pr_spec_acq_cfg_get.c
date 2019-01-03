/**
 * @file    server/proc/proc_pr_spec_acq_cfg_get.c
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


void proc_pr_spec_acq_cfg_get(struct packet *pkt)
{
	struct spec_acq_cfg acq;


	g_message("Client requested spectrometer configuration, acknowledging");

	if (be_spec_acq_cfg_get(&acq)) {
		ack_fail(pkt->trans_id);
		return;
	}

	ack_spec_acq_cfg(pkt->trans_id, &acq);
}
