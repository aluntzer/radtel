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

	case CMD_INVALID_PKT:
		proc_cmd_invalid_pkt();
		break;

	case CMD_CAPABILITIES:
		proc_cmd_capabilities(pkt);
		break;

	case CMD_SUCCESS:
		proc_cmd_success();
		break;

	case CMD_FAIL:
		proc_cmd_fail();
		break;

	case CMD_SPEC_DATA:
		proc_cmd_spec_data(pkt);
		break;

	case CMD_GETPOS_AZEL:
		proc_cmd_getpos_azel(pkt);
		break;

	default:
		g_message("Service command %x not understood\n", pkt->service);
		break;
	}


	g_free(pkt);
}
