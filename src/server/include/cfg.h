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
	gchar   **plugins;	/* plugin paths */
	gchar    *station;	/* station name */
	gdouble   lat;		/* station latitude */
	gdouble   lon;		/* station longitude */
	gint32   *hor_az;	/* horizon profile azimuth values */
	gint32   *hor_el;	/* horizon profile elevaltion values */
	gsize     n_hor;	/* number of profile values */
	gchar    *motd;		/* a message of the day */
	gchar    *masterkey;	/* guess */
	gchar    *video_uri;	/* webcam URI */
	gboolean ctrl_enable;	/* enable automatic assignment of control privileges */
};


guint16 server_cfg_get_port(void);
gchar **server_cfg_get_plugins(void);
gchar *server_cfg_get_station(void);
double server_cfg_get_station_lon(void);
double server_cfg_get_station_lat(void);
gchar *server_cfg_get_video_uri(void);

void server_cfg_set_station_lat(double lat);
void server_cfg_set_station_lon(double lon);

gsize server_cfg_get_hor_limits(int **hor_az, int **hor_el);

int server_cfg_load(void);

gchar *server_cfg_get_motd(void);
void server_cfg_set_motd(const gchar *motd);
void server_cfg_set_video_uri(const gchar *video_uri);

const gchar *server_cfg_get_masterkey(void);

gboolean server_cfg_get_auto_ctrl_enable(void);



#endif /* _SERVER_INCLUDE_CFG_H_ */

