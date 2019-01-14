/**
 * @file    server/proc/proc_pr_capabilities.c
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
#include <cfg.h>

void proc_pr_capabilities(struct packet *pkt)
{
	struct capabilities c = {0};

	g_message("Client requested capabilites, acknowledging");

	be_get_capabilities_spec(&c);
	be_get_capabilities_drive(&c);

	c.lon_arcsec = (int32_t) (3600.0 * server_cfg_get_station_lon());
	c.lat_arcsec = (int32_t) (3600.0 * server_cfg_get_station_lat());

	ack_capabilities(pkt->trans_id, &c);
}
