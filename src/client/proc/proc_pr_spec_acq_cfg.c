/**
 * @file    client/proc/proc_pr_spec_acq_cfg.c
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



void proc_pr_spec_acq_cfg(struct packet *pkt)
{
	const struct spec_acq_cfg *acq;


	g_debug("Server sent spectrometer acquisition configuration");

	if (pkt->data_size != sizeof(struct spec_acq_cfg)) {
		g_message("\tacquisition configuration payload size mismatch "
			  "%d != %d",
			  sizeof(struct spec_acq_cfg), pkt->data_size);
		return;
	}

	acq = (const struct spec_acq_cfg *) pkt->data;


	sig_pr_spec_acq_cfg(acq);
}
