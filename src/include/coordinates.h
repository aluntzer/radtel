/**
 * @file   coordinates.c
 * @ingroup coordinates
 * @author Armin Luntzer (armin.luntzer@univie.ac.at)
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
 */

#ifndef COORDINATES_H
#define COORDINATES_H

#include <time.h>
#include <math.h>

#define RAD(x)		((x) * M_PI / 180.0)
#define DEG(x)		((x) * 180.0 / M_PI)
#define DEG_TO_HOUR(x)	((x) / 15.0)
#define HOUR_TO_DEG(x)	((x) * 15.0)

struct coord_horizontal {
	double az;
	double el;
};

struct coord_equatorial {
	double ra;
	double dec;
};

struct coord_galactic{
	double lat;
	double lon;
};


struct tm *get_UT(void);

time_t epoch(void);

time_t UT_seconds(void);

time_t time_since_epoch(void);

double UT_hours(void);

double daynumber(void);

double local_sidereal_time(double lon);

double julian_date(struct tm date);

struct coord_equatorial sun_ra_dec(void);
struct coord_equatorial moon_ra_dec(double lat, double lon);

struct coord_horizontal equatorial_to_horizontal(struct coord_equatorial eq,
						 double lat, double lon,
						 double hour_angle_shift);

struct coord_equatorial horizontal_to_equatorial(struct coord_horizontal hor,
						 double lat, double lon,
						 double hour_angle_shift);

struct coord_galactic horizontal_to_galactic(struct coord_horizontal hor,
					     double lat, double lon);

struct coord_equatorial galactic_to_equatorial(struct coord_galactic gal);

struct coord_horizontal galactic_to_horizontal(struct coord_galactic gal,
					       double lat, double lon,
					       double hour_angle_shift);

struct coord_galactic equatorial_to_galactic(struct coord_equatorial eq);

double vlsr(struct coord_equatorial eq, double d);

double doppler_freq_relative(double vel, double freq_ref);
double doppler_freq(double vel, double freq_ref);
double doppler_vel(double freq, double freq_ref);

#endif
