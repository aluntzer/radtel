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


#define SDR14_NSAM  2048	/* 2048 16 bit I/Q pairs (in AD66220 mode)  */


/* when using the AD6620 modes, the total decimation is 170, see
 * M_CICx in the sdr14_ad6620_data setup block
 * this results in an effective real-time bandwidth of
 * 66666667 / 170 / 2 = 196078 Hz of which <= 6300 Hz are discarded on
 * either side of the spectrum to remove effect of the filter curve and
 * digital down conversion effects
 */
#define SDR14_DECIMATION	(10 * 17)
#define SDR14_ADC_FREQ		66666667
#define SDR14_RT_BW		(SDR14_ADC_FREQ / SDR14_DECIMATION / 2)
#define SDR14_SIDE_DISCARD_HZ	6300

//#define IS_OH_MASER

/* XXX need to implement the receiver as separate plugin
 * since the values are fixed for now, I define it here
 */
#ifndef IS_OH_MASER
#define RECV_LO_FREQ		  1413000000
#else
#define RECV_LO_FREQ		  1606000000
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
						 * SDR14_RT_BW / SDR14_NSAM / SDR14_BW_DIV_MAX HZ
						 */
#ifndef IS_OH_MASER
/* initial receiver configuration */
#define SDR14_INIT_FREQ_START_HZ	1420042187
#define SDR14_INIT_FREQ_STOP_HZ		1420970312
#else
#define SDR14_INIT_FREQ_START_HZ	1611800000
#define SDR14_INIT_FREQ_STOP_HZ		1612200000
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
	 .temp_cal_factor   = 0.8234 * 4.8,
	};


double *reamin0, *reamout0;
fftw_plan p0;


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



static int read_bytes(size_t left, uint8_t *p)
{
	size_t n = 0;
	while (left > 0)
	{
		ssize_t nr = read(sdr14_fd, p, left);
		if (nr <= 0) 	{
			if (nr < 0)
				perror("read");  /* Error! */
			break;
		}
		else {
			n += nr;     /* We have read some more data */
			left -= nr;     /* Less data left to read */
			p += nr; /* So we don't read over already read data */
		}
	}

}


static int sdr14_read(struct sdr14_data_pkt *pkt)
{
	uint8_t *p;

	size_t n = 0;
	size_t left = sizeof(struct sdr14_data_pkt);  /* The total size of the buffer */
	p = (uint8_t *) pkt;
	while (left > 0)
	{
		ssize_t nr = read(sdr14_fd, p, left);
		if (nr <= 0) 	{
			if (nr < 0)
				perror("read");  /* Error! */
			break;
		}
		else {
			n += nr;     /* We have read some more data */
			left -= nr;     /* Less data left to read */
			p += nr; /* So we don't read over already read data */
		}
	}

	return 0;
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
 * @brief computes the observing strategy
 *
 * @param acq the acquisition configuration
 * @param acs the allocated array of observation steps
 *
 */

static void sdr14_comp_obs_strategy(struct observation *obs)
{
	g_message(MSG "computing acquisition strategy for requested parameters");

	/* we use radix-2 divs */
	obs->blsize   = sdr14.bins >> obs->acq.bin_div;
	printf("div is %d blsize %d\n", obs->acq.bin_div, obs->blsize);
	/* bins to discard on either side of a "raw" spectrum */
	obs->disc_raw = (int) ((double) SDR14_SIDE_DISCARD_HZ / (SDR14_RT_BW / obs->blsize));
	obs->bw_eff   = (double) SDR14_RT_BW - (double) obs->disc_raw * 2.0 * SDR14_RT_BW / obs->blsize;
	obs->f0	      = obs->acq.freq_start_hz - SDR14_SIDE_DISCARD_HZ + SDR14_RT_BW / 2;
	obs->f1	      = obs->acq.freq_stop_hz  + SDR14_SIDE_DISCARD_HZ + SDR14_RT_BW / 2;
	obs->n_seq    = (gsize) ceil((obs->f1 - obs->f0) / obs->bw_eff);
	/* bins to discard on last spectrum in sequence so the cut out is proper */
	obs->disc_fin = (int) (obs->n_seq * obs->bw_eff -
			      (obs->acq.freq_stop_hz - obs->acq.freq_start_hz)) / (SDR14_RT_BW / obs->blsize);

	if (obs->disc_fin < 0) {
		g_warning("discard bins is %d, setting to 0\n", obs->disc_fin);
		obs->disc_fin = 0;
	}	

	g_message(MSG "observation requires acquisition of %u raw spectrae", obs->n_seq);
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

	for (i = 0; i < s->n; i++)
		s->spec[i] = (uint32_t) ((double) s->spec[i]) * sdr14.temp_cal_factor;
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
	uint8_t ack[8];
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

	int len;

	if (!obs->acq.acq_max)
		return 0;


	timer = g_timer_new();


	/* prepare and send: allocate full length */
	len = ((obs->blsize - 2 * obs->disc_raw) * obs->n_seq - obs->disc_fin);
	if (len <= 0 ) {
		g_usleep(10000);
		goto noobs;
	}

	//printf("len %d %d %d %d %d\n", len, obs->blsize, obs->disc_raw, obs->n_seq, obs->disc_fin);
	s = g_malloc0(sizeof(struct spec_data) + len * sizeof(uint32_t));

	fft_init(obs->blsize, &p0, &reamin0, &reamout0);

	/* update number of SDR14_NSAM sequences to record in the one shot command */
	oneshot_cmd[7] = obs->acq.n_stack;

	s_rec.busy = 1;
	s_rec.eta_msec = (typeof(s_acq.eta_msec))(acq_time[obs->acq.bin_div] *  1000. * (double) obs->n_seq * obs->acq.n_stack * SDR14_NSAM / obs->blsize);
	ack_status_rec(PKT_TRANS_ID_UNDEF, &s_rec);


	freq = obs->f0 - RECV_LO_FREQ;
	for (l = 0; l < obs->n_seq; l++) {
	

		sdr14_serial_flush(sdr14_fd);
		sdr14_set_freq(freq);

		freq = freq + obs->bw_eff;

		if (!g_mutex_trylock(&acq_abort)) {
			g_message(MSG "acquisition loop abort indicated");
			goto cleanup;
		}

		g_mutex_unlock(&acq_abort);

		write(sdr14_fd, oneshot_cmd, sizeof(oneshot_cmd));
		read(sdr14_fd, ack, sizeof(ack));


		for (i = 0; i < obs->blsize; i++)
			spec[i] = 0;

		s_acq.eta_msec = (typeof(s_acq.eta_msec))(acq_time[obs->acq.bin_div] * 1000. * (double) obs->acq.n_stack * SDR14_NSAM / obs->blsize);
		if (s_acq.eta_msec > MIN_MS_ACQ_STATUS) {
			s_acq.busy = 1;
			ack_status_acq(PKT_TRANS_ID_UNDEF, &s_acq);
		}

		for (k = 0; k < obs->acq.n_stack; k++) {

			int z;

			sdr14_read(&pkt);

			g_timer_start(timer);
			for (z = 0; z < SDR14_NSAM / obs->blsize; z++) {

				for (j = 0; j < 2* obs->blsize; j++)
					reamin0[j] = (double) (pkt.data[j + z * obs->blsize * 2 ]);

				cfft(&p0);

				for (i = 0; i < obs->blsize -1; i++) {

					if (i < obs->blsize / 2)
						j = i + obs->blsize / 2;
					else
						j = i - obs->blsize / 2 + 1; /* skip one for DC?? TOOD: VERIFY with sig gen */

					spec[i] +=  sqrt((reamout0[2 * j] * reamout0[2 * j]
						    + reamout0[2 * j + 1] * reamout0[2 * j + 1]) );
				}
			}

			g_timer_stop(timer);
			acq_time[obs->acq.bin_div] =(acq_time[obs->acq.bin_div] * (AVG_LEN - 1.0) +  g_timer_elapsed(timer, NULL)) / AVG_LEN;
		}

		if (s_acq.eta_msec > MIN_MS_ACQ_STATUS) {
			s_acq.busy = 0;
			s_acq.eta_msec = 0;
			ack_status_acq(PKT_TRANS_ID_UNDEF, &s_acq);
		}

		scale = (double)obs->acq.n_stack * (double)SDR14_NSAM / (double)obs->blsize;
		scale *= sqrt((double)obs->blsize);
		scale = 1.0 / scale;

		for (i = 0; i < obs->blsize - 2 * obs->disc_raw ; i++) {

			double tmp;

			tmp = (double)spec[i + obs->disc_raw] * scale;
			tmp *= tmp;	/* ADC samples voltage, we want power-equivalent -> P ~ V^2 */
			s->spec[s->n] = (uint32_t) tmp;

			if (s->n == (len - 1))	/* will skip final discarded bins */
				goto done;
			s->n++;
		}
		
	}


done:
	s->freq_min_hz = (typeof(s->freq_min_hz)) obs->acq.freq_start_hz;
	s->freq_max_hz = (typeof(s->freq_max_hz)) obs->acq.freq_stop_hz;
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) ((s->freq_max_hz - s->freq_min_hz) / s->n);

	//printf("%d %d %d %d\n", s->freq_min_hz, s->freq_max_hz, s->freq_inc_hz,  s->n );

	sdr14_apply_temp_calibration(s);

	/* handover for transmission */
	if (last_acq_mode)
		ack_spec_data(PKT_TRANS_ID_UNDEF, s);



	s_rec.busy = 0;
	s_rec.eta_msec = 0;
	ack_status_rec(PKT_TRANS_ID_UNDEF, &s_rec);

cleanup:
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
		g_warning(MSG "start frequency %d too low, min %d",
			  acq->freq_start_hz, sdr14.freq_min_hz);
		return -1;
	}

	if (acq->freq_stop_hz > sdr14.freq_max_hz) {
		g_warning(MSG "stop frequency %d too low, max %d",
			  acq->freq_stop_hz, sdr14.freq_max_hz);
		return -1;
	}

	if (acq->bin_div > sdr14.freq_bin_div_max) {
		g_warning(MSG "bandwidth divider exponent %d too high, max %d",
			  acq->bin_div, sdr14.freq_bin_div_max);
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
