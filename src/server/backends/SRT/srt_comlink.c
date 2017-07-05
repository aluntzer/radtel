/**
 * @file    server/backends/SRT/srt_comlink.c
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
 * @brief plugin for the SRT's serial com link
 *
 * @todo this is likely not work on non-unix systems
 */


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <termios.h>

#include <glib.h>
#include <gmodule.h>


#include <math.h>
#include <backend.h>


#define MSG "SRT COM: "


static char *srt_tty = "/dev/ttyUSB0";

static int fd;
static GMutex linkmutex;
static GMutex readmutex;
static GCond cond;
static gchar *line;
static gsize line_len;

/**
 * @brief open a serial tty
 * @param tty the path to the tty
 *
 * @return 0 on error, file descriptor otherwise
 */

static int srt_com_serial_open_port(const char *tty)
{
	int fd;



	fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);

	if (fd < 0) {
		perror("unable to open serial port");
		return 0;
	}

#if 0
	fcntl(fd, F_SETFL, O_NONBLOCK);
#else
	fcntl(fd, F_SETFL, 0);
#endif

	return fd;
}


/**
 * @brief close a serial tty
 * @param fd the file descriptor to close
 * @return see man 3 close()
 */

static int srt_com_serial_close_port(int fd)
{
	return close(fd);
}


/**
 * @brief set the serial link parameters (for the SRT)
 *
 * @param fd the file descriptor of the serial tty
 *
 * @return see man 3 tcsetattr
 */

static int srt_com_serial_set_comm_param(int fd)
{
	struct termios cfg;


	/* get current port attributes */
	tcgetattr(fd, &cfg);


	/* char size is 8 */
	cfg.c_cflag &= ~CSIZE;		/* clear character size bits */
	cfg.c_cflag |= CS8;		/* set with to 8 bits */

	/* baud rate is 2400 */
	cfsetispeed(&cfg, B2400);
	cfsetospeed(&cfg, B2400);

	/* single stop bit */
	cfg.c_cflag &= ~CSTOPB;	/* 1 stop bit */

	/* no parity */
	cfg.c_cflag &= ~PARENB;
	cfg.c_iflag &=  ~INPCK;

	/* no flow control */
	cfg.c_iflag &= ~(IXON | IXOFF | IXANY);

	/* set raw i/o */
	cfg.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	cfg.c_oflag &= ~OPOST;

	/* enable receiver and set local mode */
	cfg.c_cflag |= (CLOCAL | CREAD);

	/* set configuration */
	return tcsetattr (fd, TCSANOW, &cfg);
}


/**
 * @brief read from the serial port
 *
 * @param fd the file descriptor of the serial tty
 * @param buf the buffer to read to
 * @parma nbyte the size of the buffer (maximum number of bytes to read)
 *
 * @return see man 3 read
 *
 * @note unused
 */

static int srt_com_serial_read(int fd, char *buf, size_t nbyte)
{
	int n;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	n = read(fd, buf, nbyte);

	if (n < 0)
		perror("serial port read failed");

	return n;
}


/**
 * @brief write to the serial port
 *
 * @param fd the file descriptor of the serial tty
 * @param buf the buffer to write from
 * @parma nbyte the size of the data in the buffer
 * @param drain if set, waits until all output has been transmitted
 *
 * @return see man 3 write
 */

static int srt_com_serial_write(int fd, const char *buf,
				size_t nbyte, int drain)
{
	int n;


	n = write(fd, buf, nbyte);

	if (drain)
		tcdrain(fd);

	if (n < 0)
		perror("serial port write failed");

	return n;
}


/**
 * @brief glib io watch callback
 */

static gboolean srt_com_serial_glib_callback(GIOChannel *src,
					     GIOCondition giocond,
					     gpointer unused)
{
	GIOStatus ret;

	GError *err = NULL;



	/* something arrived when noone listened, release it so we don't leak */
	if (line) {
		g_message(MSG "a serial read was lost: %s", line);
		g_free(line);
	}

	ret = g_io_channel_read_line(src, &line, &line_len, NULL, &err);
	if (ret == G_IO_STATUS_ERROR) {
		g_error("Error reading: %s\n", err->message);
		g_error_free(err);
	}


	g_mutex_lock(&readmutex);
	g_cond_signal(&cond);
	g_mutex_unlock(&readmutex);

	return TRUE;
}


/**
 * @brief configure glib io polling of the serial tty
 */

static GIOChannel *srt_com_serial_init_glib_poll(int fd)
{
	GIOChannel *g_io_ch = NULL;


	g_io_ch = g_io_channel_unix_new(fd);

	/* explicitly set line terminator, autodetection does not appear
	 * to work properly here
	 */
	g_io_channel_set_line_term(g_io_ch, "\r", 1);

	g_io_add_watch (g_io_ch, (G_IO_IN | G_IO_PRI),
			srt_com_serial_glib_callback, NULL);

	return g_io_ch;
}


/**
 * @brief load configuration keys
 */

static void srt_com_load_keys(GKeyFile *kf)
{
	GError *error = NULL;


	srt_tty = g_key_file_get_string(kf, "Serial", "tty", &error);

	if (error)
		g_error(error->message);
}


/**
 * @brief load the configuration file
 */

static int srt_com_load_config(void)
{
	gsize len;
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;

	GError *error = NULL;



	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;


	ret = g_key_file_load_from_file(kf, "config/backends/srt_comlink.cfg",
					flags, &error);

	if (!ret) {
		g_error(MSG "error loading config file %s", error->message);
		g_clear_error(&error);
		return -1;
	}

	srt_com_load_keys(kf);

	g_key_file_free(kf);

	return 0;
}


/**
 * @brief read from the shared link
 * @note this blocks until one line has been read
 */

G_MODULE_EXPORT
gchar *be_shared_comlink_read(gsize *nbytes)
{
	gchar *tmp;


	g_mutex_lock(&readmutex);

	g_message(MSG "waiting for serial response");
	g_cond_wait(&cond, &readmutex);


	tmp = line;
	(*nbytes) = line_len;

	/* we have it, now clear the line pointer */
	line = NULL;

	g_mutex_unlock(&readmutex);

	return tmp;
}


/**
 * @brief write on the shared link
 *
 */

G_MODULE_EXPORT
int be_shared_comlink_write(const gchar *buf, gsize nbytes)
{
	return srt_com_serial_write(fd, buf, nbytes, 0);
}

/**
 * @brief acquire exclusive use of shared link
 */

G_MODULE_EXPORT
void be_shared_comlink_acquire(void)
{
	g_mutex_lock(&linkmutex);
	g_message(MSG "shared comlink acquired");
}


/**
 * @brief release shared link
 */

G_MODULE_EXPORT
void be_shared_comlink_release(void)
{
	g_mutex_unlock(&linkmutex);
	g_message(MSG "shared comlink released");
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
        g_message(MSG "configuring serial link");

	fd = srt_com_serial_open_port(srt_tty);
	if (!fd)
		g_error(MSG "Error opening serial port %s\n", srt_tty);

	if (srt_com_serial_set_comm_param(fd))
		g_error(MSG "Error setting parameters for serial port %s\n",
			srt_tty);

	srt_com_serial_init_glib_poll(fd);
}


/**
 * @brief the module initialisation function
 * @note this is called automatically on g_module_open
 */

G_MODULE_EXPORT
void g_module_check_init(void)
{
        g_message(MSG "initialising module");

	if (srt_com_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");
}
