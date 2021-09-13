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
 * @brief plugin for Haystack (old) SRT digital receiver
 *
 * @todo stacking not implemented
 *	 input filter curve calibration not implemented
 *	 configuration acknowledge not implemented
 *	 ???
 */


#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gmodule.h>
#include <string.h>

#include <math.h>
#include <backend.h>
#include <cmd.h>
#include <ack.h>


#define MSG "SRT SPEC: "



#define ARRAY_SIZE(x)	(sizeof(x)/sizeof(x[0]))

/* default allowed HW ranges */
#define SRT_DIGITAL_FREQ_MIN_HZ	1370000000
#define SRT_DIGITAL_FREQ_MAX_HZ	1800000000
#define SRT_DIGITAL_IF_HZ	    800000
#define SRT_DIGITAL_IF_BW_HZ	    500000
#define SRT_DIGITAL_PLL_STEP_HZ	     40000
#define SRT_DIGITAL_BINS		64
#define SRT_DIGITAL_BIN_CUT_LO		 8
#define SRT_DIGITAL_BIN_CUT_HI		 9
#define SRT_DIGITAL_BW_DIV_MAX		 2


/* initial receiver configuration */
#define SRT_INIT_FREQ_START_HZ	1420242187
#define SRT_INIT_FREQ_STOP_HZ	1420570312
#define SRT_INIT_BW_DIV			 0
#define SRT_INIT_BIN_DIV		 0
#define SRT_INIT_NSTACK			 1



/**
 * @brief the configuration of the digital spectrometer
 */
static struct {
	double freq_min_hz;		/* lower frequency limit */
	double freq_max_hz;		/* upper frequency limit */

	double freq_inc_hz;		/* PLL tuning step */

	double freq_if_hz;		/* intermediate frequency */

	double freq_lo_drift_hz;	/* LO frequency drift */

	double freq_if_bw;		/* IF bandwidth */

	int freq_bw_div_max;		/* maximum bandwidth divider */

	/* The IF bandpass filter stop bands are very prominent in the digital
	 * receiver's spectral data. The lo and hi cutoffs select the usable
	 * passband signal. Note: this is due the GC1011A's decimation filter.
	 */

	int bin_cut_lo;
	int bin_cut_hi;

	int bins;			/* number of bins in a raw spectrum */

	double temp_cal_factor;		/* calibration for temp conversion */

	struct {			/* spectral response calibration */
		gdouble *frq;
		gdouble *amp;
		gsize n;
	} cal;

} srt = {
	 .freq_min_hz      = SRT_DIGITAL_FREQ_MIN_HZ,
	 .freq_max_hz      = SRT_DIGITAL_FREQ_MAX_HZ,
	 .freq_inc_hz      = SRT_DIGITAL_PLL_STEP_HZ,
	 .freq_if_hz       = SRT_DIGITAL_IF_HZ,
	 .freq_lo_drift_hz = 0.0,
	 .freq_if_bw       = SRT_DIGITAL_IF_BW_HZ,
	 .freq_bw_div_max  = SRT_DIGITAL_BW_DIV_MAX,
	 .bin_cut_lo       = SRT_DIGITAL_BIN_CUT_LO,
	 .bin_cut_hi       = SRT_DIGITAL_BIN_CUT_HI,
	 .bins	           = SRT_DIGITAL_BINS,
	 .temp_cal_factor   = 2.0,
	};


/**
 * @brief the strategy for raw spectrum acquisition
 */

struct acq_strategy {
	guint32 refdiv;		/* reference divider */
	guint offset;		/* first bin to extract */
	guint nbins;		/* number of bins to extract */
	double *fq;		/* bin frequencies */
	double *cal;		/* spectral response calibration */
	gsize  n_elem;		/* total number of spectral bins */
};


/**
 * @brief an observation
 */

struct observation {
	struct spec_acq_cfg  acq;
	struct acq_strategy *acs;
	gsize  n_acs;
};


/* I'm beginning to suspect that we use too many locks :D */
static GThread *thread;
static GCond	acq_cond;
static GMutex   acq_lock;
static GMutex   acq_abort;
static GMutex   acq_pause;
static GRWLock  obs_rwlock;

static struct observation g_obs;





/**
 * @brief load configuration keys
 */

static void srt_spec_load_keys(GKeyFile *kf)
{
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


	if (srt.freq_min_hz < SRT_DIGITAL_FREQ_MIN_HZ) {
		srt.freq_min_hz = SRT_DIGITAL_FREQ_MIN_HZ;
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


	if (srt.freq_max_hz > SRT_DIGITAL_FREQ_MAX_HZ) {
		srt.freq_max_hz = SRT_DIGITAL_FREQ_MAX_HZ;
		g_message(MSG "adjusted upper frequency limit to %g MHz",
			  srt.freq_max_hz / 1e6);
	}


	srt.freq_lo_drift_hz = g_key_file_get_double(kf, model, "freq_lo_drift_hz", &error);
	if (error)
		g_error(error->message);

	srt.temp_cal_factor = g_key_file_get_double(kf, model, "temp_cal_factor", &error);
	if (error)
		g_error(error->message);


	g_free(model);
}


/**
 * @brief load the srt_spec configuration file from a given prefix
 *
 * @returns 0 on success, otherwise error
 */

static int srt_spec_load_config_from_prefix(const gchar *prefix, GError **err)
{
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;


	gchar *cfg;


	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	cfg = g_strconcat(prefix, "backends/srt_spectrometer.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, err);


	if (!ret) {
		g_key_file_free(kf);
		g_free(cfg);
		return -1;
	}

	g_message(MSG "Configuration file loaded from %s", cfg);

	srt_spec_load_keys(kf);

	g_key_file_free(kf);
	g_free(cfg);

	return 0;
}


/**
 * @brief try to load a srt_spec configuration file from various paths
 */

int srt_spec_load_config(void)
{
	int ret;

	gchar *prefix;

	GError *error = NULL;



	/* search relative path first */
	ret = srt_spec_load_config_from_prefix("/", &error);
	if (ret) {
		g_clear_error(&error);
		/* try again in sysconfdir */
		prefix = g_strconcat(SYSCONFDIR, "/", CONFDIR, "/", NULL);
		ret = srt_spec_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_warning(MSG "Could not find backends/srt_spec.cfg: %s. "
			  "Looked in ./, %s and %s/%s",
			  error->message, CONFDIR, SYSCONFDIR, CONFDIR);
		g_clear_error(&error);

		return -1;
	}

	return 0;
}


/**
 * @brief load spectral response calibration
 *
 * @note format is one per line: <frequency[Mhz]> <amplitude []>
 */

static int srt_spec_load_calibration_from_prefix(const gchar *prefix)
{
	gdouble frq;
	gdouble amp;

	GArray *gfrq;
	GArray *gamp;

	FILE *f;

	gchar *cfg;



	gfrq = g_array_new(FALSE, FALSE, sizeof(gdouble));
	gamp = g_array_new(FALSE, FALSE, sizeof(gdouble));

	cfg = g_strconcat(prefix, "backends/calibration/spectral_response.dat", NULL);
	f = g_fopen(cfg, "r");
	g_free(cfg);

	if (!f) {
		g_warning(MSG "spectral response calibration not found");
		return -1;
	}

	while (fscanf(f,"%lf %lf", &frq, &amp) == 2) {
		frq *= 1e6; /* to Hz */
		g_array_append_val(gfrq, frq);
		g_array_append_val(gamp, amp);
	}

	srt.cal.frq = (gdouble *) gfrq->data;
	srt.cal.amp = (gdouble *) gamp->data;
	srt.cal.n   = gfrq->len;

	g_array_free(gfrq, FALSE);
	g_array_free(gamp, FALSE);

	fclose(f);

	return 0;
}



/**
 * @brief try to load a srt_spec configuration file from various paths
 */

int srt_spec_load_calibration(void)
{
	int ret;

	gchar *prefix;

	GError *error = NULL;



	/* search relative path first */
	ret = srt_spec_load_config_from_prefix("/", &error);
	if (ret) {
		g_clear_error(&error);
		/* try again in sysconfdir */
		prefix = g_strconcat(SYSCONFDIR, "/", CONFDIR, "/", NULL);
		ret = srt_spec_load_calibration_from_prefix(prefix);
		g_free(prefix);
	}

	if (ret) {
		g_warning(MSG "Could not find backends/calibration/spectral_response.dat. "
			  "Looked in ./, %s and %s/%s",
			  CONFDIR, SYSCONFDIR, CONFDIR);
		g_clear_error(&error);

		return -1;
	}

	return 0;
}




/**
 * @brief apply gray chip response curve correction to lower half of spectrum
 *
 * @note bin[0] in the spectral data (after reversal) is the DC component, the
 *	 actual center frequency is hence in bin[32], i.e. the spectral data
 *	 are actually asymmetric around the center
 */

static void srt_spec_graycorr_lo(guint16 *s, gsize len)
{
	gsize i;

	const double cf[] = {
	1.000000, 1.006274, 1.022177, 1.040125, 1.051102, 1.048860, 1.033074,
	1.009606, 0.987706, 0.975767, 0.977749, 0.991560, 1.009823, 1.022974,
	1.023796, 1.011319, 0.991736, 0.975578, 0.972605, 0.986673, 1.012158,
	1.032996, 1.025913, 0.968784, 0.851774, 0.684969, 0.496453, 0.320612,
	0.183547, 0.094424, 0.046729, 0.026470};


	if (len > ARRAY_SIZE(cf))
		len = ARRAY_SIZE(cf);

	for (i = 0; i < len; i++)
		s[i] = (guint16) ((gdouble) s[i]) / cf[i];
}


/**
 * @brief apply gray chip response curve correction to upper half of spectrum
 */

static void srt_spec_graycorr_hi(guint16 *s, gsize len)
{
	gsize i;

	const double cf[] = {
	1.006274, 1.022177, 1.040125, 1.051102, 1.048860, 1.033074, 1.009606,
	0.987706, 0.975767, 0.977749, 0.991560, 1.009823, 1.022974, 1.023796,
	1.011319, 0.991736, 0.975578, 0.972605, 0.986673, 1.012158,1.032996,
	1.025913, 0.968784, 0.851774, 0.684969, 0.496453, 0.320612, 0.183547,
	0.094424, 0.046729, 0.026470, 0.021300};


	if (len > ARRAY_SIZE(cf))
		len = ARRAY_SIZE(cf);

	for (i = 0; i < len; i++)
		s[i] = (guint16) ((gdouble) s[i]) / cf[i];
}


/**
 * @brief apply endianess correction
 * @param s a pointer to the array
 * @param len the number of elements in the array
 */

static void srt_spec_fix_endianess(guint16 *s, gsize len)
{
	gsize i;

	for (i = 0; i < len; i++)
		s[i] = g_ntohs(s[i]);
}


/**
 * @brief reverse a sideband array
 * @param s a pointer to the array
 * @param len the number of elements in the array
 */

static void srt_spec_reverse_sideband(guint16 *s, gsize len)
{
	gsize i;

	guint16 tmp;


	for (i = 0; i < (len / 2); i++) {
		tmp = s[len - i - 1];
		s[len - i - 1] = s[i];
		s[i]  = tmp;
	}
}


/**
 * @brief prepare raw data
 * @param s a pointer to the array
 * @param len the number of elements in the array
 */

static void srt_spec_prepare_raw(guint16 *s, gsize len)
{
	gsize half = len / 2;


	srt_spec_fix_endianess(s, len);

	srt_spec_graycorr_lo(&s[0], len);
	srt_spec_reverse_sideband(&s[0], half);

	srt_spec_reverse_sideband(&s[len / 2], half);
	srt_spec_graycorr_hi(&s[half], len);
}


/**
 * @brief acquire a raw spectrum
 *
 * @param[in]  refdiv the reference divider
 * @param[out] len the number of elements in the raw spectral data
 *
 * @returns an pointer to the spectral data array
 */

static guint16 *srt_spec_acquire_raw(guint32 refdiv, int mode, gsize *len)
{
	gchar *cmd;

	guint16 *response;

	GTimer *timer;

	struct status s;


	/* serial command string */
	cmd = g_strdup_printf("%cfreq%c%c%c%c\n",
			      0,
			      ( refdiv >> 14 ) & 0xff,
			      ( refdiv >>  6 ) & 0xff,
			      ( refdiv       ) & 0x3f,
			      mode);



	timer = g_timer_new();


	be_shared_comlink_acquire();

	s.busy = 1;
	switch (mode) {
		case 0:
			s.eta_msec = (typeof(s.eta_msec)) 2389.112;
			break;
		case 1:
			s.eta_msec = (typeof(s.eta_msec)) 2908.880;
			break;
		case 2:
			s.eta_msec = (typeof(s.eta_msec)) 3957.814;
			break;
	}

	ack_status_acq(PKT_TRANS_ID_UNDEF, &s);

	g_timer_start(timer);
	/* give size explicitly, as command starts with '\0' */
	be_shared_comlink_write(cmd, 9);

	/* actual raw data is 16 bit unsigned @128 bytes total */
	response = (guint16 *) be_shared_comlink_read(len);
	g_timer_stop(timer);

	s.busy = 0;
	s.eta_msec = 0;
	ack_status_acq(PKT_TRANS_ID_UNDEF, &s);

	be_shared_comlink_release();


	g_message(MSG "raw spectrum acquisition time: %f sec %d bwdiv",
		     g_timer_elapsed(timer, NULL), g_timer_elapsed(timer, NULL), mode);

	g_timer_destroy(timer);

	/* len is in bytes, we need elements */
	(*len) /= sizeof(typeof(*response));

	if ((*len) != SRT_DIGITAL_BINS) {
		g_warning(MSG "returned spectral data length was %d "
			  "when %d was expected.", (*len), SRT_DIGITAL_BINS);
	}


	g_free(cmd);

	return response;
}


/**
 * @brief get actual bandwidth given the current configuration
 *
 * @param bw_div a bandwidth divider exponent
 *
 * @return the usable bandwidth in HZ
 */
__attribute__((unused))
static double srt_get_actual_bw(guint bw_div)
{
	double used_bins;
	double bw = 0.0;


	if (bw_div > srt.freq_bw_div_max) {
		g_warning(MSG "selected bandwidth divider exponent %d not"
			      " supported, maximum value is %d.",
			      bw_div, srt.freq_bw_div_max);
		return 0.0;
	}


	used_bins = (double) srt.bins - srt.bin_cut_lo - srt.bin_cut_hi;

	bw = used_bins * srt.freq_if_bw / (double) (srt.bins * (1 << bw_div));


	return bw;
}


/**
 * @brief get the bandwidth of a single spectral bin
 *
 * @param bw_div a bandwidth divdier exponent
 *
 * @return the bandwidth of a bin in HZ
 */

static double srt_get_bin_bw(guint bw_div)
{
	double bw;


	if (bw_div > srt.freq_bw_div_max) {
		g_warning(MSG "selected bandwidth divider exponent %d not"
			      " supported, maximum value is %d.",
			      bw_div, srt.freq_bw_div_max);

		return 0.0;
	}

	bw = srt.freq_if_bw / (double) (srt.bins * (1 << bw_div));

	return bw;
}


/**
 * @brief calculate the maximum PLL reference divider allowed for a given
 *	  frequency and bandwith divider so that the selected frequency bin is
 *	  guaranteed to be contained within the lower sideband of a single
 *	  spectrum
 *
 * @param freq the frequency in HZ
 * @param bw_div a bandwidth divdier exponent
 *
 * @return the reference divider
 */

static guint32 srt_get_refdiv_low(double freq, int bw_div)
{
	double f_0;
	double f_LO;
	double bin_bw;

	guint32 refdiv;


	bin_bw = srt_get_bin_bw(bw_div);

	/* the non-quantized frequency of the local oscillator */
	f_LO = freq + srt.freq_if_hz + srt.freq_lo_drift_hz;

	/* minimum frequency for bins left of center */
	f_0 = f_LO + bin_bw * ((srt.bins / 2 - 1) - srt.bin_cut_lo);

	/* calculate fractional PLL divider and round up */
	refdiv = (guint32) floor(f_0 / srt.freq_inc_hz);

	return refdiv;
}


/**
 * @brief calculate the minimum PLL reference divider needed for a given
 *	  frequency and bandwith divider so that the selected frequency bin is
 *	  guaranteed to be contained within the upper sideband of a single
 *	  spectrum
 *
 * @param freq the frequency in HZ
 * @param bw_div a bandwidth divdier exponent
 *
 * @return the reference divider
 */
__attribute__((unused))
static guint32 srt_get_refdiv_high(double freq, int bw_div)
{
	double f_0;
	double f_LO;
	double bin_bw;

	guint32 refdiv;


	bin_bw = srt_get_bin_bw(bw_div);

	/* the non-quantized frequency of the local oscillator */
	f_LO = freq + srt.freq_if_hz + srt.freq_lo_drift_hz;

	/* minimum frequency for bins right of center */
	f_0 = f_LO - bin_bw * ((srt.bins / 2) - srt.bin_cut_hi);

	/* calculate fractional PLL divider and round up */
	refdiv = (guint32) ceil(f_0 / srt.freq_inc_hz);

	return refdiv;
}


/**
 * @brief get the observed center frequency for a given reference divider
 *
 * @note this incorporates any configured drift in the L.O. frequency
 */

static double srt_get_cfreq(guint32 refdiv)
{
	double f_LO;


	f_LO = (double) refdiv * srt.freq_inc_hz;

	return f_LO - srt.freq_lo_drift_hz - srt.freq_if_hz;
}


/**
 * @brief get the frequency of the first usable bin for a given refdiv
 *
 * @param refdiv a reference divider
 *
 * @param bw_div a bandwidth divdier exponent
 */

static double srt_get_min_freq(guint32 refdiv, guint bw_div)
{
	double bins;

	/* bin[0] == DC, so center is shifted by 1 */
	bins = (double) ((srt.bins / 2 - 1) - srt.bin_cut_lo);

	return srt_get_cfreq(refdiv) - bins * srt_get_bin_bw(bw_div);
}


/**
 * @brief get the frequency of the last usable bin for a given refdiv
 *
 * @param refdiv a reference divider
 *
 * @param bw_div a bandwidth divdier exponent
 */

static double srt_get_max_freq(guint32 refdiv, guint bw_div)
{
	double bins;


	bins = (double) ((srt.bins / 2) - srt.bin_cut_hi);

	return srt_get_cfreq(refdiv) + bins * srt_get_bin_bw(bw_div);
}


/**
 * @brief get the frequency of a single bin
 *
 * @param refdiv a reference divider
 *
 * @param bw_div a bandwidth divdier exponent
 *
 * @param bin the index of the bin
 *
 * @returns the frequency of the bin in Hz or 0.0 on error
 */
__attribute__((unused))
static double srt_get_bin_freq(guint32 refdiv, guint bw_div, guint bin)
{
	double f_0;
	double f_inc;


	if (bin >= srt.bins) {
		g_warning(MSG "selected bin [%d] is invalid,"
			      " maximum index is [%d].", bin, (srt.bins - 1));
		return 0.0;
	}

	f_inc = srt_get_bin_bw(bw_div);

	f_0 = srt_get_cfreq(refdiv) - (srt.bins / 2 - 1) * f_inc;

	return f_0 + bin * f_inc;
}


/**
 * @brief allocate and fill an array of frequencies for a given refdiv
 *        and bw_div in the usable spectral bin range
 *
 * @param refdiv a reference divider
 *
 * @param bw_div a bandwidth divdier exponent
 *
 * @param the frequency array return pointer
 *
 * @returns the  length of the frequency array
 */

static gsize srt_get_frequencies(guint32 refdiv, guint bw_div, gdouble **p)
{
	gsize i;
	gsize nbins;

	double f_0;
	double f_inc;


	f_0 = srt_get_min_freq(refdiv, bw_div);

	f_inc = srt_get_bin_bw(bw_div);

	nbins = srt.bins - srt.bin_cut_lo - srt.bin_cut_hi;

	(*p) = g_malloc(nbins * sizeof(gdouble));

	if (!(*p))
		return 0;


	for (i = 0; i < nbins; i++)
		(*p)[i] = f_0 + i * f_inc;

	return nbins;
}


/**
 * @brief determine reference dividers for an observation
 *
 * @param acq the acquisition configuration
 *
 * @param acs the allocated array of observation steps
 *
 * @returns the number of steps
 */

static gsize srt_determine_refdivs(struct spec_acq_cfg  *acq,
				   struct acq_strategy **acs)
{
	guint rd;
	guint rd0;
	guint rd1;

	gsize n = 1;

	double fmax;

	struct acq_strategy *p;


	if (!acs)
		return 0;

	/* allocate initial */
	p = g_malloc(sizeof(struct acq_strategy));
	if (!p) {
		g_error(MSG "memory allocation failed: %s: %d",
			__func__, __LINE__);
		return 0;
	}

	/* upper/lower bounds of reference dividers */
	rd0 = srt_get_refdiv_low((double) acq->freq_start_hz, acq->bw_div);
	rd1 = srt_get_refdiv_low((double) acq->freq_stop_hz,  acq->bw_div);

	/* the upper bound frequency may be available in a lower reference
	 * divider, which means that the particular frequency must also be
	 * available in the raw spectrum of our lower bound reference divider
	 */
	if (rd1 < rd0)
		rd1 = rd0;

	g_message(MSG "Input range: %u - %u Hz, refdiv range [%u,%u]",
		       acq->freq_start_hz, acq->freq_stop_hz, rd0, rd1);



	/* set the initial divider */
	p[0].refdiv = rd0;

	fmax = srt_get_max_freq(rd0, acq->bw_div);

	for (rd = rd0; rd < rd1; rd++) {

		if (srt_get_min_freq(rd, acq->bw_div) < fmax)
			if (srt_get_min_freq(rd + 1, acq->bw_div) < fmax)
				continue;

		p = g_realloc(p, (n + 1) * sizeof(struct acq_strategy));
		if (!p) {
			g_error(MSG "memory allocation failed: %s: %d",
			__func__, __LINE__);
			return 0;
		}

		p[n].refdiv = rd;

		n++;

		fmax = srt_get_max_freq(rd, acq->bw_div);
	}

	/* see if we really need rd1 for the stop frequency */
	if ((double) acq->freq_stop_hz > fmax) {

		p = g_realloc(p, (n + 1) * sizeof(struct acq_strategy));
		if (!p) {
			g_error(MSG "memory allocation failed: %s: %d",
				__func__, __LINE__);
			return 0;
		}

		p[n].refdiv = rd;

		n++;
	}

	g_message(MSG "observation requires acquisition of %u raw spectrae", n);

	/* assign strategy */
	(*acs) = p;

	return n;
}


/**
 * @brief determine the bin frequencies of the raw spectrae to be recorded
 *
 * @param acq the acquisition configuration
 * @param acs the array of observation steps
 * @param n the number of steps
 */

static void srt_calculate_bin_frequencies(struct spec_acq_cfg  *acq,
					  struct acq_strategy **acs, gsize n)
{
	gsize i;

	struct acq_strategy *p = (*acs);


	for (i = 0; i < n; i++) {
		p[i].n_elem = srt_get_frequencies(p[i].refdiv, acq->bw_div,
						  &p[i].fq);
	}
}


/**
 * @brief find the best match in the calibration data for a given frequency
 *
 * @param frq a frequency (in Hz)
 *
 * @returns the calbration; always 1.0 if no match was found
 */

static gdouble srt_find_calibration(gdouble frq)
{
	gdouble cal = -1.0;

	gsize idx0;

	static gsize idx = 1;


	idx0 = idx;

	for (; idx < srt.cal.n - 1; idx++) {
		if (srt.cal.frq[idx - 1] < frq) {
			if (srt.cal.frq[idx + 1] > frq) {
				cal = srt.cal.amp[idx];
				break;
			}
		}
	}

	/* try again for remaining section */
	if ((cal == -1.0)) {
		for (idx = 1; idx < idx0 - 1; idx++) {
			if (srt.cal.frq[idx - 1] < frq) {
				if (srt.cal.frq[idx + 1] > frq) {
					cal = srt.cal.amp[idx];
					break;
				}
			}
		}
	}

	/* still noting, set unity */
	if (cal == -1.0)
		cal = 1.0;


	/* next call might be close by, decrement one */
	if (idx > 1)
		idx--;


	return cal;
}



/**
 * @brief determine the bin calibration value for the given frequencies
 *
 * @param acq the acquisition configuration
 * @param acs the array of observation steps
 * @param n the number of steps
 */

static void srt_determine_bin_calibration(struct spec_acq_cfg  *acq,
					  struct acq_strategy **acs, gsize n)
{
	gsize i, j;

	struct acq_strategy *p = (*acs);


	/* do nothing if there is no calibration data loaded */
	if (!srt.cal.n) {
		p[i].cal = NULL;
		return;
	}

	for (i = 0; i < n; i++) {
		p[i].cal = (gdouble*) g_malloc(p[i].n_elem * sizeof(gdouble));

		for (j = 0; j < p[i].n_elem; j++)
			p[i].cal[j] = srt_find_calibration(p[i].fq[j]);
	}
}


/**
 * @brief determine the selections of bins to construct the requested spectrum
 *
 * @param acq the acquisition configuration
 * @param acs the array of observation steps
 * @param n the number of steps
 */

static void srt_determine_bin_selection(struct spec_acq_cfg  *acq,
					struct acq_strategy **acs, gsize n)
{
	gsize i;

	double f_max_prev;


	struct acq_strategy *p = (*acs);



	/* first spectrum, lower bound frequency bin */

	p[0].offset = 0;

	while (p[0].fq[p[0].offset] < (double) acq->freq_start_hz)
		p[0].offset++;

	p[0].nbins = p[0].n_elem - p[0].offset;


	/* any in-between spectrae */

	for (i = 1; i < (n - 1); i++) {

		f_max_prev = p[i - 1].fq[p[i - 1].n_elem - 1];

		p[i].offset = 0;

		while (f_max_prev > p[i].fq[p[i].offset])
			p[i].offset++;

		p[i].nbins = p[i].n_elem - p[i].offset;
	}


	/* last spectrum, upper bound frequency bin */

	/* need different stop for more than 1 acquisition */
	if (n > 1) {
		f_max_prev = p[n - 2].fq[p[n - 2].n_elem - 1];

		p[n - 1].offset = 0;

		while (f_max_prev > p[n - 1].fq[p[n - 1].offset])
			p[n - 1].offset++;
	}

	/* last bin */
	p[n - 1].nbins = 0;

	while ((p[n - 1].fq[p[n - 1].offset + p[n - 1].nbins]) <
	       (double) acq->freq_stop_hz) {
		p[n - 1].nbins++;
	}


	for (i = 0; i < n; i++)
		g_message(MSG "SPEC[%u] bins: %u, offset %u, selecting %u",
			       i, p[i].n_elem, p[i].offset, p[i].nbins);
}


/**
 * @brief computes the observing strategy
 *
 * @param acq the acquisition configuration
 * @param acs the allocated array of observation steps
 *
 * @returns the number of steps
 *
 * XXX there is a bug here where it is possible that zero bins are used from
 *     the last refdiv recorded. This affects only performance, but should still
 *     be addressed.
 */

static gsize srt_comp_obs_strategy(struct spec_acq_cfg  *acq,
				   struct acq_strategy **acs)
{
	gsize n;



	if (!acs)
		return 0;

	g_message(MSG "computing acquisition strategy for requested parameters");


	n = srt_determine_refdivs(acq, acs);

	srt_calculate_bin_frequencies(acq, acs, n);

	srt_determine_bin_calibration(acq, acs, n);

	srt_determine_bin_selection(acq, acs, n);

	return n;
}


/**
 * @brief deallocate a stuct acq_strategy
 */

static void srt_comp_obs_strategy_dealloc(struct acq_strategy **acs, gsize n)
{
	gsize i;


	for (i = 0; i < n; i++) {
		g_free((*acs)[i].fq);
		g_free((*acs)[i].cal);
	}


	g_free((*acs));
}


/**
 * @brief apply temperature calibration
 *
 * @note converts data to integer milliKelvins (see payload/pr_spec_data.h)
 *
 * @todo polynomial preamp/inputfilter curve calibration
 */

static void srt_apply_temp_calibration(struct spec_data *s)
{
	gsize i;


	for (i = 0; i < s->n; i++)
		s->spec[i] = (uint32_t) ((double) s->spec[i] * 1000.0) * srt.temp_cal_factor;

}

/**
 * @brief acquire spectrea
 *
 * @param acq the acquisition configuration
 * @returns 0 on completion, 1 if more acquisitions are pending
 *
 */

static uint32_t srt_spec_acquire(struct observation *obs)
{
	int i, j;


	struct status st;



	const struct acq_strategy *acs = obs->acs;
	const gsize n                  = obs->n_acs;

	guint16 **raw = NULL;
	gsize    len;
	gsize    total = 0;


	struct spec_data *s = NULL;

	uint32_t *p;


	if (!obs->acq.acq_max)
		return 0;


	raw = g_malloc0(n * sizeof(*raw));
	if (!raw) {
		g_error(MSG "memory allocation of %d elements failed: %s:%d",
			n, __func__, __LINE__);
		return 0;
	}


	st.busy = 1;
	switch (obs->acq.bw_div) {
		case 0:
			st.eta_msec = (typeof(st.eta_msec)) 2389.112;
			break;
		case 1:
			st.eta_msec = (typeof(st.eta_msec)) 2908.880;
			break;
		case 2:
			st.eta_msec = (typeof(st.eta_msec)) 3957.814;
			break;
	}

	st.eta_msec *= n;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);

	for (i = 0; i < n; i++) {

#if 1
		st.busy = 1;
		switch (obs->acq.bw_div) {
			case 0:
				st.eta_msec = (typeof(st.eta_msec)) 2389.112;
				break;
			case 1:
				st.eta_msec = (typeof(st.eta_msec)) 2908.880;
				break;
			case 2:
				st.eta_msec = (typeof(st.eta_msec)) 3957.814;
				break;
		}

		st.eta_msec *= (n - i);
		ack_status_rec(PKT_TRANS_ID_UNDEF, &st);
#endif

		if (!g_mutex_trylock(&acq_abort)) {
			g_message(MSG "acquisition loop abort indicated");
			goto cleanup;
		}

		g_mutex_unlock(&acq_abort);

		raw[i] = srt_spec_acquire_raw(acs[i].refdiv,
					      obs->acq.bw_div, &len);

		if (len != SRT_DIGITAL_BINS) {
			g_message(MSG "raw data size mismatch in %s: %d "
				       "expected %u got %u", __func__, __LINE__,
				       SRT_DIGITAL_BINS, len);
			goto cleanup;
		}

		srt_spec_prepare_raw(raw[i], len);

		total += len;
	}



	/* prepare and send: allocate full length */
	s = g_malloc0(sizeof(struct spec_data) + total * sizeof(uint32_t));

	s->freq_min_hz = (typeof(s->freq_min_hz)) acs[0].fq[acs[0].offset];
	s->freq_max_hz = (typeof(s->freq_max_hz)) acs[n - 1].fq[acs[n - 1].offset + acs[n - 1].nbins];
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) srt_get_bin_bw(obs->acq.bw_div);


	/* construct final spectrum from the selected bins of each raw spectrum */
	p = s->spec;
	for (i = 0; i < n; i++) {
		for (j = 0; j < acs[i].nbins; j++) {
			/* XXX YUCK! I do this here for now, but the spectrum
			 * preparation really needs some refactoring...
			 */
			(*p) = (uint32_t)  raw[i][j + srt.bin_cut_lo + acs[i].offset];
#if 1
			if (acs[i].cal) 
				(*p) = (uint32_t) (acs[i].cal[j] * (gdouble) (*p));
#endif
			p++;

			s->n++;
		}
	}

	srt_apply_temp_calibration(s);

	/* handover for transmission */
	ack_spec_data(PKT_TRANS_ID_UNDEF, s);

	st.busy = 0;
	st.eta_msec = 0;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &st);

	obs->acq.acq_max--;

cleanup:

	for (i = 0; i < n; i++)
		g_free(raw[i]);

	g_free(raw);
	g_free(s);


	return obs->acq.acq_max;
}


/**
 * @brief check acquisiton paramters for validity
 */

static int srt_spec_check_param(struct spec_acq_cfg *acq)
{
	if (acq->freq_start_hz < srt.freq_min_hz) {
		g_warning(MSG "start frequency %d too low, min %d",
			  acq->freq_start_hz, srt.freq_min_hz);
		return -1;
	}

	if (acq->freq_stop_hz > srt.freq_max_hz) {
		g_warning(MSG "stop frequency %d too low, max %d",
			  acq->freq_stop_hz, srt.freq_max_hz);
		return -1;
	}

	if (acq->bw_div > srt.freq_bw_div_max) {
		g_warning(MSG "bandwidth divider exponent %d too high, max %d",
			  acq->bw_div, srt.freq_bw_div_max);
		return -1;
	}

	if (!acq->acq_max) {
		/* we could add a meaximum limit in a configuration file entry,
		 * for now, we'll set this to the full numeric range of the data
		 * type
		 */
	        acq->acq_max = ~0;
		g_message(MSG "number of acquisitions specified as 0, assuming "
			      "perpetuous acquisition is requested, setting to "
			      "%u", acq->acq_max);
	}


	return 0;
}


/**
 * @brief pause/unpause radio acquisition
 *
 */

static void srt_spec_acq_enable(gboolean mode)
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

static gpointer srt_spec_thread(gpointer data)
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
			run = srt_spec_acquire(&g_obs);
			g_rw_lock_reader_unlock(&obs_rwlock);
		} while (run);


		g_mutex_unlock(&acq_lock);
	}
}


/**
 * @brief thread function to update the acquisition information
 */

static gpointer srt_acquisition_update(gpointer data)
{
	struct observation *obs;


	obs = (struct observation *) data;


	/* wait for mutex lock to indicate abort to a single acquisition cycle
	 * this is needed if a very wide frequency span had been selected
	 */
	g_mutex_lock(&acq_abort);

	g_rw_lock_writer_lock(&obs_rwlock);

	memcpy(&g_obs.acq, &obs->acq, sizeof(struct spec_acq_cfg));

	srt_comp_obs_strategy_dealloc(&g_obs.acs, g_obs.n_acs);

	g_obs.n_acs = obs->n_acs;
	g_obs.acs   = obs->acs;

	g_rw_lock_writer_unlock(&obs_rwlock);

	g_mutex_unlock(&acq_abort);

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
 * @brief configure radio acquisition
 *
 * @returns -1 on error, 0 otherwise
 */

static int srt_spec_acquisition_configure(struct spec_acq_cfg *acq)
{
	struct observation *obs;


	if (srt_spec_check_param(acq))
		return -1;

	g_message(MSG "configuring spectrum acquisition to "
		      "FREQ range: %g - %g MHz, BW div: %u, BIN div %u,"
		      "STACK: %u, ACQ %u",
		      acq->freq_start_hz / 1e6,
		      acq->freq_stop_hz / 1e6,
		      acq->bw_div,
		      acq->bin_div,
		      acq->n_stack,
		      acq->acq_max);


	obs = g_malloc(sizeof(struct observation));
	if (!obs) {
		g_error(MSG "memory allocation failed: %s: %d",
			__func__, __LINE__);
		return -1;
	}


	memcpy(&obs->acq, acq, sizeof(struct spec_acq_cfg));

	obs->n_acs = srt_comp_obs_strategy(&obs->acq, &obs->acs);

	/* create new thread to update acquisition thread, so we don't
	 * lock down the main loop
	 */
	g_thread_new(NULL, srt_acquisition_update, (gpointer) obs);


	/* XXX: send return packet with actual configuration */
#if 0
	s->freq_min_hz = (typeof(s->freq_min_hz)) obs->acs[0].fq[acs[0].offset];
	s->freq_max_hz = (typeof(s->freq_max_hz)) ops->acs[n - 1].fq[obs->acs[n - 1].offset + obs->acs[n - 1].nbins];
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) srt_get_bin_bw(obs->acq.bin_bw);
#endif


	return 0;
}


/**
 * @brief set a default configuration
 */

static void srt_spec_cfg_defaults(void)
{
	struct observation *obs;


	obs = g_malloc(sizeof(struct observation));
	if (!obs) {
		g_error(MSG "memory allocation failed: %s: %d",
			__func__, __LINE__);
		return;
	}

	obs->acq.freq_start_hz = SRT_INIT_FREQ_START_HZ;
	obs->acq.freq_stop_hz  = SRT_INIT_FREQ_STOP_HZ;
	obs->acq.bw_div        = SRT_INIT_BW_DIV;
	obs->acq.bin_div       = SRT_INIT_BIN_DIV;
	obs->acq.n_stack       = SRT_INIT_NSTACK;
	obs->acq.acq_max       = ~0;

	obs->n_acs = srt_comp_obs_strategy(&obs->acq, &obs->acs);

	g_thread_new(NULL, srt_acquisition_update, (gpointer) obs);
}


/**
 * @brief spectrum acquisition configuration
 */

G_MODULE_EXPORT
int be_spec_acq_cfg(struct spec_acq_cfg *acq)
{
	if (srt_spec_acquisition_configure(acq))
		return -1;

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
	srt_spec_acq_enable(mode);

	return 0;
}


/**
 * @brief get telescope spectrometer capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_spec(struct capabilities *c)
{
	c->freq_min_hz		= (uint64_t) srt.freq_min_hz;
	c->freq_max_hz		= (uint64_t) srt.freq_max_hz;
	c->freq_inc_hz		= (uint64_t) srt.freq_inc_hz;
	c->bw_max_hz		= (uint32_t) srt.freq_if_bw;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= (uint32_t) srt.freq_bw_div_max;
	c->bw_max_bins		= (uint32_t) srt.bins;
	c->bw_max_bin_div_lin	= 0;
	c->bw_max_bin_div_rad2	= 0;
	c->n_stack_max		= 0; /* stacking not implemented */

	return 0;
}


/**
 * @brief get telescope spectrometer capabilities_load
 * @note this is identical to be_get_capabilities_load, as
 *	 the hot load is part of the SRT's drive controller
 */

G_MODULE_EXPORT
int be_get_capabilities_load_spec(struct capabilities_load *c)
{
	c->freq_min_hz		= (uint64_t) srt.freq_min_hz;
	c->freq_max_hz		= (uint64_t) srt.freq_max_hz;
	c->freq_inc_hz		= (uint64_t) srt.freq_inc_hz;
	c->bw_max_hz		= (uint32_t) srt.freq_if_bw;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= (uint32_t) srt.freq_bw_div_max;
	c->bw_max_bins		= (uint32_t) srt.bins;
	c->bw_max_bin_div_lin	= 0;
	c->bw_max_bin_div_rad2	= 0;
	c->n_stack_max		= 0; /* stacking not implemented */

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

	thread = g_thread_new(NULL, srt_spec_thread, NULL);

	/* always start paused */
	srt_spec_acq_enable(FALSE);

	srt_spec_cfg_defaults();
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
const gchar *g_module_check_init(void)
{

        g_message(MSG "initialising module");

	if (srt_spec_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");

	srt_spec_load_calibration();

	return NULL;
}
