/**
 * @file    client/cmd_proc.c
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
 * @brief server command processing
 *
 */

#include <cmd_proc.h>
#include <protocol.h>

#include <gio/gio.h>
#include <glib.h>






/**
 * @brief process a command pkt
 */

void process_cmd_pkt(struct packet *pkt)
{
	switch(pkt->service) {

	case PR_INVALID_PKT:
		proc_cmd_invalid_pkt();
		break;

	case PR_CAPABILITIES:
		proc_cmd_capabilities(pkt);
		break;

	case PR_SUCCESS:
		proc_cmd_success();
		break;

	case PR_FAIL:
		proc_cmd_fail();
		break;

	case PR_SPEC_DATA:
		proc_cmd_spec_data(pkt);
		break;

	case PR_GETPOS_AZEL:
		proc_cmd_getpos_azel(pkt);
		break;

	case PR_SPEC_ACQ_ENABLE:
		proc_cmd_spec_acq_enable();
		break;

	case PR_SPEC_ACQ_DISABLE:
		proc_cmd_spec_acq_disable();
		break;

	default:
		g_message("Service command %x not understood\n", pkt->service);
		break;
	}


	g_free(pkt);
}
