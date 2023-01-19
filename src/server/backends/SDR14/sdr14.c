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


#include <termios.h>



static char *sdr14_tty = "/dev/ttyUSB0";
static int sdr14_fd;


#define AVG 10
#define NSAM  2048	/* 2048 16 bit I/Q pairs (in AD66220 mode)  */

int Status;
int freq_A2D = 66666666; /* this is only relevant in real non-AD6620 mode */


double *reamin0, *reamout0;
fftw_plan p0;

/* I'm beginning to suspect that we use too many locks :D */
static GThread *thread;
static GCond	acq_cond;
static GMutex   acq_lock;
static GMutex   acq_pause;
static GRWLock  obs_rwlock;


#define MSG "SDR14 SPEC: "

#define SDR14_HDR_LEN	2
/* SDR14 data items are fixed for type 0 data items (I/Q or real samples) */
#define SDR14_DATA0_LEN	4096

struct sdr14_data_pkt {
	uint8_t  hdr[SDR14_HDR_LEN];
	int16_t data[SDR14_DATA0_LEN];
};


/**
 * @brief flush pending bytes
 */

static void sdr14_serial_flush(int fd)
{
	int n;
	unsigned char c;

	printf("flushing...");
	while ((n = read(fd, &c, sizeof(c))) > 0);
	printf("done\n");
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



static int myread(size_t left, uint8_t *p)
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
	uint8_t ack[8];
	uint8_t *p;
#if 0
	uint8_t oneshot_cmd[8] = {0x08, 0x00, 0x18, 0x00, 0x81, 0x02, 0x02, 0x01};





	write(sdr14_fd, oneshot_cmd, sizeof(oneshot_cmd));

	/*  read ack */
	n = read(sdr14_fd, ack, sizeof(ack));
#endif
	/*  read data sample */




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


#if 0
	/*  read unsolicited receive state */
	read(sdr14_fd, ack, sizeof(ack));

	/*  read unsolicited receive state idle */
	read(sdr14_fd, ack, sizeof(ack));
#endif

	return 0;
}

static void sdr14_get_mode(void)
{
	uint8_t cmd[6] = {0x50, 0x20, 0x18, 0x00, 0x00};

	printf("reading mode\n");

	write(sdr14_fd, cmd, sizeof(cmd));

	read(sdr14_fd, cmd, sizeof(cmd));

	printf("ack was %x\n", cmd[5]);

}

static void sdr14_set_freq(void)
{
	uint8_t cmd[10] = {0x0a, 0x00, 0x20, 0x00, 0x00, 0x40, 0x4b, 0x4c, 0x00, 0x01};
	uint8_t cmd2[6] = {0x06, 0x00, 0x40, 0x00, 0x00, 0x0};
	uint8_t cmd3[6] = {0x06, 0x00, 0x38, 0x00, 0x00, 0x0};
	uint8_t ack[10] = {0};
	int n;

	printf("setting 5 MHz NCO freq\n");

	n = write(sdr14_fd, &cmd[0], 10);
	printf("wrote %d\n", n);

	n = myread(10, ack);

	printf("acked %d %x %x %x\n", n, ack[0], ack[1], ack[2],  ack[3]);
#if 1
	write(sdr14_fd, cmd2, sizeof(cmd2));
	n =read(sdr14_fd, cmd2, sizeof(cmd2));
	printf("acked %d %d\n", n == sizeof(cmd2), n);

	write(sdr14_fd, cmd3, sizeof(cmd3));
	n = read(sdr14_fd, cmd3, sizeof(cmd3));
	printf("acked %d\n", n == sizeof(cmd3));
#endif
}





static void fft_init(int n, fftw_plan * p, float **reamin0, float **reamout0)
{
	fftw_complex *in, *out;
	//printf("entering fft_init\n");
	in = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * n);
	out = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * n);
	// printf("entering fft_init after malloc %p\n",in);
	*p = fftw_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	*reamin0 = (float *) in;
	*reamout0 = (float *) out;

	// printf("reamin0 %p reamout0 %p\n",reamin0,reamout0);
}

__attribute__((unused))
static void fft_free(fftw_plan * p, float **reamin0, float **reamout0)
{
	fftw_destroy_plan(*p);
	fftw_free((fftw_complex *) * reamin0);
	fftw_free((fftw_complex *) * reamout0);
}

static void cfft(fftw_plan * p)
{
	fftw_execute(*p);
}
	static int once;

/**
 * @brief acquire spectrea
 *
 u @param acq the acquisition configuration
 * @returns 0 on completion, 1 if more acquisitions are pending
 *
 */

static uint32_t sdr14_spec_acquire(void)
{
	int i, j, k;
	int blsiz = NSAM;

	int blsiz2 = blsiz / 2;

	GTimer *timer;
	uint32_t *p;

	double spec[NSAM] = {0};


	struct spec_data *s = NULL;
	struct sdr14_data_pkt pkt;


	timer = g_timer_new();
	g_timer_start(timer);

	/* prepare and send: allocate full length */
	s = g_malloc0(sizeof(struct spec_data) + blsiz * sizeof(uint32_t));

	s->n = blsiz;

	/* XXX fixme, these are just random values */
	s->freq_min_hz = (typeof(s->freq_min_hz)) 0;
	s->freq_max_hz = (typeof(s->freq_max_hz)) (NSAM);
	s->freq_inc_hz = (typeof(s->freq_inc_hz)) (NSAM) / s->n;

	uint8_t oneshot_cmd[8] = {0x08, 0x00, 0x18, 0x00, 0x81, 0x02, 0x00, 0x01};
	uint8_t ack[8];

	uint8_t hbeat[3] = {0x03,0x60,0x00};

	if (!once) {
	write(sdr14_fd, oneshot_cmd, sizeof(oneshot_cmd));

	/*  read ack */
	read(sdr14_fd, ack, sizeof(ack));
	once = 1;
	}

			float rre, aam, aam2, rre2;
	for (k = 0; k < AVG; k++) {

		sdr14_read(&pkt);

		for (j = 0; j < 2* blsiz; j++)
			reamin0[j] = (double) (pkt.data[j]);


		cfft(&p0);

		p = s->spec;

		for (i = 0; i < blsiz; i++) {
#if 0
			for (j = 0; j < blsiz2; j++) {
				re[j] = reamout0[2 * j];
				am[j] = reamout0[2 * j + 1];

			}


			for (i = 0; i < blsiz2; i++) {
				if (i < blsiz2 / 2)
					j = i + blsiz2 / 2;
				else
					j = i - blsiz2 / 2;

				j = i;
				/* note: should sqrt after avg, creates
				 * deviation */
				p[j] += (uint32_t) ((1.0/((float) AVG)) * sqrt(re[i] * re[i] + am[i] * am[i]));
				if (i < 10)
					p[j] = 0;
			}
#endif
#if 1
#if 0
			if (i >= 1) {
				rre = reamout0[2 * i] + reamout0[2 * (blsiz - i)];
				aam = reamout0[2 * i + 1] - reamout0[2 * (blsiz - i) + 1];
				aam2 = -reamout0[2 * i] + reamout0[2 * (blsiz - i)];
				rre2 = reamout0[2 * i + 1] + reamout0[2 * (blsiz - i) + 1];
			} else {
				rre = reamout0[2 * i] + reamout0[0];
				aam = reamout0[2 * i + 1] - reamout0[1];
				aam2 = -reamout0[2 * i] + reamout0[0];
				rre2 = reamout0[2 * i + 1] + reamout0[1];
			}
			(*p) += (uint32_t) ((1.0/((float) AVG)) * sqrt(rre * rre + aam * aam + rre2 * rre2 + aam2 * aam2));
#else
			//(*p) += (uint32_t) ((1.0/((float) AVG)) * sqrt(reamout0[2 * i] * reamout0[2 * i] + reamout0[2 * i + 1] * reamout0[2 * i + 1]) );
			
			if (i < blsiz / 2)
				j = i + blsiz / 2;
			else
				j = i - blsiz / 2;
			
			spec[i] +=   1.0/((float) AVG) * sqrt((reamout0[2 * j] * reamout0[2 * j] + reamout0[2 * j + 1] * reamout0[2 * j + 1]) );
			//(*p) += (uint32_t) ((1.0/((float) AVG)) * sqrt(reamout0[i] * reamout0[i] + reamout0[blsiz -i] * reamout0[blsiz -i]) );
#endif
			p++;
		}
#endif

	}


	write(sdr14_fd, hbeat, sizeof(hbeat));

	g_timer_stop(timer);

#if 1
	for (i = 0; i < s->n; i++) {
		s->spec[i] = (uint32_t) (spec[i]);
	}
#else
	for (j = 0; j < s->n; j++) {
		s->spec[j] = (uint32_t) (pkt.data[2*j] + 0x7fff);
	}
#endif

	/* handover for transmission */
	ack_spec_data(PKT_TRANS_ID_UNDEF, s);

	g_free(s);

	g_message(MSG "ACQ: %f sec", g_timer_elapsed(timer, NULL));

	g_timer_destroy(timer);



#if 0
	/* to move to cleanup() or module unload if any... */
	fft_free(&p0, &reamin0, &reamout0);
#endif


	return 1;
}



/**
 * @brief pause/unpause radio acquisition
 *
 */

static void sdr14_spec_acq_enable(gboolean mode)
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
			run = sdr14_spec_acquire();
			g_rw_lock_reader_unlock(&obs_rwlock);
		} while (run);


		g_mutex_unlock(&acq_lock);
	}
}


/**
 * @brief spectrum acquisition configuration
 */

G_MODULE_EXPORT
int be_spec_acq_cfg(struct spec_acq_cfg *acq)
{
#if 0
	if (srt_spec_acquisition_configure(acq))
		return -1;
#endif
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
#if 0
	memcpy(acq, &g_obs.acq, sizeof(struct spec_acq_cfg));
#endif
	return 0;
}


/**
 * @brief spectrum acquisition enable/disable
 */

G_MODULE_EXPORT
int be_spec_acq_enable(gboolean mode)
{
	once = 0;
	sdr14_spec_acq_enable(mode);

	return 0;
}


/**
 * @brief get telescope spectrometer capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_spec(struct capabilities *c)
{
	c->freq_min_hz		= 1400000000;
	c->freq_max_hz		= 1430000000;
	c->freq_inc_hz		= 100;
	c->bw_max_hz		= 100;
	c->bw_max_div_lin	= 0;
	c->bw_max_div_rad2	= 1;
	c->bw_max_bins		= 2048;
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


	g_message(MSG "configuring serial link");

	sdr14_fd = sdr14_serial_open_port(sdr14_tty);
	if (!sdr14_fd)
		g_error(MSG "Error opening serial port %s\n", sdr14_tty);

	if (sdr14_serial_set_comm_param(sdr14_fd))
		g_error(MSG "Error setting parameters for serial port %s\n",
			sdr14_tty);


	sdr14_serial_flush(sdr14_fd);
	//sdr14_get_mode();
	sdr14_set_freq();

	g_message(MSG "starting spectrum acquisition thread");

	thread = g_thread_new(NULL, sdr14_spec_thread, NULL);

	/* always start paused */
	sdr14_spec_acq_enable(FALSE);

#if 0
	srt_spec_cfg_defaults();
#endif
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
const gchar *g_module_check_init(void)
{

        g_message(MSG "initialising module");

	fft_init(NSAM, &p0, &reamin0, &reamout0);

#if 0
	if (srt_spec_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");

	srt_spec_load_calibration();
#endif
	return NULL;
}
