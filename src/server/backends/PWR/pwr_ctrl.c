/**
 * @file    server/backends/PWR/pwr_ctrl.c
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
 * @brief plugin for commandline controlled (networked) power switches
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <glib.h>
#include <gmodule.h>
#include <string.h>

#include <math.h>
#include <backend.h>
#include <ack.h>

#include <net.h>


#define MSG "PWR CTRL: "


static gchar *drive_pwr_cmd;
static gchar *drive_off_cmd;
static int drive_to_delay;
static int drive_to_max;
static int drive_to_cur;
static int drive_has_usr;


/**
 * @brief load configuration keys
 */

static void pwr_ctrl_load_keys(GKeyFile *kf)
{
	GError *error = NULL;

	if (g_key_file_has_key(kf, "DRIVE", "pwr_cmd", &error)) {

		drive_pwr_cmd = g_key_file_get_string(kf, "DRIVE", "pwr_cmd", &error);
		if (error)
			goto error;

	}
	if (error)
		goto error;

	if (g_key_file_has_key(kf, "DRIVE", "off_cmd", &error)) {

		drive_off_cmd = g_key_file_get_string(kf, "DRIVE", "off_cmd", &error);
		if (error)
			goto error;
	}
	if (error)
		goto error;

	if (g_key_file_has_key(kf, "DRIVE", "to_delay", &error)) {

		drive_to_delay = g_key_file_get_integer(kf, "DRIVE", "to_delay", &error);
		if (error)
			goto error;
	}
	if (error)
		goto error;

	if (g_key_file_has_key(kf, "DRIVE", "to_max", &error)) {

		drive_to_max = g_key_file_get_integer(kf, "DRIVE", "to_max", &error);
		if (error)
			goto error;
	}
	if (error)
		goto error;



	return;

error:
	if (error) {
		g_error(error->message);
		g_clear_error(&error);
	}
}


/**
 * @brief load the pwr_ctrl configuration file from a given prefix
 *
 * @returns 0 on success, otherwise error
 */

static int pwr_ctrl_load_config_from_prefix(const gchar *prefix, GError **err)
{
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;


	gchar *cfg;


	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	cfg = g_strconcat(prefix, "backends/pwr_ctrl.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, err);


	if (!ret) {
		g_key_file_free(kf);
		g_free(cfg);
		return -1;
	}

	g_message(MSG "Configuration file loaded from %s", cfg);

	pwr_ctrl_load_keys(kf);

	g_key_file_free(kf);
	g_free(cfg);

	return 0;
}


/**
 * @brief try to load a pwr_ctrl configuration file from various paths
 */

int pwr_ctrl_load_config(void)
{
	int ret;

	gchar *prefix;

	GError *error = NULL;



	/* search relative path first */
	ret = pwr_ctrl_load_config_from_prefix("/", &error);
	if (ret) {
		g_clear_error(&error);
		/* try again in sysconfdir */
		prefix = g_strconcat(SYSCONFDIR, "/", CONFDIR, "/", NULL);
		ret = pwr_ctrl_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_warning(MSG "Could not find backends/pwr_ctrl.cfg: %s. "
			  "Looked in ./, %s and %s/%s",
			  error->message, CONFDIR, SYSCONFDIR, CONFDIR);
		g_clear_error(&error);

		return -1;
	}

	return 0;
}


static gboolean drive_pwr_cycle_cb(gpointer data)
{
	g_message(MSG "cycle drive on!");

	if (drive_pwr_cmd)
		g_spawn_command_line_sync(drive_pwr_cmd, NULL, NULL, NULL, NULL);

	return G_SOURCE_REMOVE;
}


static gboolean drive_pwr_ctrl_cb(gpointer data)
{
	if (!drive_to_cur)
		return G_SOURCE_CONTINUE;

	if (drive_has_usr)
		return G_SOURCE_CONTINUE;

	drive_to_cur--;
	if (!drive_to_cur) {
		if (drive_off_cmd) {
			g_spawn_command_line_sync(drive_off_cmd, NULL, NULL, NULL, NULL);
		}
	}	


        return G_SOURCE_CONTINUE;
}


/**
 * @brief drive power enable/disable
 */

G_MODULE_EXPORT
int be_drive_pwr_ctrl(gboolean mode)
{
	if (mode) {

		g_message(MSG "POWER ON!");

		/* disable countdown to poweroff */

		drive_has_usr = 1;
		if (drive_pwr_cmd)
			g_spawn_command_line_sync(drive_pwr_cmd, NULL, NULL, NULL, NULL);


	} else {
		g_message(MSG "POWER OFF!");

		drive_has_usr = 0;

		drive_to_cur += drive_to_delay;
		if (drive_to_cur > drive_to_max)
			drive_to_cur = drive_to_max;
	}

	return 0;
}

/**
 * @brief drive power cycle
 */

G_MODULE_EXPORT
void be_drive_pwr_cycle(void)
{
	g_message(MSG "cycle drive off!");
	if (drive_off_cmd)
		g_spawn_command_line_sync(drive_off_cmd, NULL, NULL, NULL, NULL);

	/* return power after 5 seconds */
	g_timeout_add_seconds(5, drive_pwr_cycle_cb, NULL);
}


/**
 * @brief drive power status
 */

G_MODULE_EXPORT
gboolean be_drive_pwr_status(void)
{
	if (drive_to_cur || drive_has_usr)
		return TRUE;

	return FALSE;
}


/**
 * @brief extra initialisation function
 */

G_MODULE_EXPORT
void module_extra_init(void)
{
	g_message(MSG "configuring power controls");

	/* ceck timeouts once a second */
	g_timeout_add_seconds(1, drive_pwr_ctrl_cb, NULL);
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
const gchar *g_module_check_init(void)
{

        g_message(MSG "initialising module");

	if (pwr_ctrl_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");


	return NULL;
}
