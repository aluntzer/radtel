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
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gmodule.h>
#include <string.h>
#include <coordinates.h>

#include <math.h>
#include <backend.h>
#include <ack.h>

#include <cfg.h>


#define MSG "RT SIM: "

#ifndef CONFDIR
#define CONFDIR "config/"
#endif

/* cant use doppler_freq() (would be non-constant init)*/
#define DOPPLER_FREQ(vel, ref)     ((ref) * (1.0 - (vel) / 299790.0))
#define DOPPLER_FREQ_REL(vel, ref) ((vel) * (       ref ) / 299790.0)


/**
 * default limits by velocity and rest frequency reference
 */

#define SIM_V_REF_MHZ	1420.406
#define SIM_V_RED_KMS	 400.0
#define SIM_V_BLU_KMS	-400.0
#define SIM_V_RES_KMS	   1.0


/* default allowed HW ranges */
#define SIM_FREQ_MIN_HZ	DOPPLER_FREQ(SIM_V_RED_KMS, SIM_V_REF_MHZ * 1e6)
#define SIM_FREQ_MAX_HZ DOPPLER_FREQ(SIM_V_BLU_KMS, SIM_V_REF_MHZ * 1e6)
#define SIM_FREQ_STP_HZ DOPPLER_FREQ_REL(SIM_V_RES_KMS, SIM_V_REF_MHZ * 1e6)
#define SIM_IF_BW_HZ	(SIM_FREQ_MAX_HZ - SIM_FREQ_MIN_HZ)
#define SIM_BINS	((SIM_V_RED_KMS - SIM_V_BLU_KMS) / SIM_V_RES_KMS)
#define SIM_BW_DIV_MAX	0




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

	struct {
		double freq_min_hz;		/* lower frequency limit */
		double freq_max_hz;		/* upper frequency limit */
		double freq_inc_hz;		/* tuning step */
		double freq_if_bw;		/* IF bandwidth */
		int freq_bw_div_max;		/* maximum bandwidth divider */
		int max_bins;			/* maximum number of spectral bins */
	} radio;

} sim = {
	.az.left  =   0.0,
	.az.right = 360.0,
	.az.res   =   0.5,
	.el.lower =   0.0,
	.el.upper =  90.0,
	.el.res   =   0.5,
	.radio.freq_min_hz      = SIM_FREQ_MIN_HZ,
	.radio.freq_max_hz      = SIM_FREQ_MAX_HZ,
	.radio.freq_inc_hz      = SIM_FREQ_STP_HZ,
	.radio.freq_if_bw       = SIM_IF_BW_HZ,
	.radio.freq_bw_div_max  = (int) SIM_BW_DIV_MAX,
	.radio.max_bins	        = (int) SIM_BINS,
};


/**
 * @brief an observation
 */

struct observation {
	struct spec_acq_cfg  acq;
	gsize  n_acs;
};

static GThread *thread;
static GCond	acq_cond;
static GMutex   acq_lock;
static GMutex   acq_pause;
static GRWLock  obs_rwlock;

static struct observation g_obs;


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
	return 0;
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


static int get_offset(double glon, double glat)
{
	/* data is in 0.5 deg steps */
	glon = round(2.0 * glon) * 0.5;
	glat = round(2.0 * glat) * 0.5;
	/* 801 velocity bins (+-400 plus zero), 181 entries per glat (we have
	 * +-90) and ??? degrees per glon */
	return 801 * ((int) ((180. + 181.0) * 2. * glon) + (int) (2.0 * (glat + 90.0)));
}
#define VEL 801





#define UNOISE (rand()/((double)RAND_MAX + 1.))

/**
 * @brief gaussian noise macro
 */
#define GNOISE (UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE - 6.)

/**
 * @brief acquire spectrea
 * @returns 0 on completion, 1 if more acquisitions are pending
 */

static uint32_t sim_spec_acquire(struct observation *obs)
{
	struct status st;


	struct spec_data *s = NULL;

	struct coord_horizontal hor;
	struct coord_galactic gal;


#if 0
	if (!obs->acq.acq_max)
		return 0;
#endif
	/* efficiency is for dopes...nahh, we'll fix this soon, just stupidly testing */
	FILE *data = fopen("/home/armin/Work/radtelsim/vel_short_int.dat", "rb");
	if (!data) {
		g_error("%s: error opening file", __func__);
		return 0;
	}

	int i;
	int offset;

	short as[VEL];

	hor.az = sim.az.cur;
	hor.el = sim.el.cur;

	gal = horizontal_to_galactic(hor, server_cfg_get_station_lat(), server_cfg_get_station_lon());
	offset = get_offset(gal.lon, gal.lat) * sizeof(short);

	fseek(data, offset, SEEK_SET);

	fread(as, sizeof(short), VEL, data);
	fclose(data);



	st.busy = 1;
	st.eta_msec = (typeof(st.eta_msec)) 20.0;	/* whatever */
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);


	/* prepare and send: allocate full length */
	s = g_malloc0(sizeof(struct spec_data) + VEL * sizeof(uint32_t));


	s->freq_min_hz = (typeof(s->freq_min_hz)) DOPPLER_FREQ((SIM_V_RED_KMS + vlsr(galactic_to_equatorial(gal), 0.0)), SIM_V_REF_MHZ * 1e6);
	s->freq_max_hz = (typeof(s->freq_max_hz)) DOPPLER_FREQ((SIM_V_BLU_KMS + vlsr(galactic_to_equatorial(gal), 0.0)), SIM_V_REF_MHZ * 1e6);
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) SIM_FREQ_STP_HZ;

        //srand(time(0));
	for (i = 0; i < VEL; i++) {
		s->spec[i]  = (uint32_t) as[i]  + 300000; /* already in mK + tsys*/
	//	s->spec[i] += (int) (GNOISE * 100.);

		s->n++;
	}

	/* handover for transmission */
	ack_spec_data(PKT_TRANS_ID_UNDEF, s);

	st.busy = 0;
	st.eta_msec = 0;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);

	obs->acq.acq_max--;

cleanup:
	g_free(s);

	g_usleep(G_USEC_PER_SEC / 4);

	return obs->acq.acq_max;
}

/**
 * @brief pause/unpause radio acquisition
 *
 */

static void sim_spec_acq_enable(gboolean mode)
{
	static gboolean last = TRUE;


	/* see if we currently hold the lock */
	if (mode == last) {

		if (mode)
			ack_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		else
			ack_spec_acq_disable(PKT_TRANS_ID_UNDEF);

		return;
	}

	last = mode;


	if (!mode) {
		g_mutex_lock(&acq_pause);
		return;
	}


	g_mutex_unlock(&acq_pause);

	/* signal the acquisition thread outer loop */
	if (g_mutex_trylock(&acq_lock)) {
		g_cond_signal(&acq_cond);
		g_mutex_unlock(&acq_lock);
	}

	return;
}



/**
 * @brief thread function that does all the spectrum readout work
 */

static gpointer sim_spec_thread(gpointer data)
{
	int run;

	while (1) {

		g_mutex_lock(&acq_lock);

		ack_spec_acq_disable(PKT_TRANS_ID_UNDEF);
		g_message(MSG "spectrum acquisition stopped");

		g_cond_wait(&acq_cond, &acq_lock);

		ack_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		g_message(MSG "spectrum acquisition running");

		do {
			if (!g_mutex_trylock(&acq_pause))
				break;

			g_mutex_unlock(&acq_pause);

			g_rw_lock_reader_lock(&obs_rwlock);
			run = sim_spec_acquire(&g_obs);
			g_rw_lock_reader_unlock(&obs_rwlock);
		} while (run);


		g_mutex_unlock(&acq_lock);
	}
}
/**
 * @brief thread function to update the acquisition information
 */

static gpointer sim_acquisition_update(gpointer data)
{
	struct observation *obs;


	obs = (struct observation *) data;


	/* wait for mutex lock to indicate abort to a single acquisition cycle
	 * this is needed if a very wide frequency span had been selected
	 */

	g_rw_lock_writer_lock(&obs_rwlock);

	memcpy(&g_obs.acq, &obs->acq, sizeof(struct spec_acq_cfg));

	g_obs.n_acs = obs->n_acs;

	g_rw_lock_writer_unlock(&obs_rwlock);

	/* signal the acquisition thread outer loop */
	if (g_mutex_trylock(&acq_lock)) {
		g_cond_signal(&acq_cond);
		g_mutex_unlock(&acq_lock);
	}

	/* push current configuration to clients */
	ack_spec_acq_cfg(PKT_TRANS_ID_UNDEF, &g_obs.acq);

	g_free(data);
}


/**
 * @brief set a default configuration
 */

static void sim_spec_cfg_defaults(void)
{
	struct observation *obs;


	obs = g_malloc(sizeof(struct observation));
	if (!obs) {
		g_error(MSG "memory allocation failed: %s: %d",
			__func__, __LINE__);
		return;
	}

	obs->acq.freq_start_hz = SIM_FREQ_MIN_HZ;
	obs->acq.freq_stop_hz  = SIM_FREQ_MAX_HZ;
	obs->acq.bw_div        = 0;
	obs->acq.bin_div       = 0;
	obs->acq.n_stack       = 0;
	obs->acq.acq_max       = ~0;

	g_thread_new(NULL, sim_acquisition_update, (gpointer) obs);
}


/**
 * @brief spectrum acquisition configuration
 */

G_MODULE_EXPORT
int be_spec_acq_cfg(struct spec_acq_cfg *acq)
{
//	if (srt_spec_acquisition_configure(acq))
//		return -1;

	return 0;
}


/**
 * @brief current spectrum acquisition configuration readout
 */

G_MODULE_EXPORT
int be_spec_acq_cfg_get(struct spec_acq_cfg *acq)
{
	if (!acq)
		return -1;

	memcpy(acq, &g_obs.acq, sizeof(struct spec_acq_cfg));

	return 0;
}


/**
 * @brief spectrum acquisition enable/disable
 */

G_MODULE_EXPORT
int be_spec_acq_enable(gboolean mode)
{
	sim_spec_acq_enable(mode);

	return 0;
}


/**
 * @brief get telescope spectrometer capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_spec(struct capabilities *c)
{
	c->freq_min_hz		= (uint64_t) sim.radio.freq_min_hz;
	c->freq_max_hz		= (uint64_t) sim.radio.freq_max_hz;
	c->freq_inc_hz		= (uint64_t) sim.radio.freq_inc_hz;
	c->bw_max_hz		= (uint32_t) sim.radio.freq_if_bw;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= (uint32_t) sim.radio.freq_bw_div_max;
	c->bw_max_bins		= (uint32_t) sim.radio.max_bins;
	c->bw_max_bin_div_lin	= 0;
	c->bw_max_bin_div_rad2	= 0;
	c->n_stack_max		= 0; /* stacking not implemented */


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
	if (thread)
		return;

	g_message(MSG "starting spectrum acquisition thread");

	thread = g_thread_new(NULL, sim_spec_thread, NULL);

	/* always start paused */
	sim_spec_acq_enable(FALSE);

	sim_spec_cfg_defaults();
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
