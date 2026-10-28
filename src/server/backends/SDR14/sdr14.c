/**
 * @file    server/backends/SDR14/sdr14.c
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
 * @brief plugin for sdr14 with ftdi_sio kernel module
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gmodule.h>
#include <string.h>

#include <backend.h>
#include <cmd.h>
#include <ack.h>

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <fftw3.h>
#include <math.h>

#include <ad6620.h>

#include <termios.h>


#define MSG "SDR14 SPEC: "


#define SDR14_HDR_LEN	2
/* SDR14 data items are fixed for type 0 data items (I/Q or real samples) */
#define SDR14_DATA0_LEN	4096

struct sdr14_data_pkt {
	uint8_t  hdr[SDR14_HDR_LEN];
	int16_t data[SDR14_DATA0_LEN];
};

/* full ASCP data-item-0 message: 2 byte header + 4096 int16 samples */
#define SDR14_FULL_DATA_LEN	(SDR14_HDR_LEN + SDR14_DATA0_LEN * (int)sizeof(int16_t))

/* ASCP message header fields.  The reference USB parser uses the low
 * 5 bits for the length and treats length 0 as a full 8194-byte data
 * block; follow that here.
 */
#define SDR14_ASCP_LEN_MASK	0x1F
#define SDR14_ASCP_TYPE_MASK	0xE0
#define SDR14_ASCP_TYPE_DATA0	(4 << 5)


#define SDR14_NSAM  2048	/* 2048 16 bit I/Q pairs (in AD66220 mode)  */


/* The AD6620 setup in ad6620.h uses CIC2 = 10, CIC5 = 17 and RCF = 2,
 * i.e. a total decimation of 340 (not 170).  The complex output rate is
 * therefore 66666667 / 340 = 196078 Hz, which is the full complex
 * bandwidth of the DDC output.  <= SDR14_SIDE_DISCARD_HZ are discarded
 * on either side to remove the filter roll-off and DDC effects.
 */
#define SDR14_DECIMATION	(10 * 17 * 2)
#define SDR14_ADC_FREQ		66666667
#define SDR14_RT_BW		((double)SDR14_ADC_FREQ / (double)SDR14_DECIMATION)
#define SDR14_SIDE_DISCARD_HZ	6300

/* XXX the reference SDK from ~2004 swaps I and Q for the raw complex AD6620 stream.
 * possible verifcation with a signal generator: inject a tone
 * slightly above the tuned centre; if it appears below, delete this option
 * entire entirelyy
 */
#define SDR14_SWAP_IQ		1

/* the reference SDK applies a Hann window. Enabling this improves the
 * sidelobe rejection but changes the spectral resolution and the noise
 * bandwidth, so re-measure temp_cal_factor after changing it.
 */
#define SDR14_USE_WINDOW	0

/* remove the per-block I/Q mean before the FFT.  This suppresses the
 * DC/NCO feedthrough without dropping the DC bin from the frequency
 * grid (which would break the uniform bin spacing)
 */
#define SDR14_REMOVE_DC		1

#define IS_OH_MASER

/* XXX need to implement the receiver as separate plugin
 * since the values are fixed for now, I define it here
 */
#ifndef IS_OH_MASER
#define RECV_LO_FREQ		  1413000000
#define SDR14_1_4_CAL		  ( 0.8234 * 4.8 / 2.656 * 0.654 * (M_PI / 4.0))
#else
#define RECV_LO_FREQ		  1606000000
#define SDR14_1_4_CAL		  ( 0.8234 * 4.8 * (M_PI / 4.0))
#endif /* IS_OH_MASER */
#define RECV_IF_HZ		     6500000
#define RECV_IF_BW		    10700000	/* nominal output low pass */

/* default allowed HW ranges */
#define SDR_14_LOW_SKIP_HZ	500000		/* lower bound skip */
#define SDR14_FREQ_MIN_HZ	(RECV_LO_FREQ + SDR_14_LOW_SKIP_HZ)
#define SDR14_FREQ_MAX_HZ	(RECV_LO_FREQ + RECV_IF_BW)
#define SDR14_IF_BW_HZ		SDR14_RT_BW
#define SDR14_DIGITAL_BINS	SDR14_NSAM
#define SDR14_SPEC_STACK_MAX	128		/* internal fifo queue limit for contiguous sampling */
#define SDR14_TUNING_STEP_HZ	1
#define SDR14_BIN_DIV_MAX	6		/* allow resolutions down to
						 * SDR14_RT_BW /
						 * (SDR14_NSAM >> SDR14_BIN_DIV_MAX) HZ
						 */
#ifndef IS_OH_MASER
/* initial receiver configuration */
#define SDR14_INIT_FREQ_START_HZ	1420042187
#define SDR14_INIT_FREQ_STOP_HZ		1420970312
#else

#if 1
       #define SDR14_INIT_FREQ_START_HZ        1611800000
       #define SDR14_INIT_FREQ_STOP_HZ         1612200000
#else
       #define SDR14_INIT_FREQ_START_HZ        (1611800000 + 54000000)
       #define SDR14_INIT_FREQ_STOP_HZ         (1612200000 + 54000000)
#endif
#define SDR14_CAL      SDR14_1_6_CAL

#endif /* IS_OH_MASER */
#define SDR14_INIT_BIN_DIV		         6
#define SDR14_INIT_NSTACK		        64



/**
 * @brief the configuration of the digital spectrometer
 */

static struct {
	double freq_min_hz;		/* lower frequency limit */
	double freq_max_hz;		/* upper frequency limit */

	double freq_inc_hz;		/* PLL tuning step */

	double freq_if_hz;		/* intermediate frequency */

	double freq_if_bw;		/* IF bandwidth */

	int freq_bin_div_max;		/* maximum bandwidth divider */

	int bins;			/* number of bins in a raw spectrum */

	double temp_cal_factor;		/* calibration for temp conversion */

	struct {			/* spectral response calibration */
		gdouble *frq;
		gdouble *amp;
		gsize n;
	} cal;

} sdr14 = {
	 .freq_min_hz      = SDR14_FREQ_MIN_HZ,
	 .freq_max_hz      = SDR14_FREQ_MAX_HZ,
	 .freq_inc_hz      = SDR14_TUNING_STEP_HZ,
	 .freq_if_hz       = RECV_IF_HZ,
	 .freq_if_bw       = SDR14_IF_BW_HZ,
	 .freq_bin_div_max = SDR14_BIN_DIV_MAX,
	 .bins	           = SDR14_DIGITAL_BINS,

	 /* note: the calibration was measured with the old magnitude-averaging
	  * normalisation.  The new PSD normalisation returns
	  * mean(|X|^2) / N, i.e. sigma^2 for noise (independent of the
	  * FFT size) and A^2 * N for a coherent tone.  The old estimator
	  * returned (pi/4) * sigma^2 for noise and A^2 * N for a tone, so
	  * multiply by pi/4 to preserve the old continuum calibration and
	  * remove the pi/4 magnitude-averaging bias.
	  *
	  * XXX delete this once the new normalisation is validated
	  *
	  */
	 .temp_cal_factor   = SDR14_1_4_CAL,
	};


double *reamin0, *reamout0;
fftw_plan p0;

static double window[SDR14_NSAM];
static double window_sum = 1.0;


static char *sdr14_tty = "/dev/ttyUSB1";
static int sdr14_fd;
static gboolean last_acq_mode = TRUE;

/* I'm beginning to suspect that we use too many locks :D */
static GThread *thread;
static GCond	acq_cond;
static GMutex   acq_lock;
static GMutex   acq_abort;
static GMutex   acq_pause;
static GRWLock  obs_rwlock;



/**
 * @brief an observation
 */

struct observation {
	struct spec_acq_cfg acq;
	gsize blsize;
	int disc_raw;
	int disc_fin;
	gsize n_seq;
	double f0;
	double f1;
	double bw_eff;
	double bin_hz;		/* true FFT bin width */
	double freq_first;	/* actual centre of the first retained bin */
};

static struct observation g_obs;



/**
 * @brief flush pending bytes
 */

static void sdr14_serial_flush(int fd)
{
	int n;
	unsigned char c;

	while ((n = read(fd, &c, sizeof(c))) > 0);
}


/**
 * @brief open a serial tty
 * @param tty the path to the tty
 *
 * @return 0 on error, file descriptor otherwise
 */

static int sdr14_serial_open_port(const char *tty)
{
	int fd;


	fd = open(tty, O_RDWR);

	if (fd < 0) {
		perror("unable to open serial port");
		return 0;
	}


	return fd;
}

/**
 * @brief set the serial link parameters for the SDR14 with the ftdi_sio module
 *
 * @param fd the file descriptor of the serial tty
 *
 * @return see man 3 tcsetattr
 */

static int sdr14_serial_set_comm_param(int fd)
{
	struct termios cfg = {0};


	/* we start from a cleared struct, so we need only set our config */
	cfg.c_cflag = CS8 | CLOCAL | CREAD;
        cfg.c_iflag = IGNPAR;

        cfsetispeed(&cfg, B230400);
        cfsetospeed(&cfg, B230400);

        cfg.c_cc[VTIME] = 1;

	/* set configuration */
	return tcsetattr (fd, TCSANOW, &cfg);
}



/**
 * @brief read exactly \a left bytes
 *
 * @return the number of bytes actually read, or -1 on a read error
 */

static ssize_t read_bytes(size_t left, uint8_t *p)
{
	size_t n = 0;

	while (left > 0) {
		ssize_t nr = read(sdr14_fd, p, left);

		if (nr <= 0) {
			if (nr < 0)
				perror("read");
			break;
		}

		n    += (size_t)nr;
		left -= (size_t)nr;
		p    += nr;
	}

	return (ssize_t)n;
}


/**
 * @brief read one complete ASCP message into \a pkt
 *
 * data item 0 blocks are 8194 bytes long and use a zero length field
 * any other message is read into the same buffer and must be skipped by the caller
 *
 * @return the total message length, or -1 on error
 */

static ssize_t sdr14_read_msg(struct sdr14_data_pkt *pkt)
{
	uint8_t hdr[SDR14_HDR_LEN];
	size_t len;
	ssize_t n;


	n = read_bytes(sizeof(hdr), hdr);
	if (n != (ssize_t)sizeof(hdr))
		return -1;

	len = hdr[0] & SDR14_ASCP_LEN_MASK;
	if (len == 0)
		len = SDR14_FULL_DATA_LEN;	/* special case: full data block */

	if (len < sizeof(hdr) || len > sizeof(*pkt))
		return -1;

	memcpy(pkt->hdr, hdr, sizeof(hdr));

	n = read_bytes(len - sizeof(hdr), (uint8_t *)pkt->data);
	if (n != (ssize_t)(len - sizeof(hdr)))
		return -1;

	return (ssize_t)len;
}


static gboolean sdr14_is_data0(const struct sdr14_data_pkt *pkt, ssize_t len)
{
	return (pkt->hdr[1] & SDR14_ASCP_TYPE_MASK) == SDR14_ASCP_TYPE_DATA0 && len == (ssize_t)SDR14_FULL_DATA_LEN;
}

__attribute__((unused))
static void sdr14_get_mode(void)
{
	uint8_t cmd[6] = {0x50, 0x20, 0x18, 0x00, 0x00};

	write(sdr14_fd, cmd, sizeof(cmd));
	read(sdr14_fd, cmd, sizeof(cmd));
}


static void sdr14_setup_ad6620(void)
{
	size_t i;

	uint8_t ack[3];
	uint8_t cmd[9] = {0x09, 0xa0, 0, 0, 0, 0, 0, 0, 0};


	for (i = 0; i < AD6620_DATA_ITEMS; i++) {

		memcpy(&cmd[2],
		       &sdr14_ad6620_data[i * AD6620_DATA_BLKSZ],
		       AD6620_DATA_BLKSZ);

		write(sdr14_fd, cmd, sizeof(cmd));

		/* XXX want timeout here */
		read(sdr14_fd, ack, sizeof(ack));
	}
}



static void sdr14_set_freq(uint32_t hz)
{
	uint8_t smpl[5] = {0x5, 0x20, 0xb0, 0x00, 0x00};

	uint8_t cmd[10] = {0x0a, 0x00, 0x20, 0x00, 0x00, 0xc0, 0x65, 0x52, 0x00, 0x01};
	uint8_t cmd2[6] = {0x06, 0x00, 0x40, 0x00, 0x00, 0x18};
	uint8_t cmd3[6] = {0x06, 0x00, 0x38, 0x00, 0x00, 0x00};
	uint8_t ack[10] = {0};


	write(sdr14_fd, smpl, sizeof(smpl));
	read(sdr14_fd, smpl, sizeof(smpl));


	/* NOTE: the SDR14 expects the frequency in little endian
	 * since this is pretty much the exclusive condition of any platform
	 * we use (including ARM), we don't bother swapping the bytes
	 * explicitly
	 */

	memcpy(&cmd[5], &hz, 4);
	write(sdr14_fd, &cmd[0], 10);
	read_bytes(10, ack);

	write(sdr14_fd, cmd2, sizeof(cmd2));
	read(sdr14_fd, cmd2, sizeof(cmd2));

	write(sdr14_fd, cmd3, sizeof(cmd3));
	read(sdr14_fd, cmd3, sizeof(cmd3));
}


static void fft_init(int n, fftw_plan * p, double **reamin0, double **reamout0)
{
	fftw_complex *in;
	fftw_complex *out;


	in  = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * n);
	out = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * n);

	(*p) = fftw_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

	(*reamin0)  = (double *) in;
	(*reamout0) = (double *) out;
}


static void fft_free(fftw_plan * p, double **reamin0, double **reamout0)
{
	fftw_destroy_plan(*p);
	fftw_free((fftw_complex *) * reamin0);
	fftw_free((fftw_complex *) * reamout0);
}


static void cfft(fftw_plan * p)
{
	fftw_execute(*p);
}


/**
 * @brief fill the FFTW input buffer from one I/Q block
 *
 * @param pkt		the received data packet
 * @param offset	offset (in int16_t words) of the block in pkt->data
 * @param blsize	number of complex samples in the block
 * @param win		window coefficients (may all be 1.0)
 * @param swap_iq	swap the I and Q words before the FFT
 * @param remove_dc	subtract the per-block I/Q mean
 */

static void sdr14_fill_fft_input(const struct sdr14_data_pkt *pkt,
				 gsize offset, gsize blsize,
				 const double *win,
				 gboolean swap_iq, gboolean remove_dc)
{
	gsize i;
	double mean_i = 0.0;
	double mean_q = 0.0;

	if (remove_dc) {
		for (i = 0; i < blsize; i++) {
			double iv = (double)pkt->data[offset + 2 * i];
			double qv = (double)pkt->data[offset + 2 * i + 1];

			if (swap_iq) {
				double t = iv;
				iv = qv;
				qv = t;
			}

			mean_i += iv;
			mean_q += qv;
		}

		mean_i /= (double)blsize;
		mean_q /= (double)blsize;
	}

	for (i = 0; i < blsize; i++) {
		double iv = (double)pkt->data[offset + 2 * i];
		double qv = (double)pkt->data[offset + 2 * i + 1];

		if (swap_iq) {
			double t = iv;
			iv = qv;
			qv = t;
		}

		reamin0[2 * i]     = (iv - mean_i) * win[i];
		reamin0[2 * i + 1] = (qv - mean_q) * win[i];
	}
}


/**
 * @brief computes the observing strategy
 *
 * @param acq the acquisition configuration
 * @param acs the allocated array of observation steps
 *
 */

static void sdr14_comp_obs_strategy(struct observation *obs)
{
	double bin_hz;
	double side_rem;
	double span;
	double n_total;
	double n_keep;

	g_message(MSG "computing acquisition strategy for requested parameters");

	/* we use radix-2 divs */
	obs->blsize = sdr14.bins >> obs->acq.bin_div;
	bin_hz = (double)SDR14_RT_BW / (double)obs->blsize;
	obs->bin_hz = bin_hz;

	/* bins to discard on either side of a "raw" spectrum */
	obs->disc_raw = (int)((double)SDR14_SIDE_DISCARD_HZ / bin_hz);
	if (obs->disc_raw < 1)
		obs->disc_raw = 1;
	if (2 * obs->disc_raw >= (int)obs->blsize) {
		g_warning(MSG "side discard of %d bins too large for %"
			  G_GSIZE_FORMAT " bins, clamping",
			  obs->disc_raw, obs->blsize);
		obs->disc_raw = (int)obs->blsize / 2 - 1;
	}

	/* the first retained bin is not exactly at freq_start: it is freq_start - side_rem */
	side_rem = (double)SDR14_SIDE_DISCARD_HZ - (double)obs->disc_raw * bin_hz;

	obs->bw_eff = (double)SDR14_RT_BW - 2.0 * (double)obs->disc_raw * bin_hz;
	obs->f0 = (double)obs->acq.freq_start_hz - SDR14_SIDE_DISCARD_HZ + (double)SDR14_RT_BW / 2.0;
	obs->f1 = (double)obs->acq.freq_stop_hz + SDR14_SIDE_DISCARD_HZ + (double)SDR14_RT_BW / 2.0;

	span = (double)obs->acq.freq_stop_hz - (double)obs->acq.freq_start_hz;
	obs->n_seq = (gsize)ceil((obs->f1 - obs->f0) / obs->bw_eff);

	/* keep only the bins whose centre lies at or below freq_stop, so
	 * the last reported bin really is the last one inside the
	 * requested range.
	 */
	obs->freq_first = (double)obs->acq.freq_start_hz - side_rem;
	n_total = (double)obs->n_seq * ((double)obs->blsize - 2.0 * (double)obs->disc_raw);
	n_keep = floor((span + side_rem) / bin_hz) + 1.0;

	if (n_keep < 1.0)
		n_keep = 1.0;
	if (n_keep > n_total)
		n_keep = n_total;

	obs->disc_fin = (int)(n_total - n_keep);
	if (obs->disc_fin < 0) {
		g_warning(MSG "discard bins is %d, setting to 0", obs->disc_fin);
		obs->disc_fin = 0;
	}

	g_message(MSG "observation requires acquisition of %" G_GSIZE_FORMAT " raw spectrae", obs->n_seq);
}


/**
 * @brief apply temperature calibration
 *
 * @note milliK is already done in spec_acquire!
 * @note converts data to integer milliKelvins (see payload/pr_spec_data.h)
 *
 * @todo polynomial preamp/inputfilter curve calibration
 */

static void sdr14_apply_temp_calibration(struct spec_data *s)
{
	gsize i;

	for (i = 0; i < s->n; i++) {
		double v = (double)s->spec[i] * sdr14.temp_cal_factor;

		if (!isfinite(v) || v >= (double)UINT32_MAX)
			s->spec[i] = UINT32_MAX;
		else if (v <= 0.0)
			s->spec[i] = 0;
		else
			s->spec[i] = (uint32_t)v;
	}
}


/**
 * @brief acquire spectrea
 *
 u @param acq the acquisition configuration
 * @returns 0 on completion, 1 if more acquisitions are pending
 *
 */
#define AVG_LEN 5.
#define MIN_MS_ACQ_STATUS 500
static uint32_t sdr14_spec_acquire(struct observation *obs)
{
	int i, j, k, l;

	uint8_t oneshot_cmd[8] = {0x08, 0x00, 0x18, 0x00, 0x81, 0x02, 0x02, 0};
#if 0
	uint8_t hbeat[3] = {0x03,0x60,0x00};
#endif
	static double acq_time[SDR14_BIN_DIV_MAX + 1] = {[0 ... SDR14_BIN_DIV_MAX] = 0.001};	/* bin_div starts at 1 */

	GTimer *timer;

	double spec[SDR14_NSAM] = {0};

	struct status s_acq;
	struct status s_rec;

	struct spec_data *s = NULL;
	struct sdr14_data_pkt pkt;

	double freq;
	double scale;
	double fft_count;
	double sum_w;
	double prev_last = 0.0;	/* previous sequence's last output value */

	int len;

	if (!obs->acq.acq_max)
		return 0;


	timer = g_timer_new();


	/* prepare and send: allocate full length */
	len = (int)((obs->blsize - 2 * obs->disc_raw) * obs->n_seq -
		    obs->disc_fin);
	if (len <= 0) {
		g_timer_destroy(timer);
		g_usleep(10000);
		goto noobs;
	}

	s = g_malloc0(sizeof(struct spec_data) + len * sizeof(uint32_t));

	fft_init(obs->blsize, &p0, &reamin0, &reamout0);

	/* precompute the window for this FFT size */
	sum_w = 0.0;
	for (i = 0; i < (int)obs->blsize; i++) {
#if SDR14_USE_WINDOW
		window[i] = 0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)(obs->blsize - 1));
#else
		window[i] = 1.0;
#endif
		sum_w += window[i];
	}
	window_sum = sum_w;

	/* update number of SDR14_NSAM sequences to record in the one shot command */
	oneshot_cmd[7] = obs->acq.n_stack;

	s_rec.busy = 1;
	s_rec.eta_msec = (typeof(s_acq.eta_msec))(acq_time[obs->acq.bin_div] *  1000. * (double)obs->n_seq * (double)obs->acq.n_stack);
	ack_status_rec(PKT_TRANS_ID_UNDEF, &s_rec);


	freq = obs->f0 - RECV_LO_FREQ;
	for (l = 0; l < (int)obs->n_seq; l++) {
	

		sdr14_serial_flush(sdr14_fd);
		sdr14_set_freq(freq);

		freq = freq + obs->bw_eff;

		if (!g_mutex_trylock(&acq_abort)) {
			g_message(MSG "acquisition loop abort indicated");
			goto cleanup;
		}

		g_mutex_unlock(&acq_abort);

		write(sdr14_fd, oneshot_cmd, sizeof(oneshot_cmd));

		/* read the start response as a proper ASCP message */
		if (sdr14_read_msg(&pkt) < 0) {
			g_warning(MSG "failed to read start response");
			goto cleanup;
		}


		for (i = 0; i < (int)obs->blsize; i++)
			spec[i] = 0;

		s_acq.eta_msec = (typeof(s_acq.eta_msec))(acq_time[obs->acq.bin_div] * 1000. * (double)obs->acq.n_stack);
		if (s_acq.eta_msec > MIN_MS_ACQ_STATUS) {
			s_acq.busy = 1;
			ack_status_acq(PKT_TRANS_ID_UNDEF, &s_acq);
		}

		for (k = 0; k < (int)obs->acq.n_stack; k++) {

			int z;
			int skipped = 0;
			ssize_t nread;

			g_timer_start(timer);

			/* Read the next data-item-0 block, skipping any
			 * interleaved status/ack messages.  The device sends
			 * unsolicited RX-state and AD-overload messages while
			 * streaming; consuming those as sample data would
			 * desynchronise the whole stream.
			 */
			while (1) {
				nread = sdr14_read_msg(&pkt);
				if (nread < 0) {
					g_warning(MSG "failed to read data block");
					goto cleanup;
				}

				if (sdr14_is_data0(&pkt, nread))
					break;

				skipped++;

				if (skipped > 16) {
					g_warning(MSG "too many non-data messages, aborting");
					goto cleanup;
				}
			}

			for (z = 0; z < (int)(SDR14_NSAM / obs->blsize); z++) {

				sdr14_fill_fft_input(&pkt,
						     (gsize)z * obs->blsize * 2,
						     obs->blsize,
						     window,
						     SDR14_SWAP_IQ,
						     SDR14_REMOVE_DC);

				cfft(&p0);

				/* plain fftshift: DC at blsize/2, ascending
				 * frequency, no bin removed
				 */
				for (i = 0; i < (int)obs->blsize; i++) {
					double re;
					double im;

					j = (int)(((gsize)i + obs->blsize / 2) % obs->blsize);
					re = reamout0[2 * j];
					im = reamout0[2 * j + 1];
					spec[i] += re * re + im * im;
				}
			}

			g_timer_stop(timer);
			acq_time[obs->acq.bin_div] = (acq_time[obs->acq.bin_div] * (AVG_LEN - 1.0) + g_timer_elapsed(timer, NULL)) / AVG_LEN;
		}

		if (s_acq.eta_msec > MIN_MS_ACQ_STATUS) {
			s_acq.busy = 0;
			s_acq.eta_msec = 0;
			ack_status_acq(PKT_TRANS_ID_UNDEF, &s_acq);
		}

		/* FFTW's forward transform is unnormalised.  Use a PSD
		 * normalisation: multiply the accumulated power by
		 * N / (M * sum(w)^2).  For noise this gives sigma^2,
		 * independent of the FFT size, so the continuum level does
		 * not change with the bin count.  For a coherent tone it
		 * gives A^2 * N (the usual PSD peak scaling).  Without a
		 * window sum(w) = N, so the factor reduces to 1/(M*N).
		 */
		fft_count = (double)obs->acq.n_stack * ((double)SDR14_NSAM / (double)obs->blsize);
		scale = (double)obs->blsize /
			(fft_count * window_sum * window_sum);

		for (i = 0; i < (int)(obs->blsize - 2 * obs->disc_raw); i++) {
			gsize idx = (gsize)i + obs->disc_raw;
			double v;

			if (i == 0) {
				/* The first retained bin sits in the filter
				 * roll-off.  Interpolate it from the previous
				 * sequence's last bin and this sequence's
				 * second bin so the concatenation points do
				 * not show an edge zero/dip.
				 */
				double right = spec[idx + 1] * scale;
				double left = (s->n > 0) ? prev_last : right;

				v = 0.5 * (left + right);
			} else if (idx == obs->blsize / 2) {
				/* The DC bin is suppressed by the mean
				 * subtraction.  Replace it with the average
				 * of its neighbours so the uniform grid does
				 * not contain a zero hole.
				 */
				v = 0.5 * (spec[idx - 1] + spec[idx + 1]) *
				    scale;
			} else {
				v = spec[idx] * scale;
			}

			prev_last = v;

			if (!isfinite(v) || v >= (double)UINT32_MAX)
				s->spec[s->n] = UINT32_MAX;
			else if (v <= 0.0)
				s->spec[s->n] = 0;
			else
				s->spec[s->n] = (uint32_t)v;

			s->n++;
			if (s->n >= (uint32_t)len)
				goto done;
		}

	}


done:
	s->freq_min_hz = (typeof(s->freq_min_hz))llround(obs->freq_first);
	if (s->n > 1) {
		s->freq_max_hz = (typeof(s->freq_max_hz))llround(obs->freq_first + (double)(s->n - 1) * obs->bin_hz);
		/* the payload carries an integer Hz increment, round it rather than truncating;
		 * at the finest resolution the fractional part of the true bin width would otherwise
		 * accumulate to several kHz over a wide observation.
		 */
		s->freq_inc_hz = (typeof(s->freq_inc_hz))llround((double)(s->freq_max_hz - s->freq_min_hz) / (double)(s->n - 1));
	} else {
		s->freq_max_hz = s->freq_min_hz;
		s->freq_inc_hz = 0;
	}

	sdr14_apply_temp_calibration(s);

	/* handover for transmission */
	if (last_acq_mode)
		ack_spec_data(PKT_TRANS_ID_UNDEF, s);



	s_rec.busy = 0;
	s_rec.eta_msec = 0;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &s_rec);

cleanup:
	if (s_rec.busy) {
		s_rec.busy = 0;
		s_rec.eta_msec = 0;
		ack_status_rec(PKT_TRANS_ID_UNDEF, &s_rec);
	}

	g_timer_destroy(timer);

	g_free(s);

	fft_free(&p0, &reamin0, &reamout0);

noobs:
	obs->acq.acq_max--;

	return obs->acq.acq_max;
}


/**
 * @brief check acquisiton paramters for validity
 */

static int sdr14_spec_check_param(struct spec_acq_cfg *acq)
{
	if (acq->freq_start_hz < sdr14.freq_min_hz) {
		g_warning(MSG "start frequency %" G_GUINT64_FORMAT " too low, min %g", acq->freq_start_hz, sdr14.freq_min_hz);
		return -1;
	}

	if (acq->freq_stop_hz > sdr14.freq_max_hz) {
		g_warning(MSG "stop frequency %" G_GUINT64_FORMAT " too high, max %g", acq->freq_stop_hz, sdr14.freq_max_hz);
		return -1;
	}

	if (acq->freq_start_hz > sdr14.freq_max_hz ||
	    acq->freq_stop_hz < sdr14.freq_min_hz) {
		g_warning(MSG "requested frequency range is outside the tunable range");
		return -1;
	}

	if (acq->freq_stop_hz <= acq->freq_start_hz) {
		g_warning(MSG "stop frequency must be greater than start frequency");
		return -1;
	}

	if (acq->bin_div > (uint32_t)sdr14.freq_bin_div_max) {
		g_warning(MSG "bandwidth divider exponent %u too high, max %d", acq->bin_div, sdr14.freq_bin_div_max);
		return -1;
	}

	/* 0 and 1 both mean "no stacking" */
	if (acq->n_stack == 0)
		acq->n_stack = 1;

	if (acq->n_stack > SDR14_SPEC_STACK_MAX) {
		g_warning(MSG "stack count %u too high, max %d", acq->n_stack, SDR14_SPEC_STACK_MAX);
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

static void sdr14_spec_acq_enable(gboolean mode)
{
	/* see if we currently hold the lock */
	if (mode == last_acq_mode) {

		if (g_mutex_trylock(&acq_lock)) {
			g_mutex_unlock(&acq_lock);
			if (!mode)
				ack_spec_acq_disable(PKT_TRANS_ID_UNDEF);
		 } else {
			if (mode)
				ack_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		}

		return;
	}

	last_acq_mode = mode;


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

static gpointer sdr14_spec_thread(gpointer data)
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
#if 1	/* set 0 to annoy squatters by constantly turning off everything */
			run = sdr14_spec_acquire(&g_obs);
#else
			/* disable acquisition on drive power off */
			if (be_drive_pwr_status())
				run = sdr14_spec_acquire(&g_obs);
			else
				run = 0;
#endif

			g_rw_lock_reader_unlock(&obs_rwlock);
		} while (run);


		g_mutex_unlock(&acq_lock);
	}

	return NULL;
}



/**
 * @brief thread function to update the acquisition information
 */

static gpointer sdr14_acquisition_update(gpointer data)
{
	struct observation *obs;


	obs = (struct observation *) data;


	/* wait for mutex lock to indicate abort to a single acquisition cycle
	 * this is needed if a very wide frequency span had been selected
	 */
	g_mutex_lock(&acq_abort);

	g_rw_lock_writer_lock(&obs_rwlock);

	memcpy(&g_obs, obs, sizeof(struct observation));

	g_rw_lock_writer_unlock(&obs_rwlock);

	g_mutex_unlock(&acq_abort);
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

	return NULL;
}





/**
 * @brief configure radio acquisition
 *
 * @returns -1 on error, 0 otherwise
 */

static int sdr14_spec_acquisition_configure(struct spec_acq_cfg *acq)
{
	struct observation *obs;


	if (sdr14_spec_check_param(acq))
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

	sdr14_comp_obs_strategy(obs);

	/* create new thread to update acquisition thread, so we don't
	 * lock down the main loop
	 */
	g_thread_new(NULL, sdr14_acquisition_update, (gpointer) obs);



	return 0;
}

/**
 * @brief set a default configuration
 */

static void sdr14_spec_cfg_defaults(void)
{
	struct observation *obs;

	obs = g_malloc(sizeof(struct observation));
	if (!obs) {
		g_error(MSG "memory allocation failed: %s: %d",
				__func__, __LINE__);
		return;
	}

	obs->acq.freq_start_hz = SDR14_INIT_FREQ_START_HZ;
	obs->acq.freq_stop_hz  = SDR14_INIT_FREQ_STOP_HZ;
	obs->acq.bw_div        = 0;
	obs->acq.bin_div       = SDR14_INIT_BIN_DIV;
	obs->acq.n_stack       = SDR14_INIT_NSTACK;
	obs->acq.acq_max       = ~0;

	sdr14_comp_obs_strategy(obs);

	g_thread_new(NULL, sdr14_acquisition_update, (gpointer) obs);
}




/**
 * @brief spectrum acquisition configuration
 */

G_MODULE_EXPORT
int be_spec_acq_cfg(struct spec_acq_cfg *acq)
{
	if (sdr14_spec_acquisition_configure(acq))
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
	sdr14_spec_acq_enable(mode);

	return 0;
}


/**
 * @brief get telescope spectrometer capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_spec(struct capabilities *c)
{
	c->freq_min_hz		= (uint64_t) sdr14.freq_min_hz;
	c->freq_max_hz		= (uint64_t) sdr14.freq_max_hz;
	c->freq_inc_hz		= (uint64_t) sdr14.freq_inc_hz;
	c->bw_max_hz		= (uint32_t) sdr14.freq_if_bw;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= 0;
	c->bw_max_bins		= (uint32_t) sdr14.bins;
	c->bw_max_bin_div_lin	= 0;
	c->bw_max_bin_div_rad2	= (uint32_t) sdr14.freq_bin_div_max;
	c->n_stack_max		= SDR14_SPEC_STACK_MAX;

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
	c->freq_min_hz		= (uint64_t) sdr14.freq_min_hz;
	c->freq_max_hz		= (uint64_t) sdr14.freq_max_hz;
	c->freq_inc_hz		= (uint64_t) sdr14.freq_inc_hz;
	c->bw_max_hz		= (uint32_t) sdr14.freq_if_bw;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= 0;
	c->bw_max_bins		= (uint32_t) sdr14.bins;
	c->bw_max_bin_div_lin	= 0;
	c->bw_max_bin_div_rad2	= (uint32_t) sdr14.freq_bin_div_max;
	c->n_stack_max		= SDR14_SPEC_STACK_MAX;


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


	g_message(MSG "configuring serial link");

	sdr14_fd = sdr14_serial_open_port(sdr14_tty);
	if (!sdr14_fd)
		g_error(MSG "Error opening serial port %s\n", sdr14_tty);

	if (sdr14_serial_set_comm_param(sdr14_fd))
		g_error(MSG "Error setting parameters for serial port %s\n",
			sdr14_tty);


	sdr14_serial_flush(sdr14_fd);
#if 0
	sdr14_get_mode();
#endif
	sdr14_setup_ad6620();

	g_message(MSG "starting spectrum acquisition thread");

	thread = g_thread_new(NULL, sdr14_spec_thread, NULL);

	/* always start paused */
	sdr14_spec_acq_enable(FALSE);

	sdr14_spec_cfg_defaults();
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
const gchar *g_module_check_init(void)
{

        g_message(MSG "initialising module");


#if 0
	if (srt_spec_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");

	srt_spec_load_calibration();
#endif
	return NULL;
}
