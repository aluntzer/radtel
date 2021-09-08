/**
 * @file    server/backends/SRT/srt_drive.c
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
#include <ack.h>

#include <net.h>


#define MSG "SRT DRIVE: "

#define DEG(x) ((x) / G_PI * 180.0)
#define RAD(x) ((x) * G_PI / 180.0)

static GThread *thread;
static GCond cond;
static GMutex mutex;

/* length units in inches */
static struct {

	/* Azimuth movement limits of the drive. The left value is the
	 * pointing reset value at STOW */
	struct {
		double left;
		double right;
	} az_limits;

	/* Elevation movement limits of the drive. The lower value is the
	 * pointing reset value at STOW */
	struct {
		double lower;
		double upper;
	} el_limits;

	/* see SRT Memo #002 for tilt corrections */
	struct {
		double az_axis_tilt;
		double az_rot_axis_sky;
		double el_axis_tilt;
	} axis_tilt;

	/* configuration of the CASSI drive */

	double az_counts_per_deg;	/* sensor counts per deg. of azimuth */

	double pushrod_len;		/* length of the rigid arm */
	double pushrod_joint;		/* distance of pushrods upper joint to
					 * the elevation axis
					 */
	double pushrod_collar;		/* pushrod collar offset */
	double pushrod_horizon_angle;	/* angle of the rigid arm at horizon */
	double pushrod_counts;		/* sensor count per inch of pushrod
					 * movement
					 */

	enum {AZ_CENTER_NORTH, AZ_CENTER_SOUTH} az_center;

	double pushrod_zero_len;	/* number of counts at lower elevation
					 * limit pushrod position
					 */

	double max_counts_per_move_cmd;	/* maximum number of sensor counts to
					 * drive in any given axis for a single
					 * move command
					 */
	/* coordinates in drive reference frame */
	struct {
		double az_cur;		/* current azimuth */
		double el_cur;		/* current elevation */

		double az_tgt;		/* target azimuth */
		double el_tgt;		/* target elevation */

		double az_cnts;		/* azimuth sensor counts */
		double el_cnts;		/* elevation sensor counts */
	} pos;

	/* the SRT's hot load (noise diode, vane calibrator not implemented)
	 * is part of the drive controls
	 */
	gdouble hot_load_temp;		/* hot load temperature */
	gboolean hot_load_ena;		/* hot load enable/disable */
	gboolean hot_load_state;	/* last known hot load status */


} srt;



static void srt_drive_notify_pos_update(void);


/**
 * @brief determine the azimuth center reference
 */

static void srt_drive_set_az_center(void)
{

	if (srt.az_limits.left < srt.az_limits.right) {
		if (srt.az_limits.right < 360.0) {
			srt.az_center = AZ_CENTER_SOUTH;
			g_message(MSG "Setting azimuth center to south");

			return;
		}
	}

	srt.az_center = AZ_CENTER_NORTH;

	if (srt.az_limits.right < 360.)
		srt.az_limits.right += 360.0;

	g_message(MSG "Setting azimuth center to north");
}


/**
 * @brief get the length of the pushrod at position zero
 *
 * @note this corresponds to the position at the lower elevation limit
 * @note this is for the drive's reference frame
 */

static double srt_drive_cassi_set_pushdrod_zero_len_counts(void)
{
	double a;
	double b;
	double c;
	double d;
	double e;

	double rod_zero_len;



	a = srt.pushrod_len * srt.pushrod_len;

	b = srt.pushrod_joint * srt.pushrod_joint;

	c = srt.pushrod_collar * srt.pushrod_collar;

	d = srt.pushrod_len * srt.pushrod_joint;

	e = cos(RAD(srt.pushrod_horizon_angle));


	rod_zero_len = a + b - c - 2.0 * d * e;

	if (rod_zero_len > 0.0)
		rod_zero_len = sqrt(rod_zero_len);
	else
		rod_zero_len = 0.0;

	srt.pushrod_zero_len = rod_zero_len;
}


/**
 * @brief load configuration keys
 */

static void srt_drive_load_keys(GKeyFile *kf)
{
	gsize len;

	gchar *model;

	double *tmp;

	GError *error = NULL;


	model = g_key_file_get_string(kf, "Drive", "model", &error);

	if (error)
		g_error(error->message);

	if (g_strcmp0(model, "CASSI"))
		g_error(MSG "SRT mounts other than CASSI not supported.");


	tmp = g_key_file_get_double_list(kf, model,
					   "az_limits", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of azimuth limits configured.");

	srt.az_limits.left  = tmp[0];
	srt.az_limits.right = tmp[1];
	g_free(tmp);

	tmp = g_key_file_get_double_list(kf, model,
					   "el_limits", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of elevation limits configured.");

	srt.el_limits.lower = tmp[0];
	srt.el_limits.upper = tmp[1];
	g_free(tmp);


	srt.az_counts_per_deg = g_key_file_get_double(kf, model,
					"az_counts_per_deg", &error);
	if (error)
		g_error(error->message);

	srt.pushrod_len = g_key_file_get_double(kf, model,
					"pushrod_len", &error);
	if (error)
		g_error(error->message);

	srt.pushrod_joint = g_key_file_get_double(kf, model,
					"pushrod_joint", &error);
	if (error)
		g_error(error->message);

	srt.pushrod_collar = g_key_file_get_double(kf, model,
					"pushrod_collar", &error);
	if (error)
		g_error(error->message);

	srt.pushrod_horizon_angle = g_key_file_get_double(kf, model,
					"pushrod_horizon_angle", &error);
	if (error)
		g_error(error->message);

	srt.pushrod_counts = g_key_file_get_double(kf, model,
					 "pushrod_counts", &error);
	if (error)
		g_error(error->message);

	tmp = g_key_file_get_double_list(kf, model,
					   "az_axis_tilt", &len, &error);
	if (len != 2)
		g_error(MSG "Invalid number of azimuth tilt corrections "
			     "configured");
	if (error)
		g_error(error->message);

	srt.axis_tilt.az_axis_tilt    = tmp[0];
	srt.axis_tilt.az_rot_axis_sky = tmp[1];
	g_free(tmp);


	srt.axis_tilt.el_axis_tilt = g_key_file_get_double(kf, model,
					    "el_axis_tilt", &error);
	if (error)
		g_error(error->message);


	srt.max_counts_per_move_cmd = g_key_file_get_double(kf, model,
					"max_counts_per_move_cmd", &error);
	if (error)
		g_error(error->message);


	srt.hot_load_temp = g_key_file_get_double(kf, model,
						  "hot_load_temp", &error);
	if (error) {
		g_error(error->message);
	}
}


/**
 * @brief load the srt_drive configuration file from a given prefix
 *
 * @returns 0 on success, otherwise error
 */

static int srt_drive_load_config_from_prefix(const gchar *prefix, GError **err)
{
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;


	gchar *cfg;


	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

	cfg = g_strconcat(prefix, "backends/srt_drive.cfg", NULL);
	ret = g_key_file_load_from_file(kf, cfg, flags, err);


	if (!ret) {
		g_key_file_free(kf);
		g_free(cfg);
		return -1;
	}

	g_message(MSG "Configuration file loaded from %s", cfg);

	srt_drive_load_keys(kf);

	g_key_file_free(kf);
	g_free(cfg);

	return 0;
}


/**
 * @brief try to load a srt_drive configuration file from various paths
 */

int srt_drive_load_config(void)
{
	int ret;

	gchar *prefix;

	GError *error = NULL;



	/* search relative path first */
	ret = srt_drive_load_config_from_prefix("/", &error);
	if (ret) {
		g_clear_error(&error);
		/* try again in sysconfdir */
		prefix = g_strconcat(SYSCONFDIR, "/", CONFDIR, "/", NULL);
		ret = srt_drive_load_config_from_prefix(prefix, &error);
		g_free(prefix);
	}

	if (ret) {
		g_warning(MSG "Could not find backends/srt_drive.cfg: %s. "
			  "Looked in ./, %s and %s/%s",
			  error->message, CONFDIR, SYSCONFDIR, CONFDIR);
		g_clear_error(&error);

		return -1;
	}

	return 0;
}




/**
 * @brief azimuth correction for tilt in azimut axis
 */

double srt_drive_az_tilt_corr(double az, double el)
{

	double a;
	double b;


	if (el >= 90.0)
		return 0.0;


	a  = RAD(srt.axis_tilt.az_axis_tilt);
	a *= sin(RAD(az - srt.axis_tilt.az_rot_axis_sky));
	a += RAD(srt.axis_tilt.el_axis_tilt);
	a *= tan(RAD(el));
	a  = atan(a);

	b  = RAD(srt.axis_tilt.az_axis_tilt);
	b *= sin(RAD(srt.az_limits.left
		     - srt.axis_tilt.az_rot_axis_sky));
	b += RAD(srt.axis_tilt.el_axis_tilt);
	b *= tan(RAD(srt.el_limits.lower));
	b  = atan(b);

	return DEG(a - b);
}


/**
 * @brief elevation correction for tilt in azimut axis
 */

double srt_drive_el_tilt_corr(double az)
{
	double a;
	double b;


	if (srt.axis_tilt.az_axis_tilt == 0.0)
		return 0.0;


	a = cos(RAD(az - srt.axis_tilt.az_rot_axis_sky));
	b = cos(RAD((srt.az_limits.left
		     - srt.axis_tilt.az_rot_axis_sky)));

	return srt.axis_tilt.az_axis_tilt * DEG(a - b);
}


/**
 * @brief map azimuth into proper range
 */

static double srt_drive_map_az(double az)
{
	az = fmod(az, 360.0);

	if (srt.az_center == AZ_CENTER_NORTH) {

		/* put azimut into a range from 180 to 540 */

		az += 360.0;

		if(az > 540.0)
			az -= 360.0;

		if(az < 180.0)
			az += 360.0;
	}

	return az;
}


/**
 * @brief check if coordinates are within hardware limits
 *
 * @returns -1 on error, 0 otherwise
 */

static int srt_drive_check_limits(double az, double el)
{
	g_message(MSG "check if AZ/EL %g/%g is within limits", az, el);

	if (az > srt.az_limits.right) {
		g_warning(MSG "error, az %g > %g", az, srt.az_limits.right);
		return -1;
	}

	if (az < srt.az_limits.left) {
		g_warning(MSG "error, az %g < %g", az, srt.az_limits.left);
		return -1;
	}

	if (el < srt.el_limits.lower) {
		g_warning(MSG "error, el %g < %g", el, srt.el_limits.lower);
		return -1;
	}

	if (el > srt.el_limits.upper) {
		g_warning(MSG "error, el %g < %g", el, srt.el_limits.upper);
		return -1;
	}

	return 0;
}


/**
 * @brief azimuth to drive reference frame
 */

static double srt_drive_az_to_drive_ref(double az)
{
	return az - srt.az_limits.left;
}


/**
 * @brief azimuth to telescope reference frame
 */

static double srt_drive_az_to_telescope_ref(double az)
{
	return az + srt.az_limits.left;
}


/**
 * @brief elevation to drive reference frame
 */

static double srt_drive_el_to_drive_ref(double el)
{
	return el - srt.el_limits.lower;
}


/**
 * @brief elevation to telescope reference frame
 */

static double srt_drive_el_to_telescope_ref(double el)
{
	return el + srt.el_limits.lower;
}


/**
 * @brief get number of azimuth sensor counts
 * @note az must be in drive reference frame
 */

static double srt_drive_az_counts(double az)
{
	return az * srt.az_counts_per_deg;
}


/**
 * @brief get drive reference azimuth from sensor count
 */

static double srt_drive_az_from_counts(double az_cnts)
{
	return az_cnts / srt.az_counts_per_deg;
}


/**
 * @brief get the number of elevation sensor counts for the CASSI drive
 * @note az, el must be in drive reference frame
 */

static double srt_drive_cassi_el_counts(double el)
{
	double a;
	double b;
	double c;
	double d;
	double e;

	double el_cnts;


	a = srt.pushrod_len * srt.pushrod_len;

	b = srt.pushrod_joint * srt.pushrod_joint;

	c = srt.pushrod_collar * srt.pushrod_collar;

	d = srt.pushrod_len * srt.pushrod_joint;

	e = cos(RAD(srt.pushrod_horizon_angle - el));

	el_cnts = a + b - c - 2.0 * d * e;


	if (el_cnts >= 0.0) {
		el_cnts = (srt.pushrod_zero_len - sqrt(el_cnts));
		el_cnts *= srt.pushrod_counts;
	} else {
		el_cnts = 0.0;
	}

	return el_cnts;
}


/**
 * @brief get drive reference elevation from CASSI sensor count
 */

static double srt_drive_el_from_cassi_counts(double el_cnts)
{

	double a;
	double b;
	double c;
	double d;
	double e;
	double f;


	if (el_cnts <= 0.0)
		return 0.0;

	a = srt.pushrod_len * srt.pushrod_len;

	b = srt.pushrod_joint * srt.pushrod_joint;

	c = srt.pushrod_collar * srt.pushrod_collar;

	d = srt.pushrod_len * srt.pushrod_joint;

	e = srt.pushrod_zero_len - el_cnts / srt.pushrod_counts;

	f = -0.5 * (e * e - a - b + c) / d;


	return srt.pushrod_horizon_angle - DEG(acos(f));
}


/**
 * @brief check if the drive can rotate give the input counts
 *
 * @brief returns 0 if both of the counts are smaller than 1, bits set
 *	  otherwise
 */

static int srt_drive_done(double az_cnts, double el_cnts)
{
	return ((fabs(az_cnts) < 1.0) && (fabs(el_cnts) < 1.0));
}


/**
 * @brief adjust sensor counts to be at most max_counts_per_move_cmd
 */

static double srt_drive_adjust_counts(double cnts)
{
	if (fabs(cnts) > srt.max_counts_per_move_cmd)
		return copysign(srt.max_counts_per_move_cmd, cnts);

	return cnts;
}


/**
 * @brief get the id of the azimuth motor direction
 *
 * @returns 0 for counter-clockwise (left) azimuth, 1 for CW (right) azimuth
 */

static int srt_drive_get_az_motor_id(double cnts)
{
	if (cnts > 0.0)
		return 1;

	return 0;
}


/**
 * @brief get the id of the elvation motor direction
 *
 * @returns 2 for down elevation, 3 for up elevation
 */

static int srt_drive_get_el_motor_id(double cnts)
{
	if (cnts > 0.0)
		return 3;

	return 2;
}


/**
 * @brief command the drive motors via the shared com link and evaluate
 *	  the response
 *
 * @return total number of counts + halfcounts driven (see also SRT MEMO #022)
 */

static double srt_drive_motor_cmd_eval(gchar *cmd)
{
	gsize len;

	gchar c  = '\0';
	int cnts = 0;
	int f1   = 0;
	int f2   = 0;

	double ret;

	gchar *response;


	g_debug(MSG "CMD: %s", cmd);


	be_shared_comlink_write(cmd, strlen(cmd));

	response = be_shared_comlink_read(&len);

	if (sscanf(response, "%c %d %d %d", &c, &cnts, &f1, &f2) != 4)
		g_warning(MSG, "error scanning com link response: %s", response);


	switch(c) {
	case 'M':
		g_debug(MSG "CMD OK, %d counts, f1: %d, motor: %d",
			  cnts, f1, f2);
		ret = (double) cnts + (double) f1 * 0.5;
		break;

	case 'C':
		g_debug(MSG "CMD OK, diode command was %d", f1);
		break;

	case 'T':
		g_warning(MSG "CMD TIMEOUT, %d counts, motor: %d, f2: %d",
			  cnts, f1, f2);
		ret = 0.0;
		break;

	default:
		/* XXX: issue cmd_drive_error(); */
		g_warning(MSG "error in com link response: %s", response);
		ret = 0.0;
		break;
	}


	g_free(response);

	return ret;
}


/**
 * @brief command the drive motors
 */

static int srt_drive_cmd_motors(double *az_cnt, double *el_cnt)
{
	const int azc = (int) (*az_cnt);
	const int elc = (int) (*el_cnt);

	gchar *cmd;

	GTimer *tm;

	struct status s;


	tm = g_timer_new();


	g_message(MSG "rotating AZ/EL counts: %d %d", azc, elc);

	/* azimuth drive */
	if (azc) {
		cmd = g_strdup_printf("move %d %d\n",
				      srt_drive_get_az_motor_id(azc),
				      abs(azc));

		s.busy = 1;
		s.eta_msec = (typeof(s.eta_msec)) 174.1202 * fabs(*az_cnt); /** XXX add config file entry **/
		ack_status_slew(PKT_TRANS_ID_UNDEF, &s);

		g_timer_start(tm);
		(*az_cnt) = copysign(srt_drive_motor_cmd_eval(cmd), azc);
		g_timer_stop(tm);

		s.busy = 0;
		s.eta_msec = 0;
		ack_status_slew(PKT_TRANS_ID_UNDEF, &s);

		g_free(cmd);

		if ((*az_cnt) == 0.0)
			return -1;

		g_message("Azimuth: %f sec/count", g_timer_elapsed(tm, NULL) / (*az_cnt));
	}

	/* elevation drive */
	if (elc) {
		cmd = g_strdup_printf("move %d %d\n",
				      srt_drive_get_el_motor_id(elc),
				      abs(elc));

		/* note: assuming worst-case speed */
		s.busy = 1;
		s.eta_msec = (typeof(s.eta_msec)) 304.33578 * fabs(*el_cnt); /** XXX add config file entry **/
		ack_status_slew(PKT_TRANS_ID_UNDEF, &s);

		g_timer_start(tm);
		(*el_cnt) = copysign(srt_drive_motor_cmd_eval(cmd), elc);
		g_timer_stop(tm);

		s.busy = 0;
		s.eta_msec = 0;
		ack_status_slew(PKT_TRANS_ID_UNDEF, &s);

		g_free(cmd);

		if ((*el_cnt) == 0.0)
			return -1;

		g_message("Elevation: %f sec/count", g_timer_elapsed(tm, NULL) / (*el_cnt));
	}

	g_timer_destroy(tm);

	return 0;
}


/**
 * @brief move the telescope
 *
 * @returns 0 on completion, 1 if more moves are pending
 */

static int srt_drive_move(void)
{
	int ret;

	double az_cnt;
	double el_cnt;

	double d_az_cnt;
	double d_el_cnt;


	/* absolute sensor counts */
	az_cnt = srt_drive_az_counts(srt.pos.az_tgt);
	el_cnt = srt_drive_cassi_el_counts(srt.pos.el_tgt);

	/* delta counts rounded to nearest integer*/
	d_az_cnt = (int) (srt_drive_adjust_counts(az_cnt - srt.pos.az_cnts));
	d_el_cnt = (int) (srt_drive_adjust_counts(el_cnt - srt.pos.el_cnts));

	/* already there? */
	if (srt_drive_done(d_az_cnt, d_el_cnt))
		return 0;

	be_shared_comlink_acquire();
	ret = srt_drive_cmd_motors(&d_az_cnt, &d_el_cnt);
	be_shared_comlink_release();

	if (ret) {

		net_server_broadcast_message("Somebody crashed into a wall, "
					     "initiating recovery procedure.",
					     NULL);

		g_warning(MSG "timeout occured, starting recovery procedure");

		be_park_telescope();
		return 0;
	}

	/* update current sensor count */
	srt.pos.az_cnts += d_az_cnt;
	srt.pos.el_cnts += d_el_cnt;

	/* update current drive reference coordinates */
	srt.pos.az_cur = srt_drive_az_from_counts(srt.pos.az_cnts);
	srt.pos.el_cur = srt_drive_el_from_cassi_counts(srt.pos.el_cnts);

	srt_drive_notify_pos_update();

	g_message(MSG "now at telescope AZ/EL: %g %g",
		  srt_drive_az_to_telescope_ref(srt.pos.az_cur),
		  srt_drive_el_to_telescope_ref(srt.pos.el_cur));


	return 1;
}


/**
 * @brief thread function that does all the regular moving/tracking work
 */

static gpointer srt_drive_thread(gpointer data)
{
	double az_cnt;
	double el_cnt;

	struct status s;


	while (1) {
		g_mutex_lock(&mutex);

		g_message(MSG "waiting for new coordinate input");

		g_cond_wait(&cond, &mutex);

		g_message(MSG "coordinates updated %g %g",
			  srt.pos.az_tgt, srt.pos.el_tgt);

		az_cnt = fabs(srt_drive_az_counts(srt.pos.az_tgt) - srt.pos.az_cnts);
		el_cnt = fabs(srt_drive_cassi_el_counts(srt.pos.el_tgt) - srt.pos.el_cnts);

		s.busy = 1;
		s.eta_msec = (typeof(s.eta_msec)) 174.1202 * az_cnt + 304.33578 * el_cnt; /** XXX config file! **/
		ack_status_move(PKT_TRANS_ID_UNDEF, &s);

		/* move until complete */
		while (srt_drive_move());

		s.busy = 0;
		s.eta_msec = 0;
		ack_status_move(PKT_TRANS_ID_UNDEF, &s);

		g_mutex_unlock(&mutex);
	}
}


/**
 * @brief rotate the mount to coordinates
 *
 * @returns -1 on error, 0 otherwise
 */

static int srt_drive_moveto(double az, double el)
{
	ack_moveto_azel(PKT_TRANS_ID_UNDEF, az, el);

	az += srt_drive_az_tilt_corr(az, el);
	el += srt_drive_el_tilt_corr(az);

	az = srt_drive_map_az(az);

	if (srt_drive_check_limits(az, el))
		return -1;


	g_debug(MSG "rotating to AZ/EL %g/%g", az, el);

	srt.pos.az_tgt = srt_drive_az_to_drive_ref(az);
	srt.pos.el_tgt = srt_drive_el_to_drive_ref(el);

	if (g_mutex_trylock(&mutex)) {
		g_cond_signal(&cond);
		g_mutex_unlock(&mutex);
	}

	return 0;
}


/**
 * @brief thread function to park the telescope
 */

static gpointer srt_park_thread(gpointer data)
{
	double msec;
	struct status s;

	g_mutex_lock(&mutex);

	be_shared_comlink_acquire();


	g_message(MSG "current sensor counts: AZ: %g EL: %g",
		  srt.pos.az_cnts, srt.pos.el_cnts);

	s.busy = 1;
	/** XXX config file! **/
	msec = 174.1202 * srt.pos.az_cnts + 304.33578 * srt.pos.el_cnts;

	/** worst-case scenario: full rotation and then some **/
	if (msec == 0.0)
		msec = 174.1202 * 5000. + 304.33578 * 5000.;

	s.eta_msec = (typeof(s.eta_msec)) msec;
	ack_status_move(PKT_TRANS_ID_UNDEF, &s);
	ack_status_slew(PKT_TRANS_ID_UNDEF, &s);


	/* move to stow in azimuth */
	if (srt_drive_motor_cmd_eval("move 0 5000\n") == 0.0) {
		srt.pos.az_cur  = 0.0;
		srt.pos.az_tgt  = 0.0;
		srt.pos.az_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in azimuth");
	}


	/* move to stow in elevation */
	if (srt_drive_motor_cmd_eval("move 2 5000\n") == 0.0) {
		srt.pos.el_cur  = 0.0;
		srt.pos.el_tgt = 0.0;
		srt.pos.el_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in elevation");
	}

	be_shared_comlink_release();
	g_mutex_unlock(&mutex);

	srt_drive_notify_pos_update();

	s.busy = 0;
	s.eta_msec = 0;
	ack_status_move(PKT_TRANS_ID_UNDEF, &s);
	ack_status_slew(PKT_TRANS_ID_UNDEF, &s);


	net_server_broadcast_message("Telescope is now at park position.",
				     NULL);


	g_thread_exit(NULL);
}


/**
 * @brief thread function to recalibrate the telescope pointing
 */

static gpointer srt_recal_thread(gpointer data)
{
	double msec;
	struct status s;

	g_mutex_lock(&mutex);

	g_message(MSG "current sensor counts: AZ: %g EL: %g",
		  srt.pos.az_cnts, srt.pos.el_cnts);

	s.busy = 1;
	/** XXX config file! **/
	msec = 174.1202 * srt.pos.az_cnts + 304.33578 * srt.pos.el_cnts;

	/** worst-case scenario: full rotation and then some **/
	if (msec == 0.0)
		msec = 174.1202 * 5000. + 304.33578 * 5000.;

	s.eta_msec = (typeof(s.eta_msec)) msec;
	ack_status_move(PKT_TRANS_ID_UNDEF, &s);
	ack_status_slew(PKT_TRANS_ID_UNDEF, &s);


	be_shared_comlink_acquire();
	/* move to stow in azimuth */
	if (srt_drive_motor_cmd_eval("move 0 5000\n") == 0.0) {
		srt.pos.az_cur  = 0.0;
		srt.pos.az_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in azimuth");
	}


	/* move to stow in elevation */
	if (srt_drive_motor_cmd_eval("move 2 5000\n") == 0.0) {
		srt.pos.el_cur  = 0.0;
		srt.pos.el_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in elevation");
	}

	be_shared_comlink_release();

	/* rotate back to where we are supposed to be */
	g_cond_signal(&cond);
	g_mutex_unlock(&mutex);

	s.busy = 0;
	s.eta_msec = 0;
	ack_status_move(PKT_TRANS_ID_UNDEF, &s);
	ack_status_slew(PKT_TRANS_ID_UNDEF, &s);

	g_thread_exit(NULL);
}


/**
 * @brief thread function that toggles the state of the hot load
 *
 * @note this drives only the noise diode, just because I'm lazy
 * and did not add a config option for the calibration vane
 * (it's just IDs 4: vane out and 5: vane in instead of 6/7)
 *
 */

static gpointer srt_hot_load_thread(gpointer data)
{
	gchar *cmd;

	g_mutex_lock(&mutex);

	be_shared_comlink_acquire();


	g_message(MSG "Changing hot load state to %s",
		      srt.hot_load_ena ? "ON" : "OFF");


	if (srt.hot_load_ena)
		cmd = "move 7 0\n";	/* id: 7 -> diode on */
	else
		cmd = "move 6 0\n";	/* id: 6 -> diode off */

	/* toggle diode */
	if (srt_drive_motor_cmd_eval(cmd) == 0.0)
		srt.hot_load_state = srt.hot_load_ena;
	else
		g_message(MSG "unexpected response while changing hot load state");


	be_shared_comlink_release();
	g_mutex_unlock(&mutex);

	if (srt.hot_load_state)
		ack_hot_load_enable(PKT_TRANS_ID_UNDEF);
	else
		ack_hot_load_disable(PKT_TRANS_ID_UNDEF);


	g_thread_exit(NULL);
}


/**
 * @brief push drive position to server
 */

static void srt_drive_notify_pos_update(void)
{
	double az_arcsec;
	double el_arcsec;

	struct getpos pos;


	az_arcsec = srt_drive_az_to_telescope_ref(srt.pos.az_cur) * 3600.0;
	el_arcsec = srt_drive_el_to_telescope_ref(srt.pos.el_cur) * 3600.0;

	/* emit notification */
	pos.az_arcsec = (typeof(pos.az_arcsec)) az_arcsec;
	pos.el_arcsec = (typeof(pos.el_arcsec)) el_arcsec;
	ack_getpos_azel(PKT_TRANS_ID_UNDEF, &pos);
}


/**
 * @brief azimuth to telescope reference frame
 */

static void srt_toggle_hot_load(gboolean mode)
{
	/* if already in mode, just respond */
	if (mode == srt.hot_load_state) {

		if (srt.hot_load_state)
			ack_hot_load_enable(PKT_TRANS_ID_UNDEF);
		else
			ack_hot_load_disable(PKT_TRANS_ID_UNDEF);

		return;
	}

	/* target mode */
	srt.hot_load_ena = mode;


	g_thread_new(NULL, srt_hot_load_thread, NULL);
}




/**
 * @brief move to parking position
 */

G_MODULE_EXPORT
void be_park_telescope(void)
{
	net_server_broadcast_message("Moving telescope to park position, "
				     "please stand by.", NULL);

	g_message(MSG "parking telescope");

	g_thread_new(NULL, srt_park_thread, NULL);
}


/**
 * @brief recalibrate pointing
 */

G_MODULE_EXPORT
void be_recalibrate_pointing(void)
{
	g_warning(MSG "recalibrating pointing");

	g_thread_new(NULL, srt_recal_thread, NULL);
}


/**
 * @brief move the telescope to a certain azimuth and elevation
 */

G_MODULE_EXPORT
int be_moveto_azel(double az, double el)
{
	if (srt_drive_moveto(az, el)) {
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
	(*az) = srt_drive_az_to_telescope_ref(srt.pos.az_cur);
	(*el) = srt_drive_el_to_telescope_ref(srt.pos.el_cur);

	return 0;
}


/**
 * @brief get telescope drive capabilities
 */

G_MODULE_EXPORT
int be_get_capabilities_drive(struct capabilities *c)
{
	double el_drive_min;
	double el_drive_max;
	double el_drive_cnt;
	double el_drive_res;


	el_drive_min = srt_drive_el_to_drive_ref(srt.el_limits.lower);
	el_drive_max = srt_drive_el_to_drive_ref(srt.el_limits.upper);

	el_drive_cnt = srt_drive_cassi_el_counts(el_drive_max)
		     - srt_drive_cassi_el_counts(el_drive_min);

	el_drive_res = (el_drive_max - el_drive_min) / el_drive_cnt;


	c->az_min_arcsec = (int32_t) (3600.0 * srt.az_limits.left);
	c->az_max_arcsec = (int32_t) (3600.0 * srt.az_limits.right);
	c->az_res_arcsec = (int32_t) (3600.0 / srt.az_counts_per_deg);

	c->el_min_arcsec = (int32_t) (3600.0 * srt.el_limits.lower);
	c->el_max_arcsec = (int32_t) (3600.0 * srt.el_limits.upper);
	c->el_res_arcsec = (int32_t) (3600.0 * el_drive_res);

	return 0;
}


/**
 * @brief get telescope drive capabilities_load
 */

G_MODULE_EXPORT
int be_get_capabilities_load_drive(struct capabilities_load *c)
{
	double el_drive_min;
	double el_drive_max;
	double el_drive_cnt;
	double el_drive_res;


	el_drive_min = srt_drive_el_to_drive_ref(srt.el_limits.lower);
	el_drive_max = srt_drive_el_to_drive_ref(srt.el_limits.upper);

	el_drive_cnt = srt_drive_cassi_el_counts(el_drive_max)
		     - srt_drive_cassi_el_counts(el_drive_min);

	el_drive_res = (el_drive_max - el_drive_min) / el_drive_cnt;


	c->az_min_arcsec = (int32_t) (3600.0 * srt.az_limits.left);
	c->az_max_arcsec = (int32_t) (3600.0 * srt.az_limits.right);
	c->az_res_arcsec = (int32_t) (3600.0 / srt.az_counts_per_deg);

	c->el_min_arcsec = (int32_t) (3600.0 * srt.el_limits.lower);
	c->el_max_arcsec = (int32_t) (3600.0 * srt.el_limits.upper);
	c->el_res_arcsec = (int32_t) (3600.0 * el_drive_res);

	c->hot_load = (uint32_t) (srt.hot_load_temp * 1000.0);

	/* push along hot load status, so we ensure that
	 * any connecting client is informed immediately
	 */
	if (srt.hot_load_ena)
		ack_hot_load_enable(PKT_TRANS_ID_UNDEF);
	else
		ack_hot_load_disable(PKT_TRANS_ID_UNDEF);


	return 0;
}




/**
 * @brief hot load enable/disable
 */

G_MODULE_EXPORT
int be_hot_load_enable(gboolean mode)
{
	srt_toggle_hot_load(mode);

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

	g_message(MSG "starting drive slewing thread");

	thread = g_thread_new(NULL, srt_drive_thread, NULL);

	/* make sure the hot load is turned off on restart */
	srt.hot_load_state = TRUE;
	srt_toggle_hot_load(FALSE);

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
		g_warning(MSG "Error loading module configuration, "
			      "this plugin may not function properly.");

	srt_drive_cassi_set_pushdrod_zero_len_counts();
	srt_drive_set_az_center();

	
	return NULL;
}
