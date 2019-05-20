/**
 * @file    server/cfg.c
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
 * @todo we don't check for errors on key file loads.
 *	 also, we do return copies of configuration items
 *
 */

#include <cfg.h>

static struct server_settings *server_cfg;


/**
 * @brief load configuration keys in the network group
 */

static void server_cfg_load_network(GKeyFile *kf, struct server_settings *s)
{
	const gchar *grp = "Network";


	s->port = g_key_file_get_integer(kf, grp, "port", NULL);
}


/**
 * @brief load configuration keys in the backend group
 */

static void server_cfg_load_backend(GKeyFile *kf, struct server_settings *s)
{
	gsize len;

	const gchar *grp = "Backend";


	s->plugins = g_key_file_get_string_list(kf, grp, "plugins", &len, NULL);
}


/**
 * @brief load configuration keys in the location group
 */

static void server_cfg_load_location(GKeyFile *kf, struct server_settings *s)
{
	gsize len_az = 0;
	gsize len_el = 0;

	const char *grp = "Location";


	s->station = g_key_file_get_string(kf, grp, "station", NULL);
	s->lat     = g_key_file_get_double(kf, grp, "lat", NULL);
	s->lon     = g_key_file_get_double(kf, grp, "lon", NULL);

	s->hor_az  = g_key_file_get_integer_list(kf, grp, "hor_az", &len_az, NULL);
	s->hor_el  = g_key_file_get_integer_list(kf, grp, "hor_el", &len_el, NULL);

	if (len_az != len_el) {
		g_warning("Horizon profile AZ/EL values do not form pairs");
		s->n_hor = 0;
		g_free(s->hor_az);
		g_free(s->hor_el);
	}

	s->n_hor = len_az;
}



/**
 * @brief get the configured server port
 */

guint16 server_cfg_get_port(void)
{
	return server_cfg->port;
}


/**
 * @brief get the NULL terminated array of configured plugin paths
 */

gchar **server_cfg_get_plugins(void)
{
	return server_cfg->plugins;
}


/**
 * @brief get station name string
 */

gchar *server_cfg_get_station(void)
{
	return server_cfg->station;
}


/**
 * @brief get station latitude
 */

double server_cfg_get_station_lat(void)
{
	return server_cfg->lat;
}


/**
 * @brief get station longitude
 */

double server_cfg_get_station_lon(void)
{
	return server_cfg->lon;
}


/**
 * @brief set station latitude
 */

void server_cfg_set_station_lat(double lat)
{
	server_cfg->lat = lat;
}


/**
 * @brief set station longitude
 */

void server_cfg_set_station_lon(double lon)
{
	server_cfg->lon = lon;
}


/**
 * @brief get station's horizon limit profile
 *
 * @returns the size of the az/el arrays pointed to by hor_az and hor_el
 */

gsize server_cfg_get_hor_limits(gint32 **hor_az, gint32 **hor_el)
{

	(*hor_az) = g_memdup(server_cfg->hor_az,
			     server_cfg->n_hor * sizeof(gint32));

	(*hor_el) = g_memdup(server_cfg->hor_el,
			     server_cfg->n_hor * sizeof(gint32));

	return server_cfg->n_hor;
}


/**
 * @brief load the server configuration file
 */

int server_cfg_load(void)
{
	gsize len;
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;

	GError *error = NULL;

	gchar *cfg;





	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	cfg = g_strconcat(CONFDIR, "server.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, &error);

	if (!ret) {
		g_error("Error loading server config file %s", error->message);
		g_clear_error(&error);
		g_free(cfg);
		return -1;
	}

	g_free(cfg);

	server_cfg = g_malloc0(sizeof(struct server_settings));

	server_cfg_load_network(kf, server_cfg);
	server_cfg_load_backend(kf, server_cfg);
	server_cfg_load_location(kf, server_cfg);

	g_key_file_free(kf);

	return 0;

}
