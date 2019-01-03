/**
 * @file    server/pkt_proc.c
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
 * @brief client command processing
 *
 */

#include <pkt_proc.h>
#include <protocol.h>

#include <gio/gio.h>
#include <glib.h>






/**
 * @brief process a command pkt
 */

void process_pkt(struct packet *pkt)
{
	switch(pkt->service) {

	case PR_INVALID_PKT:
		proc_pr_invalid_pkt();
		break;

	case PR_CAPABILITIES:
		proc_pr_capabilities();
		break;

	case PR_MOVETO_AZEL:
		proc_pr_moveto_azel(pkt);
		break;

	case PR_RECAL_POINTING:
		proc_pr_recalibrate_pointing();
		break;

	case PR_PARK_TELESCOPE:
		proc_pr_park_telescope();
		break;

	case PR_SPEC_ACQ_CFG:
		proc_pr_spec_acq_cfg(pkt);
		break;

	case PR_SPEC_ACQ_ENABLE:
		proc_pr_spec_acq_enable();
		break;

	case PR_SPEC_ACQ_DISABLE:
		proc_pr_spec_acq_disable();
		break;

	case PR_GETPOS_AZEL:
		proc_pr_getpos_azel();
		break;

	default:
		g_message("Service command %x not understood\n");
		break;
	}


	g_free(pkt);
}
