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
	guint32 i;
	guint32 n;

	gint32 *az;
	gint32 *el;

	struct capabilities *c;



	g_debug("Client requested capabilites, acknowledging");

	n = server_cfg_get_hor_limits(&az, &el);

	c = g_malloc0(sizeof(struct capabilities)
		      + n * sizeof(struct local_horizon));

	c->n_hor = n;

	for (i = 0; i < n; i++) {
		c->hor[i].az = az[i];
		c->hor[i].el = el[i];
	}

	g_free(az);
	g_free(el);


	be_get_capabilities_spec(c);
	be_get_capabilities_drive(c);

	c->lon_arcsec = (int32_t) (3600.0 * server_cfg_get_station_lon());
	c->lat_arcsec = (int32_t) (3600.0 * server_cfg_get_station_lat());

	ack_capabilities(pkt->trans_id, c);

	g_free(c);
}
