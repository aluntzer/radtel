/**
 * @file    server/include/cfg.h
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

#ifndef _SERVER_INCLUDE_CFG_H_
#define _SERVER_INCLUDE_CFG_H_

#include <glib.h>

struct server_settings {
	guint16  port;		/* network port */
	gchar  **plugins;	/* plugin paths */
	gchar   *station;	/* station name */
	double   lat;		/* station latitude */
	double   lon;		/* station longitude */
	int     *hor_az;	/* horizon profile azimuth values */
	int     *hor_el;	/* horizon profile elevaltion values */
	gsize    n_hor;		/* number of profile values */
};


guint16 server_cfg_get_port(void);
gchar **server_cfg_get_plugins(void);
gchar *server_cfg_get_station(void);
double server_cfg_get_station_lon(void);
double server_cfg_get_station_lon(void);
gsize server_cfg_get_hor_limits(int **hor_az, int **hor_el);

int server_cfg_load(void);



#endif /* _SERVER_INCLUDE_CFG_H_ */
