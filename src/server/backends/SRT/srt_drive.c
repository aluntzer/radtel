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
	double pushrod_counts;		/* sensor count per inch of pushrod movement */

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

} srt;





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
		g_error(MSG "Invalid number of azimuth tilt corrections configured");
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


}


/**
 * @brief load the configuration file
 */

static int srt_drive_load_config(void)
{
	gsize len;
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;

	GError *error = NULL;



	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;


	ret = g_key_file_load_from_file(kf, "config/backends/srt_drive.cfg",
					flags, &error);

	if (!ret) {
		g_error(MSG "error loading config file %s", error->message);
		g_clear_error(&error);
		return -1;
	}

	srt_drive_load_keys(kf);

	g_key_file_free(kf);

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


	g_message(MSG "CMD: %s", cmd);

	be_shared_comlink_write(cmd, strlen(cmd));

	response = be_shared_comlink_read(&len);

	if (sscanf(response, "%c %d %d %d", &c, &cnts, &f1, &f2) != 4)
		g_warning(MSG, "error scanning com link response: %s", response);


	switch(c) {
	case 'M':
		g_message(MSG "CMD OK, %d counts, f1: %d, motor: %d",
			  cnts, f1, f2);
		ret = (double) cnts + (double) f1 * 0.5;
		break;

	case 'T':
		g_message(MSG "CMD TIMEOUT, %d counts, motor: %d, f2: %d",
			  cnts, f1, f2);
		ret = 0.0;
		break;
	default:
		/* XXX: issue cmd_drive_error(); */
		g_message(MSG, "error in com link response: %s", response);
		ret = 0.0;
		break;
	}


	g_free(response);

	return ret;
}


/**
 * @brief command the drive motors
 */

static void srt_drive_cmd_motors(double *az_cnt, double *el_cnt)
{
	const int azc = (int) (*az_cnt);
	const int elc = (int) (*el_cnt);

	gchar *cmd;


	g_message(MSG "rotating AZ/EL counts: %d %d", azc, elc);

	/* azimuth drive */
	if (azc) {
		cmd = g_strdup_printf("move %d %d\n",
				      srt_drive_get_az_motor_id(azc),
				      abs(azc));
		(*az_cnt) = copysign(srt_drive_motor_cmd_eval(cmd), azc);
		g_free(cmd);
	}

	/* elevation drive */
	if (elc) {
		cmd = g_strdup_printf("move %d %d\n",
				      srt_drive_get_el_motor_id(elc),
				      abs(elc));
		(*el_cnt) = copysign(srt_drive_motor_cmd_eval(cmd), elc);
		g_free(cmd);
	}
}


/**
 * @brief move the telescope
 *
 * @returns 0 on completion, 1 if more moves are pending
 */

static int srt_drive_move(void)
{
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
	srt_drive_cmd_motors(&d_az_cnt, &d_el_cnt);
	be_shared_comlink_release();

	/* update current sensor count */
	srt.pos.az_cnts += d_az_cnt;
	srt.pos.el_cnts += d_el_cnt;

	/* update current drive reference coordinates */
	srt.pos.az_cur = srt_drive_az_from_counts(srt.pos.az_cnts);
	srt.pos.el_cur = srt_drive_el_from_cassi_counts(srt.pos.el_cnts);


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
	while (1) {
		g_mutex_lock(&mutex);

		g_message(MSG "waiting for new coordinate input");

		g_cond_wait(&cond, &mutex);

		g_message(MSG "coordinates updated %g %g",
			  srt.pos.az_tgt, srt.pos.el_tgt);

		/* move until complete */
		while (srt_drive_move());

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
	double az_cnts;
	double el_cnts;


	az += srt_drive_az_tilt_corr(az, el);
	el += srt_drive_el_tilt_corr(az);

	az = srt_drive_map_az(az);

	if (srt_drive_check_limits(az, el))
		return -1;


	g_message(MSG "rotating to AZ/EL %g/%g", az, el);

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

	g_mutex_lock(&mutex);

	be_shared_comlink_acquire();


	g_message(MSG "current sensor counts: AZ: %g EL: %g",
		  srt.pos.az_cnts, srt.pos.el_cnts);


	/* move to stow in azimuth */
	if (srt_drive_motor_cmd_eval("move 0 5000\n")) {
		srt.pos.az_cur  = 0.0;
		srt.pos.az_tgt  = 0.0;
		srt.pos.az_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in azimuth");
	}


	/* move to stow in elevation */
	if (srt_drive_motor_cmd_eval("move 2 5000\n")) {
		srt.pos.el_cur  = 0.0;
		srt.pos.el_tgt = 0.0;
		srt.pos.el_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in elevation");
	}

	be_shared_comlink_release();
	g_mutex_unlock(&mutex);

	g_thread_exit(NULL);
}


/**
 * @brief thread function to recalibrate the telescope pointing
 */

static gpointer srt_recal_thread(gpointer data)
{

	g_mutex_lock(&mutex);

	g_message(MSG "current sensor counts: AZ: %g EL: %g",
		  srt.pos.az_cnts, srt.pos.el_cnts);

	be_shared_comlink_acquire();
	/* move to stow in azimuth */
	if (srt_drive_motor_cmd_eval("move 0 5000\n")) {
		srt.pos.az_cur  = 0.0;
		srt.pos.az_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in azimuth");
	}


	/* move to stow in elevation */
	if (srt_drive_motor_cmd_eval("move 2 5000\n")) {
		srt.pos.el_cur  = 0.0;
		srt.pos.el_cnts = 0.0;
	} else {
		g_message(MSG "unexpected response while stowing in elevation");
	}

	be_shared_comlink_release();

	/* rotate back to where we are supposed to be */
	g_cond_signal(&cond);
	g_mutex_unlock(&mutex);

	g_thread_exit(NULL);
}


/**
 * @brief move to parking position
 */

G_MODULE_EXPORT
void be_park_telescope(void)
{
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
		g_warning(MSG "Error loading module configuration, this plugin may not function properly.");

	srt_drive_cassi_set_pushdrod_zero_len_counts();
	srt_drive_set_az_center();

	return NULL;
}
