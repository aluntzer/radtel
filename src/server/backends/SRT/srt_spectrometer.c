/**
 * @file    server/backends/SRT/srt_spectrometer.c
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
 * @brief plugin for the SRT antenna rotator drive
 *
 * @note this plugin supports only the CASSI mount
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gmodule.h>
#include <string.h>


#include <math.h>
#include <backend.h>


#define MSG "SRT SPEC: "


static GThread *thread;
static GCond cond;
static GMutex mutex;


#define SRT_DIGITAL_FREQ_MIN 1370000000.0
#define SRT_DIGITAL_FREQ_MAX 1800000000.0


/* length units in inches */
static struct {

	double freq_min_hz;			/* lower frequency limit */
	double freq_max_hz;			/* upper frequency limit */

	double freq_inc_hz;	/* PLL tuning step */

} srt = {.freq_inc_hz = 40000.0};




/**
 * @brief load configuration keys
 */

static void srt_spec_load_keys(GKeyFile *kf)
{
	gsize len;

	gchar *model;

	double tmp;

	GError *error = NULL;




	model = g_key_file_get_string(kf, "Radio", "model", &error);

	if (error)
		g_error(error->message);

	if (g_strcmp0(model, "Digital"))
		g_error(MSG "SRT radios other than digital are not supported.");


	srt.freq_min_hz = g_key_file_get_double(kf, model,
					"freq_min_hz", &error);
	if (error)
		g_error(error->message);


	tmp = fmod(srt.freq_min_hz, srt.freq_inc_hz);
	if (tmp != 0.0) {
		srt.freq_min_hz -= tmp;
		g_message(MSG "adjusted lower frequency limit to be a multiple of 40 kHz: %g MHz",
				srt.freq_min_hz / 1e6);
	}


	if (srt.freq_min_hz < SRT_DIGITAL_FREQ_MIN) {
		srt.freq_min_hz = SRT_DIGITAL_FREQ_MIN;
		g_message(MSG "adjusted lower frequency limit to %g MHz",
			  srt.freq_min_hz / 1e6);
	}



	srt.freq_max_hz = g_key_file_get_double(kf, model,
					"freq_max_hz", &error);
	if (error)
		g_error(error->message);

	tmp = fmod(srt.freq_max_hz, srt.freq_inc_hz);
	if (tmp != 0.0) {
		srt.freq_max_hz -= tmp;
		g_message(MSG "adjusted upper frequency limit to be a multiple of 40 kHz: %g MHz",
				srt.freq_max_hz / 1e6);
	}


	if (srt.freq_max_hz > SRT_DIGITAL_FREQ_MAX) {
		srt.freq_max_hz = SRT_DIGITAL_FREQ_MAX;
		g_message(MSG "adjusted upper frequency limit to %g MHz",
			  srt.freq_max_hz / 1e6);
	}



	g_free(model);
}


/**
 * @brief load the configuration file
 */

static int srt_drive_load_config(void)
{
	gsize len;
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;

	GError *error = NULL;



	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;


	ret = g_key_file_load_from_file(kf, "config/backends/srt_spectrometer.cfg",
					flags, &error);

	if (!ret) {
		g_error(MSG "error loading config file %s", error->message);
		g_clear_error(&error);
		return -1;
	}

	srt_spec_load_keys(kf);

	g_key_file_free(kf);

	return 0;
}

/**
 * @brief thread function that does all the spectrum readout work
 */

static gpointer srt_spec_thread(gpointer data)
{
	gsize len;
	gchar *response;
	gchar *cmd;
	int refdiv;

	while (1) {
		be_shared_comlink_acquire();

		refdiv = 1420.4 * ( 1.0 / 0.04 ) + 0.5;

		g_message(MSG "refdiv: %x", refdiv);
		cmd = g_strdup_printf("%cfreq%c%c%c%c\n",0,
				      ( refdiv >> 14 ) & 0xff,
				      ( refdiv >>  6 ) & 0xff,
				      ( refdiv       ) & 0x3f,
				      0);

		//g_message(MSG "CMD: %s", &cmd[1]);
		/* give size explicitly, as command starts with '\0' */
		be_shared_comlink_write(cmd, 10);

		response = be_shared_comlink_read(&len);

		be_shared_comlink_release();

		g_message(MSG "response: (%d)", len);
		g_free(response);
	}

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
	if (thread)
		return;

	g_message(MSG "starting spectrum acquisition thread");

	thread = g_thread_new(NULL, srt_spec_thread, NULL);
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
const gchar *g_module_check_init(void)
{

        g_message(MSG "initialising module");

	if (srt_drive_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");

	return NULL;
}
