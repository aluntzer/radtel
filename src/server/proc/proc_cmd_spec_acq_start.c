/**
 * @file    server/proc/proc_cmd_spec_acq_start.c
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

void proc_cmd_spec_acq_start(struct packet *pkt)
{
	gsize pkt_size;

	struct spec_acq *acq;


	g_message("Client requested start spectrum acquisition");

	if (pkt->data_size != sizeof(struct spec_acq)) {
		cmd_invalid_pkt();
		return;
	}


	acq = (struct spec_acq *) pkt->data;

	
	if(be_spec_acq_start(acq))
		cmd_fail();

	cmd_success();
}
