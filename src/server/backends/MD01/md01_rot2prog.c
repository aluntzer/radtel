/**
 * @file    server/backends/MD01/md01_rot2prog.c
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
 * @brief plugin for control of SPID MD01 controller via Rot2Prog protocol
 *
 * @note this plugin is in alpha state
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <glib.h>
#include <gmodule.h>
#include <string.h>

#include <math.h>
#include <backend.h>
#include <ack.h>

#include <net.h>


#define MSG "MD01 ROT2PROG: "

#define ROT2PROG_CMD_BYTES	13
#define ROT2PROG_ACK_BYTES	12
#define ROT2PROG_CMD_CONFIG	498	/* typo in protocol table? */
#define ROT2PROG_ACK_CONFIG	499

#define CMD_STOP		0x0f	/* stop rotation */
#define CMD_STATUS		0x1f	/* get position */
#define CMD_SET			0x2f	/* set position */
#define CMD_POLL_MSG		0x3f	/* poll message (?) */
#define CMD_CFG_CTRL		0x4f	/* get controller configuration dump */
#define CMD_SEND_CFG		0xf4	/* not listed, but documented */
#define CMD_GET_PARAM		0xf5	/* listed, appears to do nothing */
#define CMD_SAVE_CFG		0xf6	/* save configuration sent with 0xf4 */
#define CMD_POWER		0xf7	/* 3 payload byte response, purpose unknown */
#define CMD_CLEAN_SETTINGS	0xf8	/* reset (to defaults?) (not tested) */
#define CMD_GET_SOFT_HARD	0xa1	/* get hard/soft start/stop, response format unknown */
#define CMD_SET_SOFT_HARD	0xa2	/* set hard/soft start/stop, command format unknown */
#define CMD_MOTOR		0x14	/* motor command, needs specifier in H1 field */


/* default tty configuration, overridable in config file */
static char *md01_tty = "/dev/ttyUSB0";
static speed_t tty_rate = B460800;

static int fd;


static struct {

	/* azimuth movement limits */
	struct {
		double left;
		double right;
	} az_limits;

	/* elevation movement limits */
	struct {
		double lower;
		double upper;
	} el_limits;

	/* park position */
	struct {
		double az;
		double el;
		gboolean is_conf;
	} park;


	/* coordinates */
	struct {
		double az_cur;		/* current azimuth */
		double el_cur;		/* current elevation */

		double az_tgt;		/* target azimuth */
		double el_tgt;		/* target elevation */
	} pos;

	/* drive resolution */
	struct	{
		gint hdiv;		/* horizontal res divider */
		gint vdiv;		/* vertical res divider */
		double h;		/* horizontal res in deg */
		double v;		/* vertical res in deg */
	} res;

} md01;



static void md01_rot2prog_eval_response(const char *msg, gsize len);


/**
 * @brief convert numerical value to speed_t
 * @note see man 3 termios
 */

static speed_t get_baudrate(gint baud)
{
	switch (baud) {
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
	default:
		g_warning(MSG "unsupported baud rate %d", baud);
		return B0;	/* hang up */
	}
}


/**
 * @brief load configuration keys
 */

static void md01_rot2prog_load_keys(GKeyFile *kf)
{
	gsize len;

	double *tmp;

	GError *error = NULL;


	md01_tty = g_key_file_get_string(kf, "Serial", "tty", &error);
	if (error)
		g_error(error->message);


	tty_rate = get_baudrate(g_key_file_get_integer(kf, "Serial", "baud", &error));
	if (error)
		g_error(error->message);

	tmp = g_key_file_get_double_list(kf, "Drive",
					   "az_limits", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of azimuth limits configured.");

	md01.az_limits.left  = tmp[0];
	md01.az_limits.right = tmp[1];
	g_free(tmp);

	tmp = g_key_file_get_double_list(kf, "Drive",
					   "el_limits", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of elevation limits configured.");

	md01.el_limits.lower = tmp[0];
	md01.el_limits.upper = tmp[1];
	g_free(tmp);

	md01.res.hdiv = g_key_file_get_integer(kf, "Drive", "hor_div", &error);
	if (error)
		g_error(error->message);

	md01.res.h = 1.0 / ((double) md01.res.hdiv);


	md01.res.vdiv = g_key_file_get_integer(kf, "Drive", "ver_div", &error);
	if (error)
		g_error(error->message);

	md01.res.v = 1.0 / ((double) md01.res.vdiv);


	/* this key is optional */
	if (g_key_file_has_key(kf, "Drive", "park_pos", &error)) {

		tmp = g_key_file_get_double_list(kf, "Drive", "park_pos",
						 &len, &error);

		if (len == 2) {
			md01.park.az = tmp[0];
			md01.park.el = tmp[1];
			md01.park.is_conf = TRUE;
			g_free(tmp);
		} else if (len && len != 2) {
			g_error(MSG "Error, park position format is AZ;EL");
		}
	}



}


/**
 * @brief load the md01_rot2prog configuration file from a given prefix
 *
 * @returns 0 on success, otherwise error
 */

static int md01_rot2prog_load_config_from_prefix(const gchar *prefix, GError **err)
{
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;


	gchar *cfg;


	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	cfg = g_strconcat(prefix, "backends/md01_rot2prog.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, err);


	if (!ret) {
		g_key_file_free(kf);
		g_free(cfg);
		return -1;
	}

	g_message(MSG "Configuration file loaded from %s", cfg);

	md01_rot2prog_load_keys(kf);

	g_key_file_free(kf);
	g_free(cfg);

	return 0;
}


/**
 * @brief try to load a md01_rot2prog configuration file from various paths
 */

int md01_rot2prog_load_config(void)
{
	int ret;

	gchar *prefix;

	GError *error = NULL;



	/* search relative path first */
	ret = md01_rot2prog_load_config_from_prefix("/", &error);
	if (ret) {
		g_clear_error(&error);
		/* try again in sysconfdir */
		prefix = g_strconcat(SYSCONFDIR, "/", CONFDIR, "/", NULL);
		ret = md01_rot2prog_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_warning(MSG "Could not find backends/md01_rot2prog.cfg: %s. "
			  "Looked in ./, %s and %s/%s",
			  error->message, CONFDIR, SYSCONFDIR, CONFDIR);
		g_clear_error(&error);

		return -1;
	}

	return 0;
}


/**
 * @brief open a serial tty
 * @param tty the path to the tty
 *
 * @return 0 on error, file descriptor otherwise
 */

static int md01_rot2prog_serial_open_port(const char *tty)
{
	int fd;



	fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY);


	if (fd < 0) {
		perror("unable to open serial port");
		return 0;
	}

	fcntl(fd, F_SETFL, 0);

	return fd;
}


/**
 * @brief close a serial tty
 * @param fd the file descriptor to close
 * @return see man 3 close()
 */
__attribute__((unused))
static int md01_rot2prog_serial_close_port(int fd)
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

static int md01_rot2prog_serial_set_comm_param(int fd)
{
#if 0
	struct termios cfg;


	/* get current port attributes */
	tcgetattr(fd, &cfg);


	/* char size is 8 */
	cfg.c_cflag &= ~CSIZE;		/* clear character size bits */
	cfg.c_cflag |= CS8;		/* set with to 8 bits */

	/* baud rate is fixed at 460800 for USB link */
	/* TODO: make configurable */
	cfsetispeed(&cfg, tty_rate);
	cfsetospeed(&cfg, tty_rate);

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

	/* cfg.c_cc[VMIN] = ROT2PROG_ACK_BYTES; */
	cfg.c_cc[VMIN] = 0;
	cfg.c_cc[VTIME] = 10;

	/* set configuration */
	return tcsetattr (fd, TCSANOW, &cfg);

#else
        struct termios cfg = {0};


        /* we start from a cleared struct, so we need only set our config */
        cfg.c_cflag = CS8 | CLOCAL | CREAD;
        cfg.c_iflag = IGNPAR;

        cfsetispeed(&cfg, B460800);
        cfsetospeed(&cfg, B460800);

        cfg.c_cc[VTIME] = 1;

        /* set configuration */
        return tcsetattr (fd, TCSANOW, &cfg);
#endif
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

static int md01_rot2prog_serial_write(int fd, const char *buf,
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





static int myread(int fd, uint8_t *p, size_t left)
{
        size_t n = 0;
        while (left > 0)
        {
                ssize_t nr = read(fd, p, left);
                if (nr <= 0)    {
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

	return n;
}



/**
 * @brief flush pending bytes
 */

static void md01_rot2prog_serial_flush(int fd)
{
        int n;
        unsigned char c;
	return;
        printf("flushing...");
        while ((n = read(fd, &c, sizeof(c))) > 0);
        printf("done\n");
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

static int md01_rot2prog_serial_read(int fd, char *buf, size_t nbyte)
{
	int n;

	n = myread(fd, buf, nbyte);

	if (n < 0)
		perror("serial port read failed");

	return n;
}


/**
 * @brief interpret the responses received from the device
 */

static void md01_rot2prog_eval_response(const char *msg, gsize len)
{
	int i;



	if (len == ROT2PROG_ACK_BYTES) {
		md01.pos.az_cur = msg[1] * 100.
				+ msg[2] * 10.
				+ msg[3]
				+ msg[4] * md01.res.h
				- 360.;

		md01.pos.el_cur = msg[6] * 100.
				+ msg[7] * 10.
				+ msg[8]
				+ msg[9] * md01.res.v
				- 360.0;

		md01.pos.az_cur = round(md01.pos.az_cur * 10.0) * 0.1;
		md01.pos.el_cur = round(md01.pos.el_cur * 10.0) * 0.1;
		/* fold into 0-360, 0-90 */
		md01.pos.az_cur = fmod(md01.pos.az_cur, 360.0);
		md01.pos.el_cur = fmod(md01.pos.el_cur, 90.0);

		if (md01.pos.az_cur < 0.)
			md01.pos.az_cur += 360.0;
		if (md01.pos.el_cur < 0.)
			md01.pos.el_cur += 90.0;

	} else if (len == ROT2PROG_ACK_CONFIG) {
		g_message(MSG "configuration data received:");
#if 1
		for (i = 0; i < len; i++) {
			g_print("%x ", msg[i]);
			if (i && (i % 16 == 0))
			    g_print("\n");
		}
		g_print("\n");
#endif

	} else {
		g_message(MSG "unknown message of lenght %d received:", len);

		for (i = 0; i < len; i++)
			g_print("%x ", msg[i]);

		g_print("\n");
	}

#if 0
		for (i = 0; i < len; i++)
			g_print("%x ", msg[i]);

		g_print("\n");
#endif
}




/**
 * @brief rotate to coordinates
 *
 * @returns -1 on error, 0 otherwise
 */

static int md01_rot2prog_moveto(double az, double el)
{
	char buf[ROT2PROG_ACK_BYTES];
	char cmdstr[ROT2PROG_CMD_BYTES];
	int u_az, u_el;


	u_az = (int) (md01.res.hdiv * (360 + az));
	u_el = (int) (md01.res.vdiv * (360 + el));

	md01.pos.az_tgt = az;
	md01.pos.el_tgt = el;


	cmdstr[0]  = 0x57;                       /* S   */
	cmdstr[1]  = 0x30 +  u_az / 1000;        /* H1  */
	cmdstr[2]  = 0x30 + (u_az % 1000) / 100; /* H2  */
	cmdstr[3]  = 0x30 + (u_az % 100) / 10;   /* H3  */
	cmdstr[4]  = 0x30 + (u_az % 10);         /* H4  */
	cmdstr[5]  = md01.res.hdiv;		 /* PH  */
	cmdstr[6]  = 0x30 +  u_el / 1000;        /* V1  */
	cmdstr[7]  = 0x30 + (u_el % 1000) / 100; /* V2  */
	cmdstr[8]  = 0x30 + (u_el % 100) / 10;   /* V3  */
	cmdstr[9]  = 0x30 + (u_el % 10);         /* V4  */
	cmdstr[10] = md01.res.vdiv;		 /* PV  */
	cmdstr[11] = 0x2F;                       /* K   */
	cmdstr[12] = 0x20;                       /* END */

#if 0
	{
		int i;
		for (i = 0; i < ROT2PROG_CMD_BYTES; i++) {
			g_print("%x ", cmdstr[i]);
			if (i && (i % 16 == 0))
				g_print("\n");
		}
		g_print("\n");
		g_message("moveto write!");
	}
#endif

	md01_rot2prog_serial_write(fd, cmdstr, ROT2PROG_CMD_BYTES, 0);

#if 1
	int n;
	n = md01_rot2prog_serial_read(fd, buf, ROT2PROG_ACK_BYTES);

	if (n != ROT2PROG_ACK_BYTES) {
		g_warning(MSG "moveto mismatch in message length. expected %d, got %d",
			  ROT2PROG_ACK_BYTES, n);


	{
		int i;
		for (i = 0; i < n; i++) {
			g_print("%x ", buf[i]);
		}
		g_print("\n");

		md01_rot2prog_serial_flush(fd);
	}




		return -1;
	} else

	md01_rot2prog_eval_response(buf, n);

	az = md01.pos.az_cur;
	el = md01.pos.el_cur;


#endif
	ack_moveto_azel(PKT_TRANS_ID_UNDEF, md01.pos.az_tgt, md01.pos.el_tgt);

	g_debug(MSG "rotating to AZ/EL %g/%g", md01.pos.az_tgt, md01.pos.el_tgt);

	/* TODO eval response and return -1 on errro */

	return 0;
}

/**
 * @brief get current pointing
 */

static void md01_rot2prog_get_position(double *az, double *el)
{
	int n;

	char buf[ROT2PROG_ACK_BYTES];

	char get[ROT2PROG_CMD_BYTES] = "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1F\x20";
	char poll[ROT2PROG_CMD_BYTES] = "\x57\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x3F\x20";


	md01_rot2prog_serial_write(fd, get, ROT2PROG_CMD_BYTES, 0);

	n = md01_rot2prog_serial_read(fd, buf, ROT2PROG_ACK_BYTES);

	if (n != ROT2PROG_ACK_BYTES) {
		g_warning(MSG "mismatch in message length. expected %d, got %d",
			       ROT2PROG_ACK_BYTES, n);
		return;
	}




	md01_rot2prog_eval_response(buf, n);

	(*az) = md01.pos.az_cur;
	(*el) = md01.pos.el_cur;



	md01_rot2prog_serial_write(fd, poll, ROT2PROG_CMD_BYTES, 0);

	n = md01_rot2prog_serial_read(fd, buf, 2);

	if (n != 2) {
		g_warning(MSG "mismatch in message length. expected %d, got %d",
			       ROT2PROG_ACK_BYTES, n);
		return;
	}



}


/**
 * @brief push drive position to client
 */

static void md01_rot2prog_notify_pos_update(void)
{
	double az_arcsec;
	double el_arcsec;

	struct getpos pos;



	md01_rot2prog_get_position(&az_arcsec, &el_arcsec);

	/* value is in degrees, convert */
	az_arcsec *= 3600.0;
	el_arcsec *= 3600.0;

	/* emit notification */
	pos.az_arcsec = (typeof(pos.az_arcsec)) az_arcsec;
	pos.el_arcsec = (typeof(pos.el_arcsec)) el_arcsec;
	ack_getpos_azel(PKT_TRANS_ID_UNDEF, &pos);
}


/**
 * @brief move to parking position
 */

G_MODULE_EXPORT
void be_park_telescope(void)
{
	if (!md01.park.is_conf) {
		net_server_broadcast_message("Cannot park telescpe, "
				     "no parking position configured.", NULL);
		return;
	}

	net_server_broadcast_message("Initiating move to park position.", NULL);


	/* go to configured position */
	g_message(MSG "parking telescope");
	be_moveto_azel(md01.park.az, md01.park.el);
}


/**
 * @brief recalibrate pointing
 */

G_MODULE_EXPORT
void be_recalibrate_pointing(void)
{
	g_warning(MSG "Automatic drive recalibration is not available "
		      "for this backend device");
}


/**
 * @brief move the telescope to a certain azimuth and elevation
 */

G_MODULE_EXPORT
int be_moveto_azel(double az, double el)
{
	if (md01_rot2prog_moveto(az, el)) {
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
	md01_rot2prog_get_position(az, el);

	return 0;
}


/**
 * @brief get telescope drive capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_drive(struct capabilities *c)
{
	/* TODO we want the actual limits to be retrieved
	 * from the controller (if possible) rather than
	 * the configured limits
	 */

	c->az_min_arcsec = (int32_t) (3600.0 * md01.az_limits.left);
	c->az_max_arcsec = (int32_t) (3600.0 * md01.az_limits.right);
	c->az_res_arcsec = (int32_t) (3600.0 * md01.res.h);

	c->el_min_arcsec = (int32_t) (3600.0 * md01.el_limits.lower);
	c->el_max_arcsec = (int32_t) (3600.0 * md01.el_limits.upper);
	c->el_res_arcsec = (int32_t) (3600.0 * md01.res.v);

	return 0;
}

/**
 * dummy rotation push push thread (1/s)
 *
 * maybe trigger something like this once per rotation command
 */
static gpointer md01_rot2prog_pos_push_thread(gpointer data)
{
	while (1) {
		md01_rot2prog_notify_pos_update();
		g_usleep(0.1 * G_USEC_PER_SEC);
	}


}

/**
 * @brief extra initialisation function
 */

G_MODULE_EXPORT
void module_extra_init(void)
{
	g_message(MSG "configuring serial link");

	fd = md01_rot2prog_serial_open_port(md01_tty);
	if (!fd)
		g_error(MSG "Error opening serial port %s\n", md01_tty);

	if (md01_rot2prog_serial_set_comm_param(fd))
		g_error(MSG "Error setting parameters for serial port %s\n",
			md01_tty);

         md01_rot2prog_serial_flush(fd);
#if 1
	/* dummy */
	g_thread_new(NULL, md01_rot2prog_pos_push_thread, NULL);
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

	if (md01_rot2prog_load_config())
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");


	return NULL;
}
