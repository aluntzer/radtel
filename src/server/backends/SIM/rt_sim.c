/**
 * @file    server/backends/RTsim/rt_sim.c
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
 * @brief plugin emulating a radio telescope
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gmodule.h>
#include <string.h>


#include <math.h>
#include <backend.h>
#include <ack.h>


#define MSG "RT SIM: "

#define DEG(x) ((x) / G_PI * 180.0)
#define RAD(x) ((x) * G_PI / 180.0)


static struct {

	/* Azimuth movement limits of the drive. The left value is the
	 * pointing reset value at STOW */
	struct {
		double left;
		double right;
		double res;
		double cur;
	} az;

	/* Elevation movement limits of the drive. The lower value is the
	 * pointing reset value at STOW */
	struct {
		double lower;
		double upper;
		double res;
		double cur;
	} el;

} sim = {
	.az.left  =   0.0,
	.az.right = 360.0,
	.az.res   =   0.5,
	.el.lower =   0.0,
	.el.upper =  90.0,
	.el.res   =   0.5,
};



/**
 * @brief load configuration keys
 */

static void sim_load_keys(GKeyFile *kf)
{
	gsize len;


	double *tmp;

	GError *error = NULL;


	tmp = g_key_file_get_double_list(kf, "DRIVE",
					   "az_limits", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of azimuth limits configured.");

	sim.az.left  = tmp[0];
	sim.az.right = tmp[1];
	g_free(tmp);

	tmp = g_key_file_get_double_list(kf, "DRIVE",
					 "el_limits", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of elevation limits configured.");

	sim.el.lower = tmp[0];
	sim.el.upper = tmp[1];
	g_free(tmp);


	sim.az.res = g_key_file_get_double(kf, "DRIVE", "az_res", &error);
	if (error)
		g_error(error->message);

	sim.el.res = g_key_file_get_double(kf, "DRIVE", "el_res", &error);
	if (error)
		g_error(error->message);
}





/**
 * @brief load the configuration file
 */

static int sim_load_config(void)
{
	gsize len;
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;

	GError *error = NULL;

	gchar *cfg;


	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;


	cfg = g_strconcat(CONFDIR, "backends/rt_sim.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, &error);
	g_free(cfg);

	if (!ret) {
		g_error(MSG "error loading config file %s", error->message);
		g_clear_error(&error);
		return -1;
	}

	sim_load_keys(kf);

	g_key_file_free(kf);

	return 0;
}



/**
 * @brief check if coordinates are within limits
 *
 * @returns -1 on error, 0 otherwise
 */

static int sim_drive_check_limits(double az, double el)
{
	g_message(MSG "check if AZ/EL %g/%g is within limits", az, el);

	if (az > sim.az.right) {
		g_warning(MSG "error, az %g > %g", az, sim.az.right);
		return -1;
	}

	if (az < sim.az.left) {
		g_warning(MSG "error, az %g < %g", az, sim.az.left);
		return -1;
	}

	if (el < sim.el.lower) {
		g_warning(MSG "error, el %g < %g", el, sim.el.lower);
		return -1;
	}

	if (el > sim.el.upper) {
		g_warning(MSG "error, el %g < %g", el, sim.el.upper);
		return -1;
	}

	return 0;
}


/**
 * @brief set pointing
 *
 * @returns -1 on error, 0 otherwise
 */

static int sim_drive_moveto(double az, double el)
{
	struct getpos pos;


	ack_moveto_azel(PKT_TRANS_ID_UNDEF, az, el);

	if (sim_drive_check_limits(az, el))
		return -1;

	g_debug(MSG "rotating to AZ/EL %g/%g", az, el);

	sim.az.cur = az;
	sim.el.cur = el;

	/* emit notification */
	pos.az_arcsec = (typeof(pos.az_arcsec)) (3600.0 * sim.az.cur);
	pos.el_arcsec = (typeof(pos.el_arcsec)) (3600.0 * sim.el.cur);
	ack_getpos_azel(PKT_TRANS_ID_UNDEF, &pos);


	return 0;
}

/**
 * @brief move to parking position
 */

G_MODULE_EXPORT
void be_park_telescope(void)
{
	g_message(MSG "parking telescope");

	sim.az.cur = sim.az.left;
	sim.el.cur = sim.el.lower;
}


/**
 * @brief recalibrate pointing
 */

G_MODULE_EXPORT
void be_recalibrate_pointing(void)
{
	g_warning(MSG "recalibrating pointing");
}


/**
 * @brief move the telescope to a certain azimuth and elevation
 */

G_MODULE_EXPORT
int be_moveto_azel(double az, double el)
{
	if (sim_drive_moveto(az, el)) {
		g_warning(MSG "invalid coordinates AZ/EL %g/%g", az, el);
		return -1;
	}

	return 0;
}


/**
 * @brief get telescope azimuth and elevation
 */

G_MODULE_EXPORT
int be_getpos_azel(double *az, double *el)
{
	(*az) = sim.az.cur;
	(*el) = sim.el.cur;

	return 0;
}


/**
 * @brief get telescope drive capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_drive(struct capabilities *c)
{
	c->az_min_arcsec = (int32_t) (3600.0 * sim.az.left);
	c->az_max_arcsec = (int32_t) (3600.0 * sim.az.right);
	c->az_res_arcsec = (int32_t) (3600.0 * sim.az.res);

	c->el_min_arcsec = (int32_t) (3600.0 * sim.el.lower);
	c->el_max_arcsec = (int32_t) (3600.0 * sim.el.upper);
	c->el_res_arcsec = (int32_t) (3600.0 * sim.el.res);

	return 0;
}


/**
 * @brief extra initialisation function
 *
 * @note if a thread is created in g_module_check_init(), the loader appears
 *       to fail, so we do that here
 *
 * @todo figure out if this is an error on our part
 */

G_MODULE_EXPORT
void module_extra_init(void)
{
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
const gchar *g_module_check_init(void)
{

        g_message(MSG "initialising module");

	if (sim_load_config())
		g_warning(MSG "Error loading module configuration, "
			      "this plugin may not function properly.");

	return NULL;
}
