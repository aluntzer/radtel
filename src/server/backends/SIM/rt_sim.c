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


/* default allowed HW ranges */
#define SIM_FREQ_MIN_HZ	DOPPLER_FREQ(SIM_V_RED_KMS, SIM_V_REF_MHZ * 1e6)
#define SIM_FREQ_MAX_HZ DOPPLER_FREQ(SIM_V_BLU_KMS, SIM_V_REF_MHZ * 1e6)
#define SIM_FREQ_STP_HZ DOPPLER_FREQ_REL(SIM_V_RES_KMS, SIM_V_REF_MHZ * 1e6)
#define SIM_IF_BW_HZ	(SIM_FREQ_MAX_HZ - SIM_FREQ_MIN_HZ)
#define SIM_BINS	((SIM_V_RED_KMS - SIM_V_BLU_KMS) / SIM_V_RES_KMS)
#define SIM_BW_DIV_MAX	0

/* other properties */

#define SIM_BEAM_RADIUS		0.25	/* default beam radius */

#define SIM_TSYS		100.	/* default system temperature */
#define SIM_EFF			0.6	/* default efficiency */

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
       	int w, h;
	int rs, nc;

	gdouble min = DBL_MAX;
	gdouble max = DBL_MIN;
	gdouble n, n1;

	guchar *wf;
	guchar *pix;

	gdouble (*func)(double) = NULL;



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

	w = gdk_pixbuf_get_width(pb);
	h = gdk_pixbuf_get_height(pb);

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



#define UNOISE (rand()/((double)RAND_MAX + 1.))

/**
 * @brief gaussian noise macro
 */
#define GNOISE (UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE + UNOISE - 6.)



/**
 * @brief returns the value of the width-normalised gaussian for a given FWHM
 */

static double gauss_norm(double x, double fwhm)
{

	/* The FWHM auf a gaussian with sigma == 1 is 2*sqrt(2*ln(2)) ~2.35482
	 * To normalise x, we multiply by the latter and divide by two, since
	 * we moved the factor from the divisor to the dividend.
	 * We need only half of the FWHM, i.e. 1 / (fwhm / 2) which == 2 / fwhm,
	 * so our final scale factor is 2.35482/fwhm
	 */

	x = x / fwhm * 2.*sqrt(2.*log(2.));

	return 1.0 / sqrt(2.0 * M_PI) * exp( -0.5 * x * x);
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
				GtkWindow * win;

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
 * @brief build a gaussian convolution kernel for a beam of radius r
 *
 * @param r the beam radius in degrees
 */
static gpointer gauss_kernel_spec(double r, double glat, double glon)
{
	static double x, x0, x1;
static 	double bins;

	double binw;

	int i, n;

	double s;

static	double intg, prev, tmp, total;

static	double *conv;
static	double *conv2;

	/* our width-normalised kernel runs from -3 to +3 sigma */

	x1 = 3.0 / 2.0 * sqrt(2.0 * log(2.0)) * r;

	x0 = - x1;

	/* need an extra center bin, does not count towards stepsize */
	bins = round((x1 - x0) / SKY_BASE_RES) + 1.0;
	binw = (x1 - x0) / (bins - 1.0);



	if(conv)
	g_free(conv);
	conv = g_malloc(bins * sizeof(double));

	x = x0;
	total = 0.0;
	for (i = 0; i < bins; i++) {
		n = 0;

		s = x - binw * 0.5;

		tmp = 0.0;
		intg = 0.0;
		prev = gauss_norm(s, r);

		do {
			s += SKY_GAUSS_INTG_STP;
			/* simple numerical integration */
			intg += SKY_GAUSS_INTG_STP * 0.5 * ((gauss_norm(s, r) + prev));
			prev = intg;
		} while (s < (x + binw * 0.5));

		conv[i] = prev;

		total += intg;


		x += binw;

	}

	/* oder conv[i] dividieren! */
	gdouble norm = 2.0 * bins * total;


	if (conv2)
	g_free(conv2);
		conv2 = g_malloc(bins * bins * sizeof(double));
		int j;
		for (i = 0; i < (int) bins; i++) 
			for (j = 0; j < (int) bins; j++) {
				conv2[i* (int) bins + j] = (conv[i] + conv[j]) / norm;
			}

	/* strategy: allocate columns * rows in flat array, fill with
	 * convolution kernel rows (2d gaussian has radial symmetry and can be convolve
	 * by application to rows+columns; add data in same fashion, 2d-indexed ragged type:
	 * compute once, apply continuously;
	 * once, convolved, sum along 2d direction for final spectrum
	 *
	 */

	gdouble *res;
	static gint16 *raw;

	double l, b;
	double dl, db;
	gdouble lat, lon;


	res = g_malloc0(VEL * sizeof(double));

	gint idxl=0;
	gint idxb=0;

	/* coordinates needed */
	for (l = x0; l <= x1; l+=binw) {
		idxb = 0;
		for (b = x0; b <= x1; b+=binw) {

			/* get needed coordinate */
			lat = glat + b;
			lon = glon + l;


			/* fold lat into 180 deg range */
			lat = fmod(lat + 90.0, 180.0) - 90.0;

			/* same for lon */
			lon = fmod(lon, 360.0);

			/* the sky is a sphere :) */
			if (lat < -90.0)
				lat = lat + 180.0;
			if (lat > 90.0)
				lat = lat - 180.0;

			if (lon < 0.0)
				lon = lon + 360.0;
			if (lon > 360.0)
				lon = lon - 360.0;

			/* data is reverse? */
			//lon = 360.0 - lon;


			/***
			 * strategy for image creation: sum up vel range first,
			 * then store. Apply convolution to stored data
			 */



			/* map to data grid */
			lat = round(( 1.0 / SKY_BASE_RES ) * lat) * SKY_BASE_RES;
			lon = round(( 1.0 / SKY_BASE_RES ) * lon) * SKY_BASE_RES;


			raw = sim_spec_extract_HI_survey(lat, lon);
			/* get the corresponding coordinate */


			for (i = 0; i < VEL; i++)
				res[i] += ((gdouble) raw[i]) * conv2[idxl * (int) bins + idxb];


			g_free(raw);
			idxb++;
		}
		idxl++;
	}

#if 0
	for (i = 0; i < VEL; i++)
		res[i] /= norm;
#endif

	return res;
}



static void sky_ctransp(complex double *out, complex double *in, int w, int h)
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
 * @brief transpose a two-dimensional array of complex doubles
 */

static void sky_ctransp2(complex double *out, complex double *in, int w, int h)
{
        int i, j;

#pragma omp parallel for
        for (j = 0; j < h; j++) {
#pragma omp parallel for
                for (i = 0; i < w; i++) {
                        out[i * h + j] = in[j * w + i];
		}
	}
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
 */


static gdouble *gauss_conv_kernel(gdouble r, gdouble sigma, gdouble res,
				  gint *n)
{
	gint i, j;
	gint bins;

	gdouble x;
	gdouble x0;
	gdouble x1;

	gdouble binw;

	gdouble tot;
	gdouble norm;

	double *conv;

	double *conv2;



	/* our width-normalised kernel runs from -sigma to +sigma */
	x1 = sigma / 2.0 * sqrt(2.0 * log(2.0)) * r;
	x0 = - x1;
	x  = x0;

	/* need an extra center bin, does not count towards stepsize */
	bins = (gint) (round((x1 - x0) / res));

	if (!(bins & 0x1))
	    bins++;


	binw = (x1 - x0) / ((gdouble) (bins - 1));


	/* 1d convolution profile */
	conv = g_malloc(bins * sizeof(gdouble));


	tot = 0.0;

	for (i = 0; i < bins; i++) {

		gdouble s;
		gdouble intg;
		gdouble prev;

		s = x - binw * 0.5;

		intg = 0.0;
		prev = gauss_norm(s, r);

		/* simple numerical integration */
		do {
			s += SKY_GAUSS_INTG_STP;
			intg += SKY_GAUSS_INTG_STP * 0.5 * ((gauss_norm(s, r) + prev));
			prev = intg;
		} while (s < (x + binw * 0.5));

		conv[i] = prev;

		tot += intg;

		x += binw;
	}


	/* 2d kernel */
	conv2 = g_malloc(bins * bins * sizeof(double));

	norm = 2.0 * tot * bins;

	for (i = 0; i < bins; i++)
		for (j = 0; j < bins; j++)
			conv2[i * bins + j] = (conv[i] + conv[j]) / norm;


	g_free(conv);


	(*n) = bins;

	return conv2;
}


/**
 * @brief returns an NxN normalised gaussian convolution kernel for a given
 *	  sigma cutoff and resolution in degrees
 *
 * @note bins is always uneven (need center bin)
 */


static gdouble *gauss_2d(gdouble r, gdouble sigma, gdouble res,
				  gint *n)
{
	gint i, j;
	gint bins;

	gdouble x;
	gdouble x0;
	gdouble x1;

	gdouble binw;

	gdouble tot;
	gdouble norm;

	double *conv2;



	/* our width-normalised kernel runs from -sigma to +sigma */
	x1 = sigma / 2.0 * sqrt(2.0 * log(2.0)) * r;
	x0 = - x1;
	x  = x0;

	/* need an extra center bin, does not count towards stepsize */
	bins = (gint) (round((x1 - x0) / res));

	if (!(bins & 0x1))
	    bins++;


	binw = (x1 - x0) / ((gdouble) (bins - 1));



	/* 2d kernel */
	conv2 = g_malloc(bins * bins * sizeof(double));
	norm = 2.0 * tot * bins;

	tot = 0.0;

	for (i = 0; i < bins; i++) {
		for (j = 0; j < bins; j++)  {

		gdouble s;
		gdouble intg;
		gdouble prev;

		s = (double)bins / sqrt(((bins/2 - j) * (bins/2 -j)  + (bins/2 - i) * (bins/2 -i)));
//		s = (x - binw * 0.5) / s;
//			 g_message("s2 %g", s);


		conv2[i * bins + j] = 0.5 * ((gauss_norm(s, r)));

		x += binw;
	}}


	(*n) = bins;

	return conv2;
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

static gint sky_put_matrix(double complex *dst, gint dw, gint dh,
			   const double  *src, gint sw, gint sh,
			   gint x, gint y)
{
	gint i, j;
	gint dx, dy;


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

static gint sky_get_matrix(double *dst, gint dw, gint dh,
			   const double complex *src, gint sw, gint sh,
			   gint x, gint y, gint w, gint h)
{
	gint i, j;
	gint sx, sy;


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




static double complex *ft;
static double complex *it;



#define SKY1_WIDTH  1024
#define SKY1_HEIGHT 1024

static gdouble *sky_convolve_dft(const double complex *sky, const double complex *ker, gint n)
{
	double complex *csk;
	double complex *tmp;

	double *img;

	gint i, j;

	GTimer *t;




	tmp = g_malloc(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));
	csk = g_malloc(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));

	img = g_malloc(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double));




	/* 0.0033 */
	/* convolve */
#pragma omp parallel for
   	for (j = 0; j < SKY1_HEIGHT * SKY1_WIDTH; j++)
		csk[j] = sky[j] * ker[j];



	/* 0.008347 */
	/* inverse transform */
#pragma omp parallel for
	for (j = 0; j < SKY1_WIDTH; j++)
		fft2(&csk[j * SKY1_HEIGHT], it, SKY1_HEIGHT, FFT_INVERSE);

	/* 0.009 */
	/* transpose forward */
	sky_ctransp(tmp, csk, SKY1_WIDTH, SKY1_HEIGHT);


	/* 0.008347 */
#pragma omp parallel for
	for (j = 0; j < SKY1_HEIGHT; j++)
		fft2(&tmp[j * SKY1_WIDTH], it, SKY1_WIDTH, FFT_INVERSE);



	/* 0.000505 */
	sky_get_matrix(img, SKY_WIDTH, SKY_HEIGHT, tmp, SKY1_WIDTH, SKY1_HEIGHT, 0, 0, SKY_WIDTH, SKY_HEIGHT);


	g_free(tmp);
	g_free(csk);



	return img;
}


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






static double complex *ker;
static double complex *csky;
static gdouble *msky;
static gdouble *raw_sky;
static gdouble *kernel;

static void sim_gen_sky(void)
{
	int i;

	gdouble lat, lon;
	gdouble sig;
	gdouble *rawspec;

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



#if 1
	/** XXX */
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

	if (!raw_sky) {
		raw_sky = sim_rt_get_HI_img(vmin, vmax);

		if (csky) {
			g_free(csky);
			csky =NULL;
		}

		csky  = g_malloc0(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));
		double complex *tmp = g_malloc0(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));


		sky_put_matrix(csky, SKY1_WIDTH, SKY1_HEIGHT, raw_sky, SKY_WIDTH, SKY_HEIGHT, 0, 0);
#pragma omp parallel for
		for (int j = 0; j < SKY1_HEIGHT; j++)
			fft2(&csky[j * SKY1_WIDTH], ft, SKY1_WIDTH, FFT_FORWARD);

		/* transpose forward */
		sky_ctransp(tmp, csky, SKY1_HEIGHT, SKY1_WIDTH);

#pragma omp parallel for
		for (int j = 0; j < SKY1_WIDTH; j++)
			fft2(&tmp[j * SKY1_HEIGHT], ft, SKY1_HEIGHT, FFT_FORWARD);


		for (int j = 0; j < SKY1_HEIGHT * SKY1_WIDTH; j++)
			csky[j] = tmp[j];
	}


#endif
	kernel = gauss_conv_kernel(sim.r_beam, 3.0, SKY_BASE_RES, &n);
	ker = g_malloc0(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));
#if 0
#endif
	sky_put_matrix(ker, SKY1_WIDTH, SKY1_HEIGHT, kernel, n, n, -n/2, -n/2);
#if 1

	double complex *tmp = g_malloc0(SKY1_WIDTH * SKY1_HEIGHT * sizeof(double complex));
#pragma omp parallel for
	for (int j = 0; j < SKY1_HEIGHT; j++)
		fft2(&ker[j * SKY1_WIDTH], ft, SKY1_WIDTH, FFT_FORWARD);

	/* transpose forward */
	sky_ctransp(tmp, ker, SKY1_HEIGHT, SKY1_WIDTH);

#pragma omp parallel for
	for (int j = 0; j < SKY1_WIDTH; j++)
		fft2(&tmp[j * SKY1_HEIGHT], ft, SKY1_HEIGHT, FFT_FORWARD);


#pragma omp parallel for
   	for (int j = 0; j < SKY1_HEIGHT * SKY1_WIDTH; j++)
		ker[j] = tmp[j];
#endif


	g_timer_start(timer);
#if 0
	if (!msky)
		msky = sky_convolve(raw_sky, kernel, n);
#else

//	if (!msky)
		msky = sky_convolve_dft(csky, ker, n);
#endif

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
static void sim_gui_scale_mode_changed(GtkComboBox *cb, gpointer data)
{
	sim.gui.scale = gtk_combo_box_get_active(cb);
	sim_gui_redraw_sky();
}

static void sim_gui_beam_slide_value_changed(GtkRange *range, gpointer data)
{
	gdouble *val;


	val = (gdouble *) data;

	(*val) = gtk_range_get_value(range);
	gtk_spin_button_set_value(sim.gui.sb_beam, sim.r_beam);

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
}

static gboolean sim_spb_lon_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	struct packet pkt;


	server_cfg_set_station_lon(gtk_spin_button_get_value(sb));

	pkt.trans_id = PKT_TRANS_ID_UNDEF;

	proc_pr_capabilities(&pkt);
}


static gboolean sim_spb_tsys_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.tsys = gtk_spin_button_get_value(sb);
}

static gboolean sim_spb_eff_value_changed_cb(GtkSpinButton *sb, gpointer data)
{
	sim.eff = gtk_spin_button_get_value(sb);
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


	w = gtk_label_new("EFF");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

	w = gtk_spin_button_new_with_range(0., 1.0, 0.1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), SIM_EFF);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_eff_value_changed_cb), NULL);



	w = gtk_label_new("LAT [deg]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 3, 1, 1);

	w = gtk_spin_button_new_with_range(-90., 90., 0.01);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), server_cfg_get_station_lat());
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 3, 1, 1);


	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_lat_value_changed_cb), NULL);

	w = gtk_label_new("LON [deg]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 4, 1, 1);

	w = gtk_spin_button_new_with_range(-180., 180., 0.01);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 2);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), server_cfg_get_station_lon());
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 4, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(sim_spb_lon_value_changed_cb), NULL);



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
	int i;

	struct status st;


	struct spec_data *s = NULL;

	struct coord_horizontal hor;
	struct coord_galactic gal;

	gdouble *rawspec;
	gint16 *rawu;


	//sim_gen_sky();
#if 0
	if (!obs->acq.acq_max)
		return 0;
#endif
	hor.az = sim.az.cur;
	hor.el = sim.el.cur;

	gal = horizontal_to_galactic(hor, server_cfg_get_station_lat(), server_cfg_get_station_lon());
#if 0
	gal.lat = 0;
	gal.lon = 180.0;
#endif

	st.busy = 1;
	st.eta_msec = (typeof(st.eta_msec)) 20.0;	/* whatever */
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);
#if 1

	/* conv */


	int v1 = (int) DOPPLER_VEL(g_obs.acq.freq_stop_hz, SIM_V_REF_MHZ * 1e6) + SIM_V_RED_KMS +2 + vlsr(galactic_to_equatorial(gal), 0.0);
	int v0 = (int) DOPPLER_VEL(g_obs.acq.freq_start_hz, SIM_V_REF_MHZ * 1e6) + SIM_V_RED_KMS + vlsr(galactic_to_equatorial(gal), 0.0);

//	g_message("vmin: %d vmax %d", v0, v1);

	if (v0 < 0)
		v0 = 0;
	if (v1 > VEL)
		v1 = VEL;
//	g_message("vmin: %d vmax %d\n", v0, v1);



	/* prepare and send: allocate full length */
	s = g_malloc0(sizeof(struct spec_data) + (v1 - v0) * sizeof(uint32_t));

	rawspec = gauss_kernel_spec(sim.r_beam, gal.lat, gal.lon);
#if 0
	s->freq_min_hz = (typeof(s->freq_min_hz)) DOPPLER_FREQ((v0 - SIM_V_RED_KMS  ), SIM_V_REF_MHZ * 1e6);
	s->freq_max_hz = (typeof(s->freq_max_hz)) DOPPLER_FREQ((v1 - SIM_V_RED_KMS - 2 ), SIM_V_REF_MHZ * 1e6);
#endif
	s->freq_min_hz = (typeof(s->freq_min_hz)) g_obs.acq.freq_start_hz;
	s->freq_max_hz = (typeof(s->freq_max_hz)) g_obs.acq.freq_stop_hz;
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) SIM_FREQ_STP_HZ;
#if 1
	s->freq_min_hz = round(s->freq_min_hz / s->freq_inc_hz) * s->freq_inc_hz;
	s->freq_max_hz = round(s->freq_max_hz / s->freq_inc_hz) * s->freq_inc_hz;
#endif

        //srand(time(0));
	for (i = v0; i < v1; i++) {
		/* reverse or not? */
		s->spec[i- v0]  = (uint32_t) ((double) rawspec[VEL - i - 1] * 10. * sim.eff + sim.tsys * 1000.); /* to mK fom cK + tsys*/
	//	s->spec[i]  = (uint32_t) rawspec[i] *10 + 200000; /* to mK fom cK + tsys*/
		s->spec[i-v0] += (int) (GNOISE * sqrt((double) s->spec[i-v0] * 15.));

		s->n++;
	}

	g_free(rawspec);



	/* handover for transmission */
	ack_spec_data(PKT_TRANS_ID_UNDEF, s);

#else

	/* noconv */


	/* prepare and send: allocate full length */
	s = g_malloc0(sizeof(struct spec_data) + VEL * sizeof(uint32_t));

	rawu = sim_spec_extract_HI_survey(gal.lat, gal.lon);


	s->freq_min_hz = (typeof(s->freq_min_hz)) DOPPLER_FREQ((SIM_V_RED_KMS + vlsr(galactic_to_equatorial(gal), 0.0)), SIM_V_REF_MHZ * 1e6);
	s->freq_max_hz = (typeof(s->freq_max_hz)) DOPPLER_FREQ((SIM_V_BLU_KMS + vlsr(galactic_to_equatorial(gal), 0.0)), SIM_V_REF_MHZ * 1e6);
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) SIM_FREQ_STP_HZ;

        //srand(time(0));
	for (i = 0; i < VEL; i++) {
		s->spec[i]  = (uint32_t) rawu[VEL - i - 1] * 10  + 300000; /* to mK from cK + tsys*/
	//	s->spec[i] += (int) (GNOISE * 1000.);

		s->n++;
	}

	g_free(rawu);


	/* handover for transmission */
	ack_spec_data(PKT_TRANS_ID_UNDEF, s);

#endif






	st.busy = 0;
	st.eta_msec = 0;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);

	obs->acq.acq_max--;

cleanup:
	g_free(s);

	g_usleep(G_USEC_PER_SEC);

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
