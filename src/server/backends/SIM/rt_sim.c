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
#include <fourier_transform.h>

#include <pkt_proc.h>

#include <math.h>
#include <complex.h>
#include <backend.h>
#include <ack.h>

#include <cfg.h>

#include <gtk/gtk.h>


#define MSG "RT SIM: "

#ifndef CONFDIR
#define CONFDIR "config/"
#endif

#ifndef SYSCONFDIR
#define SYSCONFDIR "config/"
#endif


/* cant use doppler_freq() (would be non-constant init)*/
#define DOPPLER_FREQ(vel, ref)     ((ref) * (1.0 - (vel) / 299790.0))
#define DOPPLER_FREQ_REL(vel, ref) ((vel) * (       ref ) / 299790.0)


#define DOPPLER_VEL(freq, ref)     (((freq) / (ref) - 1.0)* 299790.0)


/**
 * default limits by velocity and rest frequency reference
 */

#define SIM_V_REF_MHZ	1420.406
#define SIM_V_RED_KMS	 400.0
#define SIM_V_BLU_KMS	-400.0
#define SIM_V_RES_KMS	   1.0
#define SIM_HI_BINS	 801
#define SIM_HI_AMP_CAL	  10.0	/* conversion from cK to mK */

#define SIM_V_REF_HZ	(SIM_V_REF_MHZ * 1e6)


/* default allowed HW ranges */
#define SIM_FREQ_MIN_HZ	DOPPLER_FREQ(SIM_V_RED_KMS, SIM_V_REF_HZ)
#define SIM_FREQ_MAX_HZ DOPPLER_FREQ(SIM_V_BLU_KMS, SIM_V_REF_HZ)
#define SIM_FREQ_STP_HZ DOPPLER_FREQ_REL(SIM_V_RES_KMS, SIM_V_REF_HZ)

#define SIM_IF_BW_HZ	(SIM_FREQ_MAX_HZ - SIM_FREQ_MIN_HZ)
//#define SIM_BINS	(SIM_IF_BW_HZ / SIM_V_RES_KMS)
#define SIM_BINS ((SIM_V_RED_KMS - SIM_V_BLU_KMS) / SIM_V_RES_KMS)

#define SIM_BW_DIV_MAX	0

/* other properties */

#define SIM_BEAM_RADIUS		0.25	/* default beam radius */

#define SIM_TSYS		100.	/* default system temperature */
#define SIM_EFF			0.6	/* default efficiency */
#define SIM_SIG_NOISE		12.	/* default noise */
#define SIM_READOUT_HZ		1	/* default readout frequency */
#define SIM_SUN_SFU		48.	/* Sun @1415 as of Jun 11 2019 12 UTC */
#define SIM_HOT_LOAD_TEMP	290.	/* default hot load temperature */
#define SIM_NOISE_FIG		0.1	/* default amplifier noise figure */


#define SKY_GAUSS_INTG_STP	0.10	/* integration step for gaussian */
#define SKY_BASE_RES		0.5	/* resolution of the underlying data */

/* +2 and +1 extra for 0/360 lon and 0 deg lat */
#define SKY_WIDTH		((gint) ceil(360.0 / SKY_BASE_RES) + 2)
#define SKY_HEIGHT		((gint) ceil(180.0 / SKY_BASE_RES) + 1)

#define SKY_SIG_TO_KELVIN	10.0	/* data to Kelvin conversion */

/* waterfall colours */
#define RTSIM_R_LO		0
#define RTSIM_G_LO		0
#define RTSIM_B_LO		0

#define RTSIM_R_MID		255
#define RTSIM_G_MID		0
#define RTSIM_B_MID		0

#define RTSIM_R_HI		255
#define RTSIM_G_HI		255
#define RTSIM_B_H		0



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

	gdouble r_beam;				/* the beam radius */
	gdouble tsys;				/* system temperature */
	gdouble eff;				/* system efficiency (eta) */
	gdouble sig_n;				/* noise sigma */
	gdouble readout_hz;			/* spectrum readout frequency */
	gdouble sun_sfu;			/* sun solar flux units */
	gdouble hot_load_temp;			/* hot load temperature */

	gboolean hot_load_ena;			/* hot load enable status */

	gdouble noise_fig;			/* noise figure of the amplifier chain */
	gdouble sig_rms;			/* theoretical rms noise  */

	struct {
		GdkPixbuf	*pb_sky;
		GtkDrawingArea	*da_sky;

		GtkScale	*s_lo;		/* lo colour thresh slider */
		GtkScale	*s_hi;		/* hi colour thresh slider */
		GtkScale	*s_min;		/* min cutoff slider */

		GtkSpinButton	*sb_vmin;
		GtkSpinButton	*sb_vmax;

		GtkSpinButton	*sb_beam;

		gdouble		th_lo;		/* lo colour threshold */
		gdouble		th_hi;		/* hi colour threshold */
		gdouble		min;		/* min cutoff */

		enum {LIN, LOG, EXP, SQRT, SQUARE, ASINH, SINH} scale;

	} gui;

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
	.r_beam			= SIM_BEAM_RADIUS,
	.tsys			= SIM_TSYS,
	.eff			= SIM_EFF,
	.sig_n			= SIM_SIG_NOISE,
	.readout_hz		= SIM_READOUT_HZ,
	.sun_sfu		= SIM_SUN_SFU,
	.hot_load_temp		= SIM_HOT_LOAD_TEMP,
	.hot_load_ena		= FALSE,
	.noise_fig		= SIM_NOISE_FIG,

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
 * @brief load the sim configuration file from a given prefix
 *
 * @returns 0 on success, otherwise error
 */

static int sim_load_config_from_prefix(const gchar *prefix, GError **err)
{
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;


	gchar *cfg;


	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	cfg = g_strconcat(prefix, "backends/rt_sim.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, err);

	if (!ret) {
		g_key_file_free(kf);
		g_free(cfg);
		return -1;
	}

	g_message(MSG "Configuration file loaded from %s", cfg);

	sim_load_keys(kf);

	g_key_file_free(kf);
	g_free(cfg);

	return 0;
}


/**
 * @brief try to load a sim configuration file from various paths
 */

int sim_load_config(void)
{
	int ret;

	gchar *prefix;

	GError *error = NULL;



	/* search relative path first */
	ret = sim_load_config_from_prefix("./", &error);

	if (ret) {
		g_clear_error(&error);
		/* try again in confdir */
		prefix = g_strconcat(CONFDIR, "/", NULL);
		ret = sim_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_clear_error(&error);
		/* try again in confdir */
		prefix = g_strconcat("etc/", CONFDIR, "/", NULL);
		ret = sim_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_clear_error(&error);
		/* try again in sysconfdir */
		prefix = g_strconcat(SYSCONFDIR, "/", CONFDIR, "/", NULL);
		ret = sim_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_warning(MSG "Could not find backends/rt_sim.cfg: %s. "
			  "Looked in ./, %s and %s/%s",
			  error->message, CONFDIR, SYSCONFDIR, CONFDIR);
		g_clear_error(&error);

		return -1;
	}

	return 0;
}



/**
 * @brief check if coordinates are within limits
 *
 * @returns -1 on error, 0 otherwise
 */

static int sim_drive_check_limits(double az, double el)
{
	return 0; /** XXX **/
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
 * @brief get an rgb colour mapping for a value
 *
 * @note the is scheme is stolen from somewhere, but I don't remember... d'oh!
 */

static void sim_get_rgb(gdouble val, gdouble thr_lo, gdouble thr_hi,
				  guchar *r, guchar *g, guchar *b)
{
	gdouble R, G, B;

	gdouble f;


	if (val < thr_lo) {
		(*r) = RTSIM_R_LO;
		(*g) = RTSIM_G_LO;
		(*b) = RTSIM_G_LO;
		return;
	}

	if (val > thr_hi) {
		(*r) = RTSIM_R_HI;
		(*g) = RTSIM_G_HI;
		(*b) = RTSIM_G_HI;
		return;
	}

	f = (val - thr_lo) / (thr_hi - thr_lo);

	if (f < 2.0/9.0) {

		f = f / (2.0 / 9.0);

		R = (1.0 - f) * (gdouble) RTSIM_R_LO;
		G = (1.0 - f) * (gdouble) RTSIM_G_LO;
		B = RTSIM_B_LO + f * (gdouble) (255 - RTSIM_B_LO);


	} else if (f < (3.0 / 9.0)) {

		f = (f - 2.0 / 9.0 ) / (1.0 / 9.0);

		R = 0.0;
		G = 255.0 * f;
		B = 255.0;

	} else if (f < (4.0 / 9.0) ) {

		f = (f - 3.0 / 9.0) / (1.0 / 9.0);

		R = 0.0;
		G = 255.0;
		B = 255.0 * (1.0 - f);

	} else if (f < (5.0 / 9.0)) {

		f = (f - 4.0 / 9.0 ) / (1.0 / 9.0);

		R = 255.0 * f;
		G = 255.0;
		B = 0.0;

	} else if ( f < (7.0 / 9.0)) {

		f = (f - 5.0 / 9.0 ) / (2.0 / 9.0);

		R = 255.0;
		G = 255.0 * (1.0 - f);
		B = 0.0;

	} else if( f < (8.0 / 9.0)) {

		f = (f - 7.0 / 9.0 ) / (1.0 / 9.0);

		R = 255.0;
		G = 0.0;
		B = 255.0 * f;

	} else {

		f = (f - 8.0 / 9.0 ) / (1.0 / 9.0);

		R = 255.0 * (0.75 + 0.25 * (1.0 - f));
		G = 0.5 * 255.0 * f;
		B = 255.0;
	}

	(*r) = (guchar) R;
	(*g) = (guchar) G;
	(*b) = (guchar) B;
}


/**
 * @brief append new data set to waterfall
 */

static void sim_render_sky(const gdouble *amp, gsize len)
{

	int i;
	int rs, nc;

	gdouble min = DBL_MAX;
	gdouble max = DBL_MIN;

	guchar *wf;
	guchar *pix;



	GdkPixbuf *pb = sim.gui.pb_sky;



	if (!pb) {


		/* +2 + 1 extra for 0/360 and -90/90 duplicates */
		pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 722, 361);

		if (!pb) {
			g_warning("Could not create pixbuf: out of memory");
			return;
		}

		wf = gdk_pixbuf_get_pixels(pb);

		gdk_pixbuf_fill(pb, 0x000000ff);
		sim.gui.pb_sky = pb;
	}


	wf = gdk_pixbuf_get_pixels(pb);

	rs = gdk_pixbuf_get_rowstride(pb);

	nc = gdk_pixbuf_get_n_channels(pb);



	for (i = 0; i < len; i++) {
		if (amp[i] < min)
			min = amp[i];

		if (amp[i] > max)
			max = amp[i];
	}

	if (!isnormal(min))
		min = 0.;
	if (!isnormal(max))
		max = 1.;

	if (min > max) {
		double tmp = max;

		max = min;
		min = tmp;
	}





	gtk_range_set_range(GTK_RANGE(sim.gui.s_min), min, max);

	pix = wf;
#if 0
	/* set image */
	for (i = 0; i < 722; i++) {
#if 0
		/* first min = regulator min from scale */
		sim_get_rgb((amp[i] - min) / (max - min),
				   0.01, 0.99,
				   &pix[0], &pix[1], &pix[2]);
		pix += nc;
#endif

		pix[0] = 0;
		pix[1] = 255;
		pix += nc;

//		if (i > 1 && i

	}
#endif

	gdouble lat, lon;
	for (lat = -90.0; lat <= 90.0; lat += 0.5) {
		for (lon = -180.; lon <= 180.; lon += 0.5) {
		double val;

		double sig = 0.0;
		int x = (int) (2.0 * (lat + 90.));
		int y = (int) (2.0 * (lon + 180.));

		//g_message("x %d y %d", x, y);
		pix = wf + x * rs + y * nc;

		sig = amp[x * 722 + y];

		val = (sig  - sim.gui.min) / (max - min);

		sim_get_rgb(val,
			    sim.gui.th_lo, sim.gui.th_hi,
			    &pix[0], &pix[1], &pix[2]);

	}
	}


	gtk_widget_queue_draw(GTK_WIDGET(sim.gui.da_sky));
}






static gboolean sim_sky_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{

	GdkPixbuf *pbuf = NULL;

	GtkAllocation allocation;

	if (!sim.gui.pb_sky)
		return FALSE;


	gtk_widget_get_allocation(w, &allocation);

	pbuf = gdk_pixbuf_scale_simple(sim.gui.pb_sky,
				       allocation.width, allocation.height - 2,
				       GDK_INTERP_NEAREST);

	gdk_cairo_set_source_pixbuf(cr, pbuf, 0, 0);
	cairo_paint(cr);

	g_object_unref(pbuf);

	return FALSE;
}





#define VEL 801






/**
 * @brief get the half width of the gaussian scaled by a beam radius
 *       (== 1/2 FWHM) at given sigma
 */

static double gauss_half_width_sigma_r(double sigma, double r)
{
	return r * sigma * sqrt(2.0 * log(2.0));
}


/**
 * @brief returns the value of the width-normalised gaussian for a given
 *	  beam radius (== 1/2 FWHM)
 */

static double gauss_norm(double sigma, double r)
{

	/* The FWHM auf a gaussian with sigma == 1 is 2*sqrt(2*ln(2)) ~2.35482
	 * To normalise x, we multiply by the latter and divide by two, since
	 * we moved the factor from the divisor to the dividend.
	 * We need only half of the FWHM, i.e. 1 / (fwhm / 2) which == 2 / fwhm,
	 * so our final scale factor is 2.35482/fwhm where fwhm == r
	 */
	sigma = sigma / r * 2.0 * sqrt(2.0 * log(2.0));

	return 1.0 / sqrt(2.0 * M_PI) * exp(-0.5 * sigma * sigma);
}

/**
 * @brief compute the euclidian norm of a vector
 */

static double euclid_norm(double x, double y)
{
	return sqrt(x * x + y * y);
}


static int get_offset(double glat, double glon)
{
	/* data is in 0.5 deg steps */
	glon = round(2.0 * glon) * 0.5;
	glat = round(2.0 * glat) * 0.5;
	/* 801 velocity bins (+-400 plus zero), 181 entries per glat (we have
	 * +-90) and ??? degrees per glon */
	return VEL * ((int) ((180. + 181.0) * 2. * glon) + (int) (2.0 * (glat + 90.0)));
}

static gpointer sim_spec_extract_HI_survey(gdouble glat, gdouble glon)
{
	static guint16 *map;

	guint16 *as;

	int offset;



	if (!map) {

		/* efficiency is for dopes...nahh, we'll fix this soon, just stupidly testing */
#if 0
		FILE *data = fopen("/home/armin/Work/radtelsim/vel_short_int.dat", "rb");
#else
		FILE *data = fopen("sky_vel.dat", "rb");
#endif
		if (!data)
			data = fopen("../data/sky_vel.dat", "rb");

		if (!data) {
				GtkWidget *dia;

				dia = gtk_message_dialog_new(NULL,
							     GTK_DIALOG_MODAL,
							     GTK_MESSAGE_ERROR,
							     GTK_BUTTONS_CLOSE,
							     "Please place sky_vel.dat in the "
							     "directory path this program is executed in.");

				gtk_dialog_run(GTK_DIALOG(dia));
				gtk_widget_destroy(dia);


			exit(0);
			/** XXX **/
			g_error("%s: error opening file", __func__);
			return 0;
		}




		map = g_malloc(VEL * SKY_WIDTH * SKY_HEIGHT * sizeof(guint16));

		fread(map, sizeof(guint16), VEL * SKY_WIDTH * SKY_HEIGHT, data);
		fclose(data);
	}

	as = g_malloc(VEL * sizeof(guint16));
	offset = get_offset(glat, glon);

	memcpy(as, &map[offset], sizeof(guint16) * VEL);

	return as;
}


/**
 * @brief order two frequencies so f0 < f1
 *
 * @param f0 the first frequency
 * @param f1 the second frequency
 */

static void sim_order_freq(gdouble *f0, gdouble *f1)
{
	double tmp;

	if (!f0)
		return;

	if (!f1)
		return;

	if ((*f0) < (*f1))
		return;

	tmp   = (*f1);
	(*f1) = (*f0);
	(*f0) = tmp;
}


/**
 * @brief clamp a frequency to the simulator settings
 *
 * @param f the frequency in Hz to clamp into range
 *
 * @returns the clamped frequency in Hz
 */

static gdouble sim_clamp_freq_range(gdouble f)
{
	if (f < sim.radio.freq_min_hz)
		return sim.radio.freq_min_hz;

	if (f > sim.radio.freq_max_hz)
		return sim.radio.freq_max_hz;

	return f;
}


/**
 * @brief round off a frequency to the simulator spectral resolution setting
 *
 * @param f the frequency in Hz to round off
 *
 * @returns the rounded off frequency in Hz
 */

static gdouble sim_round_freq_to_res(gdouble f)
{
	return round(f / sim.radio.freq_inc_hz) * sim.radio.freq_inc_hz;
}


/**
 * @brief get the number of data bins for a frequency range given the
 *	  simulator configuration
 *
 * @param f0 the lower bound frequency in Hz
 * @param f1 the upper bound frequency in Hz
 *
 * @returns the number of bins needed
 * @note f0, f1 must be in order, clamped and rounded
 */

static gsize sim_get_spec_bins(gdouble f0, gdouble f1)
{
	return (gsize) ((f1 - f0) / sim.radio.freq_inc_hz) + 1;
}


/**
 * @brief create empty spectral data for a frequency range
 *
 * @param f0 the lower bound frequency in Hz
 * @param f1 the upper bound frequency in Hz
 *
 * @returns the zeroed struct spec_data or NULL on error
 *
 * @note f0, f1 will be arranged so that always: f0 < f1
 * @note the range will be clamped to the configured simulator parameters
 * @note the step size will be fixed to the configured simulator parameters
 * @note since claming may happen, refer to s->freq_* for further processing
 */


static struct spec_data *sim_create_empty_spectrum(gdouble f0, gdouble f1)
{
	gsize bins;

	struct spec_data *s;

	const gsize data_size = sizeof(typeof(*((struct spec_data *) 0)->spec));


	sim_order_freq(&f0, &f1);

	f0 = sim_clamp_freq_range(f0);
	f1 = sim_clamp_freq_range(f1);

	f0 = sim_round_freq_to_res(f0);
	f1 = sim_round_freq_to_res(f1);

	bins = sim_get_spec_bins(f0, f1);

	if (bins > sim.radio.max_bins) {
		g_warning(MSG "Computed bins %u > sim.radio.max_bins (%u)",
			  bins, sim.radio.max_bins);
	}

	s = g_malloc0(sizeof(struct spec_data) + bins * data_size);
	if (!s)
		return NULL;


	s->n           = (typeof(s->n)) bins;
	s->freq_min_hz = (typeof(s->freq_min_hz)) f0;
	s->freq_max_hz = (typeof(s->freq_max_hz)) f1;
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) sim.radio.freq_inc_hz;

	return s;
}


/**
 * @brief round off a velocity to the HI data resolution
 *
 * @param v the velocity (in km/s) to round off
 *
 * @returns the rounded off velocity in km/s
 */

static gdouble HI_round_vel_to_res(gdouble v)
{
	v = v / SIM_V_RES_KMS;

	if (v > 0.0)
		v = ceil(v);
	else
		v = floor(v);

	return v * SIM_V_RES_KMS;
}


/**
 * @brief clamp a velocity into the available HI data range
 *
 * @param vel the input velocity (in km/s relative to HI data rest freq)
 *
 * @returns vel the range-clamped velocituy
 */

static gdouble HI_clamp_vel_range(gdouble v)
{
	if (v > SIM_V_RED_KMS)
		return SIM_V_RED_KMS;

	if (v < SIM_V_BLU_KMS)
		return SIM_V_BLU_KMS;

	return v;
}


/**
 * @brief get the vlsr-shifted velocity of the spectrum
 *
 * @param f the frequency
 * @param gal the galactic coodrinates
 *
 * @returns the doppler velocity clamped into available HI data range
 *
 */

static gdouble HI_get_vel(double f, struct coord_galactic gal)
{
	gdouble v;

	v = -doppler_vel(f, SIM_V_REF_HZ);
	v = v - vlsr(galactic_to_equatorial(gal), 0.0);

	v = HI_round_vel_to_res(v);

	return HI_clamp_vel_range(v);
}


/**
 * @brief geht the bin (array) offset for a given velocity
 *
 * @param v the velocity in km/s
 *
 * @returns the bin offset in the HI data array for the given velocity
 *
 * @note v must be rounded and clamped to HI data range
 */

static gsize HI_get_vel_bin(gdouble v)
{
	gsize bins;


	bins = (gsize) (SIM_V_RED_KMS - v);

#if 1
	/* account for zero-km/s bin */
	if (v >= 0.0)
		bins += 1;
#endif
	if (bins > SIM_HI_BINS) {
		g_warning(MSG "something went wrong bins %d > %d max bins",
			       bins, SIM_HI_BINS);

		bins = SIM_HI_BINS;
	}

	return bins;
}


/**
 * @brief get the frequency given a velocity for the frequency reference
 *
 * @param v a velocity (in km/s)
 *
 * @returns the doppler frequency (in Hz)
 *
 * @note v must be in the valid HI data range
 */

static gdouble HI_get_freq(double v)
{
	return doppler_freq(v, SIM_V_REF_HZ);
}


/**
 * @brief fold GLAT into a circular range and round to HI base resolution
 *
 * @param lat a galactic latitude
 *
 * @returns the galactic latitude folded in range -90.0 to +90.0
 */

static gdouble HI_fold_glat_round(gdouble lat)
{
	lat = fmod(lat + 90.0, 180.0) - 90.0;

	/* the sky is a sphere :) */
	if (lat < -90.0)
		lat = lat + 180.0;

	if (lat > 90.0)
		lat = lat - 180.0;

	/* map to HI data grid */
	lat = round(( 1.0 / SKY_BASE_RES ) * lat) * SKY_BASE_RES;


	return lat;
}


/**
 * @brief fold GLON into a circular range and round to HI base resolution
 *
 * @param lon galactic longitude
 *
 * @returns the galactic longitude folded in range 0.0 to +360.0
 */

static gdouble HI_fold_glon_round(gdouble lon)
{
	lon = fmod(lon, 360.0);

	if (lon < 0.0)
		lon = lon + 360.0;
	if (lon > 360.0)
		lon = lon - 360.0;

	/* map to HI data grid */
	lon = round(( 1.0 / SKY_BASE_RES ) * lon) * SKY_BASE_RES;


	return lon;
}


/**
 * @brief apply scale calibration to HI data for use with spec data
 *
 * @param val the value to calibrate
 *
 * @returns the calibrated value
 *
 * @note spectra data amplitude unit is milliKelvins
 */

static gdouble HI_raw_amp_to_mKelvins(gdouble val)
{
	return val * SIM_HI_AMP_CAL;
}

/**
 * @brief get theoretical RMS noise sigma
 *
 * @param tsys the system temperature
 * @param tint the integration time
 * @param bw   the observed bandwith
 *
 * @returns the rms noise sigma
 */

static gdouble rms_noise_sigma(gdouble tsys, gdouble tint, gdouble bw)
{
	/* tint can never be 0, so this should be fine */
	return tsys / sqrt(tint * bw);
}


/**
 * @brief extract and compute get the spectrum for a given galatic lat/lon
 *	  center for a given beam
 *
 * @param gal the galactic lat/lon
 * @param red  the red velocity
 * @param blue the blue velocity
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 * @param[out] n_elem the number of elements in the returned array
 *
 * @returns the requested spectrum or NULL on error
 *
 * @note all parameters must be valid for the given Hi data
 */

static gdouble *HI_gen_conv_spec(struct coord_galactic gal,
				 gdouble red, gdouble blue,
				 const gdouble *beam, gsize n, gdouble w,
				 gsize *n_elem)
{
	gsize i;
	gsize r, b;
	gsize bw, bh;
	gsize bins;

	gdouble x, y;
	gdouble off, res;
	gdouble lat, lon;
	gdouble amp;

	gdouble *spec;

	gint16 *raw;


	r = HI_get_vel_bin(red);
	b = HI_get_vel_bin(blue);


	bins = b - r + 1;

	spec = g_malloc0(bins * sizeof(double));

	if (!spec)
		return NULL;


	off = 0.5 * w;
	res = w / ((gdouble) n - 1.0);


	bh = 0;

	for (x = -off; x <= off; x += res) {

		bw = 0;

		for (y = -off; y <= off; y += res) {

			lon = HI_fold_glon_round(gal.lon + x);
			lat = HI_fold_glat_round(gal.lat + y);

			raw = sim_spec_extract_HI_survey(lat, lon);

			for (i = r; i <= b; i++) {
				amp = (gdouble) raw[SIM_HI_BINS - i - 1];
				amp = amp * beam[bh * n + bw];
				spec[i - r] += amp;
			}

			g_free(raw);

			bw++; /* beam width index */
		}

		bh++; /* beam height index */
	}

	for (i = 0; i < bins; i++)
		spec[i] = HI_raw_amp_to_mKelvins(spec[i]);

	(*n_elem) = bins;


	return spec;
}

/**
 * @brief get the index for a given frequency within the target spectrum
 *
 * @param s the target spec_data
 * @param gal the galactic lat/lon
 * @parma f the frequency in Hz
 *
 * @returns the index
 *
 * @note all inputs must be valid for the particular configuration
 */

static gsize HI_get_spec_idx(struct spec_data *s, struct coord_galactic gal,
			     gdouble f)
{
	int idx;

	gdouble v;


	v = vlsr(galactic_to_equatorial(gal), 0.0);

	f = f - (gdouble) s->freq_min_hz - doppler_freq_relative(v, SIM_V_REF_HZ);
	f = f / (gdouble) s->freq_inc_hz;


	idx = (int) f;

	/* adjust for actual spectrum */
	if (idx < 0)
		idx = 0;

	if (idx > s->n)
		idx = s->n;

	return (gsize) idx;
}


/**
 * @brief stack a HI spectrum for a given GLAT/GLON and a beam on a base spectrum
 *
 * @param s the target spec_data
 *
 * @param gal the galactic lat/lon
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 */

static void HI_stack_spec(struct spec_data *s,
			  struct coord_galactic gal,
			  const gdouble *beam, gint n, gdouble w)
{
	gsize bins;
	gsize i, i0, i1;

	gdouble f0, f1;
	gdouble red, blue;

	gdouble *spec;



	red  = HI_get_vel((gdouble) s->freq_min_hz, gal);
	blue = HI_get_vel((gdouble) s->freq_max_hz, gal);

	spec = HI_gen_conv_spec(gal, red, blue, beam, n, w, &bins);

	if (!spec) {
		g_warning(MSG "could not extract HI spectrum");
		return;
	}

	/* we need to know where we'll have to put the corresponding velocity
	 * bins within the spectrum
	 */

	f0 = HI_get_freq(red);
	f1 = HI_get_freq(blue);

	f0 = sim_round_freq_to_res(f0);
	f1 = sim_round_freq_to_res(f1);

	/* find index offsets */
	i0 = HI_get_spec_idx(s, gal, f0);
	i1 = HI_get_spec_idx(s, gal, f1);


	for (i = i0; i <= i1; i++)
		s->spec[i] = (typeof(*s->spec)) ((gdouble) s->spec[i] +
						 spec[i - i0]);


	g_free(spec);
}


static gdouble *gauss_2d(gdouble sigma, gdouble r, gdouble res, gint *n);


/**
 * @brief stack the CMB for a give GLAT/GLON and a beam on a base spectrum
 *
 * @param s the target spec_data
 *
 * @param gal the galactic lat/lon
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 *
 * @todo maybe add a data source (e.g. from Planck, such as found at
 *	 https://irsa.ipac.caltech.edu/data/Planck/release_2/all-sky-maps/previews/COM_CMB_IQU-100-fgsub-sevem-field-Int_2048_R2.01_full/index.html
 *	for now, we just use a uniform CMB
 */

static void sim_stack_cmb(struct spec_data *s,
			  struct coord_galactic gal,
			  const gdouble *beam, gint n, gdouble w)
{
	gsize i;


       	/* add CMB in mK */
	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((double) s->spec[i] + 2725.);
}


/**
 * @brief stack the system temperature on a base spectrum
 *
 * @param tsys a system temperature in Kelvins
 *
 * @param s the target spec_data
 */

static void sim_stack_tsys(struct spec_data *s, gdouble tsys)
{
	gsize i;


	tsys = tsys * 1000.0; /* to mK */

	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((gdouble) s->spec[i] + tsys);
}

/**
 * @brief stack the amplifier noise temperature on a base spectrum
 *
 * @param tsys a system temperature in Kelvins
 *
 * @param noise_fig the noise figure in decibels
 *
 * @param s the target spec_data
 */

static void sim_stack_amp_noise(struct spec_data *s, gdouble tsys, gdouble noise_fig)
{
	gsize i;


	/* noise temperature from noise figure */
	tsys = tsys * (pow(10., noise_fig * 0.1) - 1.);
	tsys = tsys * 1000.0; /* to mK */

	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((gdouble) s->spec[i] + tsys);
}



/**
 * @brief simulate the sun for a given GLAT/GLON and a beam
 *
 * @param gal the galactic lat/lon
 * @param freq a frequency in Hz
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 *
 * @param returns the solar radio temperature
 *
 * @note this is a very simple solar model, where we assume that the
 *	 smallest beam is 0.5 deg, so we can use the beam itself as the
 *	 sun, ignoring any frequency-radius dependency and limb brightening.
 *	 If this simulator is ever to be operated for higher resolutions, or
 *	 frequency ranges significantly outside the neutral hydrogen range,
 *	 this generator must be updated to do an actual convolution on a
 *	 better solar base emission model.
 *
 * @warn this assumes that the beam's coordinate system's basis vector is in
 *	 the same rotation as the horizon coordinate system
 *
 * @todo maybe also add an online data source for up-to-date flux values, e.g.
 *	 ftp://ftp.swpc.noaa.gov/pub/lists/radio/rad.txt
 *
 */

static gdouble sim_sun(struct coord_galactic gal, gdouble freq,
		       const gdouble *beam, gint n, gdouble w)
{
	gssize x, y;

	gdouble res;

	gdouble D;
	gdouble A;
	gdouble l;
	gdouble T;

	struct coord_galactic sun;


	sun = equatorial_to_galactic(sun_ra_dec(0.0));

	/* force onto grid */
	sun.lat = round(( 1.0 / SKY_BASE_RES ) * sun.lat) * SKY_BASE_RES;
	sun.lon = round(( 1.0 / SKY_BASE_RES ) * sun.lon) * SKY_BASE_RES;

	res = w / (gdouble) n;

	x = (gssize) ((sun.lat - gal.lat) / res + 0.5 * (gdouble) n);
	y = (gssize) ((sun.lon - gal.lon) / res + 0.5 * (gdouble) n);

	if ((x < 0) || (y < 0))
		return 0.0;

	if ((x >= n) || (y >= n))
		return 0.0;


	l = 299792458.0 / freq;

	D = 1.22 * l / atan(2.0 * sim.r_beam / 180.0 * M_PI);

	A = 0.25 * D * D * M_PI;

	/* convert flux to temperature
	 * sun is in solar flux units, i.e. 10^-22 W/m^2/Hz
	 */

	T = A * 0.5 * 1e-22 * sim.sun_sfu / 1.38064852e-23;


	/* beam is normalised to integral 1, scale temperature to center bin */
	T = T * beam[y * n + x] / beam[n/2 * n + n/2];

	return T;
}


/**
 * @brief stack the sun for a given GLAT/GLON and a beam on a base spectrum
 *
 * @param s the target spec_data
 *
 * @param gal the galactic lat/lon
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 */

static void sim_stack_sun(struct spec_data *s, struct coord_galactic gal,
			  const gdouble *beam, gint n, gdouble w)
{
	gsize i;

	gdouble T;


	T = sim_sun(gal, s->freq_min_hz, beam, n, w);

	/* convert to mK */
	T = 1000. * T;

	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((double) s->spec[i] + T);
}


/**
 * @brief simulate moon for a given GLAT/GLON and a beam
 *
 * @param gal the galactic lat/lon
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 *
 * @param returns the moon's radio temperature
 *
 * @note this is a very simple moon model, where we assume that the
 *	 smallest beam is 0.5 deg, so we can use the beam itself as the
 *	 moon
 *
 * @warn this assumes that the beam's coordinate system's basis vector is in
 *	 the same rotation as the horizon coordinate system
 *
 */

static gdouble sim_moon(struct coord_galactic gal, const gdouble *beam,
			gint n, gdouble w)
{
	gssize x, y;

	gdouble res;

	gdouble E;
	gdouble T;

	gdouble a, b;
	gdouble costheta;

	struct coord_galactic moon;
	struct coord_galactic sun;


	moon = equatorial_to_galactic(moon_ra_dec(server_cfg_get_station_lat(),
						  server_cfg_get_station_lon(),
						  0.0));
	/* force onto grid */
	moon.lat = round(( 1.0 / SKY_BASE_RES ) * moon.lat) * SKY_BASE_RES;
	moon.lon = round(( 1.0 / SKY_BASE_RES ) * moon.lon) * SKY_BASE_RES;

	res = w / (gdouble) n;

	x = (gssize) ((moon.lat - gal.lat) / res + 0.5 * (gdouble) n);
	y = (gssize) ((moon.lon - gal.lon) / res + 0.5 * (gdouble) n);


	if ((x < 0) || (y < 0))
		return 0.0;

	if ((x >= n) || (y >= n))
		return 0.0;


	/* approximate the reflected solar energy per unit area via the
	 * solar constant in Earth's neighbourhood, reduced by the angle
	 * between the surface of the moon facing the earth and the sun
	 * (varname I is reserved due to complex.h)
	 */

	sun = equatorial_to_galactic(sun_ra_dec(0.0));

	a = sun.lat - moon.lat;
	b = sun.lon - moon.lon;

	costheta = (sun.lon * sun.lon + sun.lat * sun.lat) / euclid_norm(a, b);

	costheta = fmod(costheta - 180.0, 360.) / 180. * M_PI;

	/* average the moon temperature across all degrees of latitude by
	 * divding by pi
	 */
	E = 1366.0 * costheta / M_PI;

	/* for emissivity ~ reflectance, we get the Moon's surface temperature
	 * via the  Stefan-Boltzmann equation
	 */
	T = pow((E / 5.67e-8), 0.25);

	/* scale to beam/moon ratio (apparent diameter ~0.5 deg) */

	T = T * 0.25 * 0.25 / (sim.r_beam * sim.r_beam);

	/* beam is normalised to integral 1, scale temperature to center bin */
	T = T * beam[y * n + x] / beam[n/2 * n + n/2];

	return T;
}


/**
 * @brief stack the moon for a given GLAT/GLON and a beam on a base spectrum
 *
 * @param s the target spec_data
 *
 * @param gal the galactic lat/lon
 * @param beam an nxn array which describes the shape of the beam
 * @param n the shape of the beam in data bins
 * @param w the width of the area in degrees (== width of the beam)
 *
 * @note this is a very simple moon model, where we assume that the
 *	 smallest beam is 0.5 deg, so we can use the beam itself as the
 *	 moon
 *
 * @warn this assumes that the beam's coordinate system's basis vector is in
 *	 the same rotation as the horizon coordinate system
 *
 */

static void sim_stack_moon(struct spec_data *s, struct coord_galactic gal,
			   const gdouble *beam, gint n, gdouble w)
{
	gsize i;

	gdouble T;


	T = sim_moon(gal, beam, n, w);

	/* convert to mK */
	T = 1000. * T;

	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((double) s->spec[i] + T);
}


/**
 * @brief stack system efficiency on a base spectrum
 *
 * @param s the target spec_data
 * @param eff a efficiency multiplier
 */

static void sim_stack_eff(struct spec_data *s, gdouble eff)
{
	gsize i;


	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((gdouble) s->spec[i] * eff);
}


/**
 * @brief generate approximate gaussian noise
 *
 * @note we want something decently fast, this Box-Muller transform
 *	 appears to work quite well
 */

static gdouble sim_rand_gauss(void)
{
	static gboolean gen;

	static gdouble u, v;


	gen = !gen;

	if (!gen)
		return sqrt(- 2.0 * log(u)) * cos(2.0 * M_PI * v);

	u = (rand() + 1.0) / (RAND_MAX + 2.0);
	v =  rand()        / (RAND_MAX + 1.0);

	return sqrt(-2.0 * log(u)) * sin(2.0 * M_PI * v);
}



/**
 * @brief stack system noise on a base spectrum
 *
 * @param s the target spec_data
 * @param sig a sigma mutliplier for the noise
 *
 * @note the noise is scaled by the sqrt() of the signal amplitude
 */

static void sim_stack_gnoise(struct spec_data *s, gdouble sig)
{
	gsize i;

	gdouble amp;


	for (i = 0; i < s->n; i++) {
		amp = (gdouble) s->spec[i];
		amp = amp + sqrt(amp) * sig * sim_rand_gauss();
		s->spec[i] = (typeof(*s->spec)) amp;
	}
}


/**
 * @brief stack the hot load temperature on a base spectrum
 *
 * @param tload a temperature in Kelvins
 *
 * @param s the target spec_data
 */

static void sim_stack_hot_load(struct spec_data *s, gdouble tload)
{
	gsize i;


	tload = tload * 1000.0; /* to mK */

	for (i = 0; i < s->n; i++)
		s->spec[i] = (typeof(*s->spec)) ((gdouble) s->spec[i] + tload);
}

/**
 * @brief transpose a two-dimensional array of complex doubles
 *
 * @param out the output array
 * @param in the input array
 *
 * @param w the width of the input array
 * @param h the height of the input array
 */

static void transpose(complex double *out, complex double *in, int w, int h)
{
	int i, j, n;

#pragma omp parallel for private(i), private(j)
	for(n = 0; n < w * h; n++) {

		i = n / h;
		j = n % h;

		out[n] = in[j * w + i];
	}
}

/**
 * @brief put one image into another image cyclically
 *
 * @param dst the destination matrix
 * @param dw the width of the destination
 * @param dh the height of the destination
 *
 * @param src the source matrix
 * @param sw the width of the source
 * @param sh the height of the source
 *
 * @param x the upper left corner destination x coordinate
 * @param y the upper left corner destination y coordinate
 *
 * @returns 0 on success, otherwise error
 *
 * @note src must be smaller or equal dst; src will wrap within dst,
 *	 x and y are taken as mod w and mod h respectively
 */

static int put_matrix(double complex *dst, int dw, int dh,
		      const double  *src, int sw, int sh,
		      int x, int y)
{
	int i, j;
	int dx, dy;


	if (!dst || !src)
		return -1;

	if ((dw < sw) || (dh < sh))
		return -1;


#pragma omp parallel for private(dy)
	for (i = 0; i < sh; i++) {

		dy = y + i;

		/* fold into dest height */
		if (dy < 0 || dy > dh)
			dy = (dy + dh) % dh;

#pragma omp parallel for private(dx)
		for (j = 0; j < sw; j++) {

			dx = x + j;

			/* fold into dest width */
			if (dx < 0 || dx > dw)
				dx = (dx + dw) % dw;

			dst[dy * dw + dx] =  src[i * sw + j];
		}
	}

	return 0;
}


/**
 * @brief get one image from another image cyclically
 *
 * @param dst the destination matrix
 * @param dw the width of the destination
 * @param dh the height of the destination
 *
 * @param src the source matrix
 * @param sw the width of the source area
 * @param sh the height of the source area
 *
 * @param x the upper left corner source area x coordinate
 * @param y the upper left corner source area y coordinate
 * @param w the source area width
 * @param h the source area height
 *
 * @returns 0 on success, otherwise error
 *
 * @note dst must be larger or equal src
 *       the upper left corner of the sleected area area will be placed at 0,0
 *       within dst
 *       the src area will wrap within src, as x an y are taken as mod w and
 *       mod h respectively for both source and destination
 */

static int get_matrix(double *dst, int dw, int dh,
		      const double complex *src, int sw, int sh,
		      int x, int y, int w, int h)
{
	int i, j;
	int sx, sy;


	if (!dst || !src)
		return -1;

	if ((dw < w) || (dh < h))
		return -1;

	if ((w > sw) || (h > sh))
		return -1;


#pragma omp parallel for private(sy)
	for (i = 0; i < h; i++) {

		sy = y +i;

		/* fold into src height */
		if (sy < 0 || sy > sh)
			sy = (sy + sh) % sh;

#pragma omp parallel for private(sx)
		for (j = 0; j < w; j++) {

			sx  = x + j;

			/* fold into src width */
			if (sx < 0 || sx > sw)
				sx = (sx + sw) % sw;

			dst[i * dw + j] = creal(src[sy * sw + sx]);
		}
	}

	return 0;
}


/**
 * @brief in-place perform a two-dimensional fft on data
 *
 * @param data the data matrix to transform
 * @param coeff a precomputed coefficient array, may be NULL
 * @param n the fft size
 *
 * @note the data and coefficient array dimensions must match the fft size
 *
 * @returns 0 on succes, otherwise error
 */

static int fft2d(double complex *data, double complex *coeff, int n)
{
	int i;
	double complex *tmp;


	tmp = malloc(n * n * sizeof(double complex));
	if (!tmp)
		return -1;
	/* inverse transform rows */
#pragma omp parallel for
	for (i = 0; i < n; i++)
		fft2(&data[i * n], coeff, n, FFT_INVERSE);

	/* transpose forward */
	transpose(tmp, data, n, n);

	/* inverse transform columns */
#pragma omp parallel for
	for (i = 0; i < n; i++)
		fft2(&tmp[i * n], coeff, n, FFT_INVERSE);

	memcpy(data, tmp, n * n * sizeof(double complex));

	return 0;
}



/**
 * @brief collapses the data cube along the velocity axis and return the
 *	  allocated image data
 *	  valid inputs are SIM_V_BLU_KMS to SIM_V_RED_KMS
 *
 */

static gdouble *sim_rt_get_HI_img(gint vmin, gint vmax)
{
	gdouble sig;
	gdouble lat, lon;

	gdouble *sky;

	gint16 *rawspec;


	if (vmin > vmax) {
		gint tmp;

		/* shit happens... */
		tmp  = vmin;
		vmin = vmax;
		vmax = tmp;
	}

	/* do we need to include the zero-velocity bin? */
	if ((vmin < 0) && (vmax > 0))
		vmax += 1;

	/* to array index */

	vmin += VEL / 2;
	vmax += VEL / 2;


	sky = g_malloc(SKY_WIDTH * SKY_HEIGHT * sizeof(gdouble));


	for (lon = 0.; lon <= 360.; lon += 0.5) {
		for (lat = -90.0; lat <= 90.0; lat += 0.5) {
			int i;

			rawspec = sim_spec_extract_HI_survey(lat, lon);

			sig = 0.0;

			for (i = vmin; i < vmax; i++)
				sig += (double) rawspec[i];

			g_free(rawspec);


			sky[(int) (2.0 * (90. - lat)) * 722 + (int) (2.0 * fmod(540. - lon, 360.))]  = sig * SKY_SIG_TO_KELVIN;
		}
	}

	return sky;
}


/**
 * @brief returns an NxN normalised gaussian convolution kernel for a given
 *	  sigma cutoff and resolution in degrees
 *
 * @note bins is always uneven (need center bin)
 *
 * @note this does not integrate the gaussian within a "pixel", i.e. with
 *	 respect to the integer coordinates between sample locations, so this
 *	 is technically not a correct convolution kernel; doing so would be
 *	 quite expensive, since we'd have to oversample the grid quite a lot
 *	 (for no added visual benefit)
 */

static gdouble *gauss_2d(gdouble sigma, gdouble r, gdouble res, gint *n)
{
	gint i, j;
	gint bins;

	gdouble x;

	gdouble sum;

	gdouble *ker;


	/* our width-normalised kernel runs from -sigma to +sigma */
	x = gauss_half_width_sigma_r(sigma, r);

	/* need an extra center bin, does not count towards stepsize */
	bins = (gint) (round((2.0 * x) / res));

	if (!(bins & 0x1))
	    bins++;


	ker = g_malloc(bins * bins * sizeof(double));

	sum = 0.0;

	for (i = 0; i < bins; i++) {
		for (j = 0; j < bins; j++)  {

			gdouble s;

			/* calculate norm and scale to resolution, take 1/2
			 * for symmetry (we go from x0 to x1)
			 */
			s = 0.5 * res * euclid_norm((i - bins/2), (j - bins/2));

			ker[i * bins + j] = 0.5 * ((gauss_norm(s, r)));
			sum += ker[i * bins + j];
		}
	}

	/* normalise kernel */
	for (i = 0; i < bins * bins; i++)
		ker[i] /= sum;


	(*n) = bins;

	return ker;
}




static double complex *ft;
static double complex *it;



#define SKY1_WIDTH  1024
#define SKY1_HEIGHT 1024

#if 0

static gdouble *sky_convolve(const gdouble *sky, gdouble *kernel, gint n)
{
	gint i, j;
	gint x, y;

	gint sx, sy;


	gdouble sig;

	gdouble *csky;


	if (!(n & 0x1))
	    g_warning("N is %d", n);

	csky = g_malloc(SKY_WIDTH * SKY_HEIGHT * sizeof(gdouble));


	for (y = 0; y < SKY_HEIGHT; y++) {
		for (x = 0; x < SKY_WIDTH; x++) {

			sig = 0.0;

			/* n is always uneven */
			for (i = -n/2; i < n/2; i++) {


				sx = x + i;

				/* fold into range */
				sx = (sx + SKY_WIDTH) % SKY_WIDTH;

				for (j = -n/2; j < n/2; j++) {
					sy = y + j;

					sy = (sy + SKY_HEIGHT) % SKY_HEIGHT;

					sig += sky[sy * SKY_WIDTH + sx] * kernel[(i+ n/2) * n + j + n/2];
				}
			}
			csky[y * SKY_WIDTH + x] = sig;
		}
	}

	return csky;
}

#endif




static double complex *ker;
static double complex *csky;
static double complex *conv;
static gdouble *msky;
static gdouble *raw_sky;
static gdouble *kernel;

static void sim_gen_sky(void)
{
	int i;

	static gint vmin, vmax;
	gint v1, v2;

	gint n;


	GTimer *timer;

	timer = g_timer_new();




	if (msky) {
		g_free(msky);
		msky =NULL;
	}


	if (kernel) {
		g_free(kernel);
		kernel =NULL;
	}


	if (ker) {
		g_free(ker);
		kernel =NULL;
	}



	if (!ft)
		ft = fft_prepare_coeff(1024, FFT_FORWARD);

	if (!it)
		it = fft_prepare_coeff(1024, FFT_INVERSE);


	v1  = gtk_spin_button_get_value_as_int(sim.gui.sb_vmin);
	v2  = gtk_spin_button_get_value_as_int(sim.gui.sb_vmax);

	if (v1 != vmin || v2 != vmax) {

		if (raw_sky) {
			g_free(raw_sky);
			raw_sky  = NULL;
		}

		vmin = v1;
		vmax = v2;
	}

	kernel = gauss_2d(3.0, sim.r_beam, SKY_BASE_RES, &n);

	if (!conv)
		conv = g_malloc(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));

	if (!raw_sky) {
#if 0
		gdouble T;
		struct coord_galactic gal;

		gdouble w  = 2.0 * gauss_half_width_sigma_r(3.0, sim.r_beam);
#endif
		raw_sky = sim_rt_get_HI_img(vmin, vmax);

		if (csky) {
			g_free(csky);
			csky =NULL;
		}

#if 0
		gal = equatorial_to_galactic(moon_ra_dec(server_cfg_get_station_lat(),
							 server_cfg_get_station_lon(),
							 0.0));

		T = sim_moon(gal, kernel, n, w);

		T = T * VEL * 100.;

		/* force onto grid */
		gal.lat = round(( 1.0 / SKY_BASE_RES ) * gal.lat) * SKY_BASE_RES;
		gal.lon = round(( 1.0 / SKY_BASE_RES ) * gal.lon) * SKY_BASE_RES;

		gal.lat = 90. - gal.lat;
		gal.lon = gal.lon - 90.;

		raw_sky[SKY1_WIDTH * ((int)gal.lat) * 2 + (int)gal.lon * 2] += T;



		gal = equatorial_to_galactic(sun_ra_dec(0.0));

		T = sim_sun(gal, SIM_V_REF_HZ, kernel, n, w);

		T = T * VEL * 100.;
		g_message("at %g %g T %g", gal.lat, gal.lon, T);

		/* force onto grid */
		gal.lat = round(( 1.0 / SKY_BASE_RES ) * gal.lat) * SKY_BASE_RES;
		gal.lon = round(( 1.0 / SKY_BASE_RES ) * gal.lon) * SKY_BASE_RES;

		gal.lat = gal.lat + 90.;
		gal.lon = gal.lon - 90.;
		g_message("at %g %g T %g", gal.lat, gal.lon, T);

		raw_sky[SKY1_WIDTH * ((int)gal.lat) * 2 + (int)gal.lon * 2] += T;
#endif


		csky  = g_malloc0(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));


		put_matrix(csky, SKY1_WIDTH, SKY1_HEIGHT, raw_sky, SKY_WIDTH, SKY_HEIGHT, 0, 0);

		fft2d(csky, ft, SKY1_WIDTH);
	}


#if 1
#else
	kernel = gauss_conv_kernel(sim.r_beam, 3.0, SKY_BASE_RES, &n);
#endif
	ker = g_malloc0(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));

#if 1
	put_matrix(ker, SKY1_WIDTH, SKY1_HEIGHT, kernel, n, n, -n/2, -n/2);
#else
	put_matrix(ker, SKY1_WIDTH, SKY1_HEIGHT, kernel, n, n, 360 - n/2, 180 - n/2);
#endif
#if 0
	msky = g_malloc(SKY_WIDTH * SKY_HEIGHT * sizeof(double));
	get_matrix(msky, SKY_WIDTH, SKY_HEIGHT, ker, SKY1_WIDTH, SKY1_HEIGHT,
		   0, 0, SKY_WIDTH, SKY_HEIGHT);
	sim_render_sky(msky, SKY_WIDTH * SKY_HEIGHT);

	return;
#endif
	fft2d(ker, ft, SKY1_WIDTH);

	g_timer_start(timer);

	/* multiplication step */
#pragma omp parallel for
	for (i = 0; i < SKY1_HEIGHT * SKY1_WIDTH; i++)
		conv[i] = csky[i] * ker[i];


	fft2d(conv, it, SKY1_WIDTH);
	msky = g_malloc(SKY_WIDTH * SKY_HEIGHT * sizeof(double));
	get_matrix(msky, SKY_WIDTH, SKY_HEIGHT, conv, SKY1_WIDTH, SKY1_HEIGHT,
		   0, 0, SKY_WIDTH, SKY_HEIGHT);


	g_message("msky %g", msky[SKY_WIDTH * SKY_HEIGHT/2 + SKY_WIDTH/2]);

	sim_render_sky(msky, SKY_WIDTH * SKY_HEIGHT);

	g_timer_stop(timer);

	g_message("render time %gs", g_timer_elapsed(timer, NULL));


	g_timer_destroy(timer);

}


static void sim_gui_redraw_sky(void)
{
	if (msky)
		sim_render_sky(msky, 722 * 361);
}
static void sim_redraw_cb(GtkWidget *w, gpointer dat)
{
	sim_gen_sky();
}

static gboolean sky_spb_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.r_beam = gtk_spin_button_get_value(sim.gui.sb_beam);
	sim_gen_sky();
}

static void sim_gui_slide_value_changed(GtkRange *range, gpointer data)
{
	gdouble *val;


	val = (gdouble *) data;

	(*val) = gtk_range_get_value(range);

	sim_gui_redraw_sky();
}

static gboolean sim_spb_lat_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	struct packet pkt;


	server_cfg_set_station_lat(gtk_spin_button_get_value(sb));

	pkt.trans_id = PKT_TRANS_ID_UNDEF;

	proc_pr_capabilities(&pkt);
	proc_pr_capabilities_load(&pkt);
}

static gboolean sim_spb_lon_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	struct packet pkt;


	server_cfg_set_station_lon(gtk_spin_button_get_value(sb));

	pkt.trans_id = PKT_TRANS_ID_UNDEF;

	proc_pr_capabilities(&pkt);
	proc_pr_capabilities_load(&pkt);
}


static gboolean sim_spb_tsys_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.tsys = gtk_spin_button_get_value(sb);
}

static gboolean sim_spb_eff_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.eff = gtk_spin_button_get_value(sb);
}

static gboolean sim_spb_sig_n_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.sig_n = gtk_spin_button_get_value(sb);
}

static gboolean sim_spb_readout_hz_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.readout_hz = gtk_spin_button_get_value(sb);
}

static gboolean sim_sun_sfu_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.sun_sfu = gtk_spin_button_get_value(sb);
}

static gboolean sim_noise_fig_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.noise_fig = gtk_spin_button_get_value(sb);
}

static gboolean sim_hot_load_temp_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	struct packet pkt;


	sim.hot_load_temp = gtk_spin_button_get_value(sb);

	pkt.trans_id = PKT_TRANS_ID_UNDEF;

	proc_pr_capabilities_load(&pkt);
}


static void sim_rt_gui_defaults(void)
{
	sim.gui.th_lo = 0.01;
	sim.gui.th_hi = 0.99;
}


static GtkWidget *sim_rt_par_gui(void)
{
	GtkWidget *w;
	GtkWidget *frame;
	GtkWidget *grid;



	frame = gtk_frame_new("Simulation");
	g_object_set(frame, "margin", 6, NULL);

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	g_object_set(grid, "margin", 6, NULL);

	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(grid));


	w = gtk_label_new("Rbeam [deg]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_spin_button_new_with_range(0.25, 5.0, 0.05);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), sim.r_beam);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);
	sim.gui.sb_beam = GTK_SPIN_BUTTON(w);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sky_spb_value_changed_cb), NULL);

	w = gtk_label_new("TSYS [K]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);

	w = gtk_spin_button_new_with_range(0., 1000., 1.);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_TSYS);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_tsys_value_changed_cb), NULL);

	w = gtk_label_new("Sigma");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

	w = gtk_spin_button_new_with_range(0., 20.0, 0.1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_SIG_NOISE);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_sig_n_value_changed_cb), NULL);


	w = gtk_label_new("Eff.");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 3, 1, 1);

	w = gtk_spin_button_new_with_range(0., 1.0, 0.1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_EFF);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 3, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_eff_value_changed_cb), NULL);



	w = gtk_label_new("LAT [deg]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 4, 1, 1);

	w = gtk_spin_button_new_with_range(-90., 90., 0.01);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), server_cfg_get_station_lat());
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 4, 1, 1);


	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_lat_value_changed_cb), NULL);

	w = gtk_label_new("LON [deg]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 5, 1, 1);

	w = gtk_spin_button_new_with_range(-180., 180., 0.01);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), server_cfg_get_station_lon());
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 5, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_lon_value_changed_cb), NULL);


	w = gtk_label_new("Rate [Hz]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 6, 1, 1);

	w = gtk_spin_button_new_with_range(0.1, 32.0, 1.0);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_READOUT_HZ);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 6, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_readout_hz_value_changed_cb), NULL);

	w = gtk_label_new("Sun [SFU]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 7, 1, 1);

	w = gtk_spin_button_new_with_range(0.0, 200., 1.0);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_SUN_SFU);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 7, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_sun_sfu_value_changed_cb), NULL);



	w = gtk_label_new("Hot Load [K]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 8, 1, 1);

	w = gtk_spin_button_new_with_range(0.0, 1000., 1.0);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_HOT_LOAD_TEMP);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 8, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_hot_load_temp_value_changed_cb), NULL);


	w = gtk_label_new("Noise Fig. [dB]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 10, 1, 1);

	w = gtk_spin_button_new_with_range(0.1, 4.0, 0.1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_NOISE_FIG);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 10, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_noise_fig_value_changed_cb), NULL);





	return frame;
}



void sim_exit_widget_destroy(GtkWidget *w)
{
	exit(0);
}


static void sim_rt_create_gui(void)
{
	GtkWidget *w;
	GtkWidget *grid;

	GtkWidget *win;
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *hdr;

	sim_rt_gui_defaults();


	gtk_init(NULL, NULL);

	win= gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(win, "destroy", G_CALLBACK(sim_exit_widget_destroy),
			 NULL);


	hdr = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hdr), TRUE);

	gtk_window_set_titlebar(GTK_WINDOW(win), hdr);









	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_container_add(GTK_CONTAINER(win), box);


	w = gtk_frame_new("Sky Map");
	g_object_set(w, "margin", 6, NULL);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);

	sim.gui.da_sky = GTK_DRAWING_AREA(gtk_drawing_area_new());
	gtk_widget_set_size_request(GTK_WIDGET(sim.gui.da_sky), 360, 180);
	g_object_set(GTK_WIDGET(sim.gui.da_sky), "margin", 12, NULL);

	gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(sim.gui.da_sky));

	g_signal_connect(G_OBJECT(sim.gui.da_sky), "draw",
			 G_CALLBACK(sim_sky_draw), NULL);



	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(box), sim_rt_par_gui(), FALSE, TRUE, 0);

	w = gtk_label_new("Vmin [km/s]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_spin_button_new_with_range(SIM_V_BLU_KMS, SIM_V_RED_KMS, 1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_V_RED_KMS);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);
	sim.gui.sb_vmin = GTK_SPIN_BUTTON(w);


	w = gtk_label_new("Vmax [km/s]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);

	w = gtk_spin_button_new_with_range(SIM_V_BLU_KMS, SIM_V_RED_KMS, 1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_V_BLU_KMS);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);
	sim.gui.sb_vmax = GTK_SPIN_BUTTON(w);




	w = gtk_button_new_with_label("Redraw");
	gtk_widget_set_halign(w, GTK_ALIGN_CENTER);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(sim_redraw_cb), NULL);
	gtk_widget_set_vexpand(w, FALSE);


	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_grid_attach(GTK_GRID(grid), box, 0, 3, 1, 1);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	w = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, -0.2, 1., 0.01);
	gtk_scale_add_mark(GTK_SCALE(w), sim.gui.th_lo, GTK_POS_LEFT, NULL);
	gtk_range_set_value(GTK_RANGE(w), sim.gui.th_lo);
	gtk_range_set_inverted(GTK_RANGE(w), TRUE);
	sim.gui.s_lo = GTK_SCALE(w);
	g_signal_connect(G_OBJECT(w), "value-changed",
			 G_CALLBACK(sim_gui_slide_value_changed),
			 &sim.gui.th_lo);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	w = gtk_label_new("Lo");
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, 0);
	gtk_widget_set_vexpand(vbox, TRUE);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	w = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0.0, 1.2, 0.01);
	gtk_range_set_value(GTK_RANGE(w), sim.gui.th_hi);
	gtk_scale_add_mark(GTK_SCALE(w), sim.gui.th_hi, GTK_POS_LEFT, NULL);
	gtk_range_set_inverted(GTK_RANGE(w), TRUE);
	sim.gui.s_hi = GTK_SCALE(w);
	g_signal_connect(G_OBJECT(w), "value-changed",
			 G_CALLBACK(sim_gui_slide_value_changed),
			 &sim.gui.th_hi);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	w = gtk_label_new("Hi");
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, 0);
	gtk_widget_set_vexpand(vbox, TRUE);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	w = gtk_scale_new(GTK_ORIENTATION_VERTICAL, NULL);
	gtk_range_set_inverted(GTK_RANGE(w), TRUE);
	sim.gui.s_min = GTK_SCALE(w);
	gtk_scale_set_draw_value(GTK_SCALE(w), FALSE);
	g_signal_connect(G_OBJECT(w), "value-changed",
			 G_CALLBACK(sim_gui_slide_value_changed),
			 &sim.gui.min);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	w = gtk_label_new("Lvl");
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, 0);
	gtk_widget_set_vexpand(vbox, TRUE);




#if 0
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "LIN");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "LOG");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "EXP");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "SQRT");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "SQUARE");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "ASINH");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "SINH");
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(sim_gui_scale_mode_changed), NULL);

	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);
#endif










	gtk_widget_show_all(win);

	sim_gen_sky();
}





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
	hor.az = sim.az.cur;
	hor.el = sim.el.cur;

	gal = horizontal_to_galactic(hor, server_cfg_get_station_lat(), server_cfg_get_station_lon());


	st.busy = 1;
	st.eta_msec = (typeof(st.eta_msec)) 20.0;	/* whatever */
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);

	/* conv */
	static gdouble *beam;
	static gdouble r;
	static gdouble sky_deg;
	static gint n_beam;

	static gdouble glat;
	static gdouble glon;


	gal.lat = round(( 1.0 / SKY_BASE_RES ) * gal.lat) * SKY_BASE_RES;
	gal.lon = round(( 1.0 / SKY_BASE_RES ) * gal.lon) * SKY_BASE_RES;

	if (r != sim.r_beam || !beam) {

		r = sim.r_beam;

		if (beam) {
			g_free(beam);
			beam = NULL;
		}

		sky_deg = 2.0 * gauss_half_width_sigma_r(3.0, sim.r_beam);
		beam = gauss_2d(3.0, sim.r_beam, SKY_BASE_RES, &n_beam);
	}


	if ((glat != gal.lat) || (glon != gal.lon))
		glat = gal.lat;


	s = sim_create_empty_spectrum(g_obs.acq.freq_start_hz,
				      g_obs.acq.freq_stop_hz);


	if (!s) {
		g_warning(MSG "could not create spectral data");
		return 0;
	}


	/* update theoretical rms sigma */
	sim.sig_rms = rms_noise_sigma(sim.tsys, 1.0 / sim.readout_hz,
				      g_obs.acq.freq_stop_hz - g_obs.acq.freq_start_hz);


	/* NOTE: values in s->spec must ALWAYS be >0, since it is (usually)
	 * a uin32_t. We do this by adding at least the non-zero preamp noise
	 */

	if (sim.hot_load_ena)
		sim_stack_hot_load(s, sim.hot_load_temp);

	sim_stack_cmb(s, gal, beam, n_beam, sky_deg);

	sim_stack_moon(s, gal, beam, n_beam, sky_deg);

	sim_stack_sun(s, gal, beam, n_beam, sky_deg);

	HI_stack_spec(s, gal, beam, n_beam, sky_deg);

	sim_stack_eff(s, sim.eff);

	sim_stack_tsys(s, sim.tsys);

	sim_stack_gnoise(s, sim.sig_n);

	sim_stack_amp_noise(s, sim.tsys, sim.noise_fig);

	sim_stack_gnoise(s, sim.sig_rms);



	/* handover for transmission */
	ack_spec_data(PKT_TRANS_ID_UNDEF, s);


	st.busy = 0;
	st.eta_msec = 0;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);

	obs->acq.acq_max--;

	g_free(s);

	g_usleep(G_USEC_PER_SEC / sim.readout_hz);

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
 * @brief enable/disable hot load
 *
 */

static void sim_hot_load_enable(gboolean mode)
{
	sim.hot_load_ena = mode;


	if (mode)
		ack_hot_load_enable(PKT_TRANS_ID_UNDEF);
	else
		ack_hot_load_disable(PKT_TRANS_ID_UNDEF);
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
#if 0
	/* signal the acquisition thread outer loop */
	if (g_mutex_trylock(&acq_lock)) {
		g_cond_signal(&acq_cond);
		g_mutex_unlock(&acq_lock);
	}
#endif
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

	struct observation *obs;


	obs = g_malloc(sizeof(struct observation));
	if (!obs) {
		g_error(MSG "memory allocation failed: %s: %d",
			__func__, __LINE__);
		return -1;
	}

	obs->acq.freq_start_hz = acq->freq_start_hz;
	obs->acq.freq_stop_hz  = acq->freq_stop_hz;
	obs->acq.bw_div        = 0;
	obs->acq.bin_div       = 0;
	obs->acq.n_stack       = 0;
	obs->acq.acq_max       = ~0;

	g_thread_new(NULL, sim_acquisition_update, (gpointer) obs);
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
 * @brief hot load enable/disable
 */

G_MODULE_EXPORT
int be_hot_load_enable(gboolean mode)
{
	sim_hot_load_enable(mode);

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
 * @brief get telescope spectrometer capabilities_load
 */

G_MODULE_EXPORT
int be_get_capabilities_load_spec(struct capabilities_load *c)
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
	c->hot_load		= (uint32_t) sim.hot_load_temp * 1000.0;

	/* push along hot load status, so we ensure that
	 * any connecting client is informed immediately
	 */

	if (c->hot_load) {
		if (sim.hot_load_ena)
			ack_hot_load_enable(PKT_TRANS_ID_UNDEF);
		else
			ack_hot_load_disable(PKT_TRANS_ID_UNDEF);
	}


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
 * @brief get telescope drive capabilities_load
 */

G_MODULE_EXPORT
int be_get_capabilities_load_drive(struct capabilities_load *c)
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

	sim_hot_load_enable(FALSE);

	sim_spec_cfg_defaults();

	sim_rt_create_gui();
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
