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
 * @defgroup coordinates Coordinate conversion and celestial objects
 *
 * @todo constants to defines where applicable, some cleanup/consistency
 */


#include <coordinates.h>


/**
 * @brief computes julian dates
 *
 * @param a struct tm, years in years since 1900, months: jan=1, feb=2,...
 *
 * @return  the julian date
 *
 * @note assumes gregorian calendar, so only valid for dates of
 *	 1582, October 15th or later
 *	 (after Duffett-Smith, section 4)
 */

double julian_date(struct tm date)
{
	int A, B, C, D;

	double JD;


	date.tm_year += 1900;

	if (!date.tm_mon || (date.tm_mon == 1)) {
		date.tm_year -= 1;
		date.tm_mon  += 11;
	}

	A = date.tm_year / 100;
	B = 2 - A + (A / 4);
	C = 365.25 * date.tm_year;
	D = 30.6001 * (date.tm_mon + 2);

	JD = B + C + D + date.tm_mday + 1720994.5;

	return JD;
}


/**
 * @brief get the seconds of the current epoch (2000)
 *
 * @return the seconds since the current epoch (2000)
 */

time_t epoch(void)
{
	struct tm epoch_date;

	/* tm_year: years since 1900: -> 2000; */
	epoch_date.tm_year = 100;
	epoch_date.tm_mon  = 0;
	epoch_date.tm_mday = 1;
	epoch_date.tm_hour = 0;
	epoch_date.tm_min  = 0;
	epoch_date.tm_sec  = 0;

	return mktime(&epoch_date);
}


/**
 * @brief get the current universal time
 *
 * @return a pointer to a gmtime()-internal static struct tm
 */

struct tm *get_UT(void)
{
	time_t current;


	time(&current);

	return gmtime(&current);
}


/**
 * @brief get seconds since unix epoch
 *
 * @return a time_t of the seconds since the unix epoch
 */

time_t UT_seconds(void)
{
	return mktime(get_UT());
}


/**
 * @brief get the current UT in hours
 *
 * @return the current UT in hours
 */

double UT_hours(void)
{
	struct tm *tm;


	tm = get_UT();

	return (double)(tm->tm_hour + (tm->tm_min + tm->tm_sec / 60.0) / 60.0);
}


/**
 * @brief get current seconds since the epoch (2000)
 *
 * @return the seconds since epoch (2000)
 */

time_t time_since_epoch()
{
	return difftime(UT_seconds(), epoch());
}


/**
 * @brief get the day number of the year
 *
 * @return the day number of the year
 */

double daynumber(void)
{
	double daynum;


	daynum = -365.5 + time_since_epoch() / 86400.0;

	return daynum;
}


/**
 * @brief get the local sidereal time in hours
 *
 * @return the local sidereal time in hours
 */

double local_sidereal_time(double lon)
{
	double lst;


	lst = 100.46 + 0.985647352 * daynumber() + UT_hours() * 15.0 - lon;

	lst = fmod(lst, 360.0) / 15.0;

	return lst;
}


/**
 * @brief convert equatorial to horizontal coordinates
 *
 * @param eq a struct coord_equatorial (values in hours and degrees)
 * @param lat the geographical latitude to convert for (in degrees)
 * @param lon the geographical longitude to convert for (in degrees)
 * @param hour_angle_shift an adjustment of the hour angle
 *
 * @return a struct coord_horizontal (values in degrees)
 */

struct coord_horizontal equatorial_to_horizontal(struct coord_equatorial eq,
						 double lat, double lon,
						 double hour_angle_shift)
{
	double hour_angle;

	struct coord_horizontal hor;


	hour_angle = local_sidereal_time(lon) - eq.ra + hour_angle_shift;

	if(hour_angle < 0.0)
		hour_angle += 24.0;

	hour_angle = HOUR_TO_DEG(hour_angle);

	lat	   = RAD(lat);
	eq.ra	   = RAD(eq.ra);
	eq.dec	   = RAD(eq.dec);
	hour_angle = RAD(hour_angle);

	hor.el = asin(sin(lat) * sin(eq.dec)
		      + cos(lat) * cos(eq.dec) * cos(hour_angle));

	hor.az = acos((sin(eq.dec) - sin(hor.el) * sin(lat))
		      / (cos(hor.el) * cos(lat)));

	hor.az = DEG(hor.az);
	hor.el = DEG(hor.el);

	if (sin(hour_angle) > 0.0)
		hor.az = 360.0 - hor.az;


	return hor;
}


/**
 * @brief convert horizontal to equatorial coordinates
 *
 * @param hor a struct coord_horizontal (values in degrees)
 * @param lat the geographical latitude to convert for (in degrees)
 * @param lon the geographical longitude to convert for (in degrees)
 * @param hour_angle_shift an adjustment of the hour angle
 *
 * @return a struct coord_equatorial (values in hours and degrees)
 */

struct coord_equatorial horizontal_to_equatorial(struct coord_horizontal hor,
						 double lat, double lon,
						 double hour_angle_shift)
{
	double hour_angle;

	struct coord_equatorial eq;


	hor.az = RAD(hor.az);
	hor.el = RAD(hor.el);
	lat    = RAD(lat);

	eq.dec = asin(sin(lat) * sin(hor.el)
		      + cos(lat) * cos(hor.el) * cos(hor.az));

	hor.az -= M_PI;

	hour_angle = atan2(sin(hor.az), (cos(hor.az) * sin (lat)
					 + tan(hor.el) * cos (lat)));

	hour_angle = DEG_TO_HOUR(DEG(hour_angle));
	eq.dec     = DEG(eq.dec);

	eq.ra = local_sidereal_time(lon) - hour_angle + hour_angle_shift;

	if(eq.ra < 0.0)
		eq.ra += 24.0;


	return eq;
}


/**
 * @brief convert equatorial to galactic coordinates
 *
 * @param eq a struct coord_equatorial (values in hours and degrees)
 *
 * @return a struct coord_galactic (values in degrees)
 */

struct coord_galactic equatorial_to_galactic(struct coord_equatorial eq)
{
	/* Right ascension of the north Galactic pole
	 * Declination of the north Galactic pole
	 * Longitude of the ascening node of the Galactic plane
	 * Epoch: J2000
	 */

	const double ra_pole = RAD(192.8594813);
	const double de_pole = RAD( 27.1282511);
	const double lon_asc =  33.0;

	struct coord_galactic gal;


	eq.ra  = HOUR_TO_DEG(eq.ra);
	eq.ra  = RAD(eq.ra);
	eq.dec = RAD(eq.dec);

	gal.lon = atan2(sin(eq.dec) * cos(de_pole)
			- cos(eq.dec) * cos(eq.ra - ra_pole) * sin(de_pole),
			cos(eq.dec) * sin(eq.ra - ra_pole));

	gal.lon = DEG(gal.lon) + lon_asc;

	if(gal.lon < 0.0)
		gal.lon +=360.;

	gal.lat = asin(cos(eq.dec) * cos(de_pole) * cos(eq.ra - ra_pole)
		       + sin(eq.dec) * sin(de_pole));

	gal.lat = DEG(gal.lat);


	return gal;
}


/**
 * @brief convert galactic to equatorial coordinates
 *
 * @param gal a struct coord_galactic (values in degrees)
 *
 * @return a struct coord_equatorial (values in hours and degrees)
 */

struct coord_equatorial galactic_to_equatorial(struct coord_galactic gal)
{
	/* Right ascension of the north Galactic pole
	 * Declination of the north Galactic pole
	 * Longitude of the ascening node of the Galactic plane
	 * Epoch: J2000
	 */

	const double ra_pole = RAD(192.8594813);
	const double de_pole = RAD(27.1282511);
	const double lon_asc = RAD(33.0);

	struct coord_equatorial eq;


	gal.lat = RAD(gal.lat);
	gal.lon = RAD(gal.lon);

	eq.ra  = atan2(cos(gal.lat) * cos(gal.lon-lon_asc),
		       sin(gal.lat) * cos(de_pole)
		       - cos(gal.lat) * sin(gal.lon - lon_asc) * sin(de_pole));

	eq.ra += ra_pole;

	eq.dec = asin(cos(gal.lat) * cos(de_pole) * sin(gal.lon - lon_asc)
		      + sin(gal.lat)*sin(de_pole));

	eq.ra  = DEG(eq.ra);
	eq.dec = DEG(eq.dec);
	eq.ra = DEG_TO_HOUR(eq.ra);


	return eq;
}


/**
 * @brief convert horizontal to galactic coordinates
 *
 * @param hor a struct coord_horizontal (values in degrees)
 * @param lat the latitude to convert for (in degrees)
 * @param lon the longitude to convert for (in degrees)
 *
 * @return a struct coord_galactic (values in degrees)
 */

struct coord_galactic horizontal_to_galactic(struct coord_horizontal hor,
					     double lat, double lon)
{
	struct coord_equatorial eq;


	eq = horizontal_to_equatorial(hor, lat, lon, 0.0);

	return equatorial_to_galactic(eq);
}


/**
 * @brief convert galactic to horizonta cloordinates
 *
 * @param gal a struct coord_galactic (values in degrees)
 * @param lat the geographical latitude to convert for (in degrees)
 * @param lon the geographical longitude to convert for (in degrees)
 * @param hour_angle_shift an adjustment of the hour angle
 *
 * @return a struct coord_horizontal (values in degrees)
 */

struct coord_horizontal galactic_to_horizontal(struct coord_galactic gal,
					       double lat, double lon,
					       double hour_angle_shift)
{
	struct coord_equatorial eq;


	eq = galactic_to_equatorial(gal);

	return equatorial_to_horizontal(eq, lat, lon, hour_angle_shift);
}


/**
 * @brief return the current equatorial coordinates of the Moon
 *
 * @param lat the geographical latitude
 * @param lon the geographical longitude
 *
 * @return a struct coord_equatorial (values in hours and degrees)
 *
 * @see  Astronomical Almanac page D2 Moon, 2017
 *
 * @bug defective, fixme
 */
#if 0
struct coord_equatorial moon_ra_dec(double lat, double lon)
{
	double d;
	double Lm;
	double Gamma;
	double Omega;
	double D;
	double M;

	double true_anomaly;
	double evection;
	double variation;
	double annual;
	double true_lunar_longitude;

	double hour_angle;

	double ra, dec;
	double x, y, z;
	double xx, yy, zz;

	const double obliquity_ecliptic = RAD(23.439);
	const double mean_lunar_inclination_to_ecliptic = RAD(5.1566898);
	const double lunar_eccentricity = 0.054900489;
	const double lunar_semimajor_axis_in_earth_radii = 384748.0 / 6371.0;


	struct coord_equatorial moon;


	hour_angle = local_sidereal_time(lon);

	/* fractional day number since  Jan 0, 0h TT (terrestrial time) */
	d = daynumber() + (365.2425 + 0.5) * 2.0 + 0.015;

	/* mean longitude of the Moon, measured in the the ecliptic to the mean
	 * ascending node and then along the orbit
	 * Lm  =  Long. of asc. node  + Arg. of perigee + Mean anomaly
	 */
	Lm = 303.974109 + 13.17639646 * d;
#if 0
	Ls = 279.583 + 0.985647 * d; //LSUN
#endif
	/* mean longitude of the lunar perigee, measured as for L */
	Gamma = 55.001710 + 0.11140343 * d;

	/* mean longitude of the mean ascending node of the lunar orbit
	 * on the ecliptic
	 */
	Omega = 156.281166 - 0.05295375 * d;

	/* mean elongation of the Moon from the Sun, D = Lm - Ls */
	D =  24.116028 + 12.19074910 * d;


	/* the mean lunar anomaly is the distance between the mean longitude of
	 * the moon to the mean longitude of the lunar perigee
	 */
	M = RAD(Lm - Gamma);

	/* The true anomaly can be expressed in terms of mean anomaly (M),
	 * eccentricity (e) and a Bessel function of the first kind.
	 * Series expansion in powers of e results in the equation of center:
	 * v - M = (2e - 1/4 e^3 + 5/96 e^5 + ...) * sin(M) + ...
	 * If we ignore any higher order terms, this boils down to
	 * v - M = 2e * sin(M)
	 * hence we get an approximation of the true anomaly to be
	 * v  = M + 2 e * sin (M)
	 */

	true_anomaly = /*RAD(M) +*/ 2.0 * lunar_eccentricity * sin(M);

	/* we only consider the 3 largest perturbations */

	/* Evection varies the longitude of the Moon with a period of 31.8 days,
	 * caused by gravitiational action of the Sun
	 */
#if 0
	evection = RAD(1.274) * sin(RAD(M - 2.*D));
#else
	evection = RAD(1.274) * sin(RAD(2.0 * D - Lm));
#endif


	/* Variation is the speed-up and slow-down observed as the moon
	 * approaches new-moon/full-moon and first/last quarter. This is a
	 * gravitational effect, not one of eccentricity.
	 */
	variation = RAD(0.658) * sin(RAD(2.0 * D));

	/* The Moon's orbit slightly expands when the Earth is at the perihelion
	 * of its orbit and the Sun's perturbations are strongest, and vice
	 * versa when the Earth is at the aphelion.
	 */
	annual = RAD(-0.186) * sin(RAD(357.528 + 0.9856003 * d));

	true_lunar_longitude = RAD(Lm)+ true_anomaly /*+ evection  + variation + annual*/;
#if 0
        RAD(Lm - Omega) + true_anomaly + evection+ variation;
#endif

	/* transform to topographical ra/dec */

	x = cos(true_lunar_longitude);
	y = sin(true_lunar_longitude);
	z = 0.0;

	xx = x;
	yy = y * cos(mean_lunar_inclination_to_ecliptic);
	zz = y * sin(mean_lunar_inclination_to_ecliptic);

	ra  = atan2(yy, xx) + RAD(Omega);
	dec = atan2(zz, sqrt(xx * xx + yy * yy));


	x = cos(ra) * cos(dec);
	y = sin(ra) * cos(dec);
	z = sin(dec);

	xx = x;
	yy = y * cos(obliquity_ecliptic) - z * sin(obliquity_ecliptic);
	zz = z * cos(obliquity_ecliptic) + y * sin(obliquity_ecliptic);


	/* parallax correction */
	z = zz - sin(RAD(lat)) / lunar_semimajor_axis_in_earth_radii;

	x = xx - cos(RAD(lat)) * cos(hour_angle)
		/ lunar_semimajor_axis_in_earth_radii;

	y = yy - cos(RAD(lat)) * sin(hour_angle)
		/ lunar_semimajor_axis_in_earth_radii;

	/* ??? */
	moon.ra  = DEG_TO_HOUR(DEG(atan2(y, x)));
	moon.dec = - DEG(atan2(z, sqrt(x * x + y * y)));

	return moon;
}
#else
/**
 * @note for now, use this one
 */

struct coord_equatorial moon_ra_dec(double lat, double lon,
				    double hour_angle_shift)
{
	double d;
	double Lm;
	double Gamma;
	double Omega;
	double D;
	double M;

	double true_anomaly;
	double evection;
	double variation;
	double true_lunar_longitude;

	double hour_angle;

	double ra, dec;
	double x, y, z;
	double xx, yy, zz;

	const double obliquity_ecliptic = RAD(23.45);
	const double mean_lunar_inclination_to_ecliptic = RAD(5.1453964);
	const double lunar_eccentricity = 0.054900489;
	const double lunar_semimajor_axis_in_earth_radii = 60.2665;


	struct coord_equatorial moon;


	hour_angle = local_sidereal_time(lon);

	/* fractional day number since 1999 Jan 0, 0h TT (terrestrial time) */
	d = daynumber() + (365.2425 + 0.5) * 2.0 + 0.015 + hour_angle_shift / 24.0;

	/* mean longitude of the Moon, measured in the the ecliptic to the mean
	 * ascending node and then along the orbit
	 */
	Lm =  69.167124 + 13.17639648 * d;

	/* mean longitude of the lunar perigee, measured as for L */
	Gamma = 42.524057 + 0.11140353 * d;

	/* mean longitude of the mean ascending node of the lunar orbit
	 * on the ecliptic
	 */
	Omega = 144.452077 - 0.05295377 * d;

	/* mean elongation of the Moon from the Sun */
	D = 149.940812 + 12.19074912 * d;

	/* mean lunar anomaly */
	M = Lm - Gamma;

	true_anomaly = 2.0 * lunar_eccentricity * sin(RAD(M));

	evection = 1.274 / 57.3 * sin(RAD(2.0 * D - M));

	variation = (0.658 / 57.3) * sin(RAD(2.0 * D));

	true_lunar_longitude = RAD(Lm - Omega) + true_anomaly + evection
			       + variation;


	/* transform to topographical ra/dec */

	x = cos(true_lunar_longitude);
	y = sin(true_lunar_longitude);
	z = 0.0;

	xx = x;
	yy = y * cos(mean_lunar_inclination_to_ecliptic);
	zz = y * sin(mean_lunar_inclination_to_ecliptic);

	ra  = atan2(yy, xx) + RAD(Omega);
	dec = atan2(zz, sqrt(xx * xx + yy * yy));


	x = cos(ra) * cos(dec);
	y = sin(ra) * cos(dec);
	z = sin(dec);

	xx = x;
	yy = y * cos(obliquity_ecliptic) - z * sin(obliquity_ecliptic);
	zz = z * cos(obliquity_ecliptic) + y * sin(obliquity_ecliptic);


	/* parallax correction */
	z = zz - sin(RAD(lat)) / lunar_semimajor_axis_in_earth_radii;

	x = xx - cos(RAD(lat)) * cos(hour_angle)
		/ lunar_semimajor_axis_in_earth_radii;

	y = yy - cos(RAD(lat)) * sin(hour_angle)
		/ lunar_semimajor_axis_in_earth_radii;

	moon.ra  = DEG_TO_HOUR(DEG(atan2(y, x)));
	moon.dec = DEG(atan2(z, sqrt(x * x + y * y)));

	return moon;
}
#endif

/**
 * @brief the current equatorial coordinates of the Sun
 *
 * @return a struct coord_equatorial (values in hours and degrees)
 *
 * @see  Astronomical Almanac page C3-C5 Sun, 2014
 */

struct coord_equatorial sun_ra_dec(double hour_angle_shift)
{
	double d;
	double g;
	double L;
	double ecl_lon;
	double ecl_obl;

	struct coord_equatorial sun;


	/* days since Jan 0th, 0h UT */
	d = daynumber() + hour_angle_shift / 24.0;

	/* mean longitude of the sun, corrected for aberation */
	L = 279.583 + 0.985647 * d;

	/* mean anomaly */
	g = RAD((357.528 + 0.9856003 * d));

	/* eciptic longitude */
	ecl_lon = RAD((L + 1.915 * sin(g) + 0.020 * sin(2.0 * g)));

	/* eciptic obliquity */
	ecl_obl = RAD((23.439 - 0.0000004 * d));

	/*
	 * breaks after 21 June
	 * sun.ra      = atan(cos(ecl_obl) * tan(ecl_lon)) * 180 / M_PI;
	 */

	sun.ra  = atan2(sin(ecl_lon) * cos(ecl_obl), cos(ecl_lon));
	sun.dec = asin(sin(ecl_obl) * sin(ecl_lon));

	sun.ra  = DEG(sun.ra);
	sun.dec = DEG(sun.dec);

	if (sun.ra < 0.0)
		sun.ra += 360.0;

	sun.ra = DEG_TO_HOUR(sun.ra);


	return sun;
}


/**
 * @brief get the correction for the velocity of the local standard of rest
 *
 * @param eq a struct coord_equatorial (values in hours and degrees)
 * @param d days since Jan 0th, if 0, current time is used
 *
 * @return the correction for the vlsr (in km/s)
 *
 * @note negative value == approaching
 *
 * @note to include topocentric velocity: add (40000/86163) * cos(lat)
 */

double vlsr(struct coord_equatorial eq, double d)
{
	double x, y, z;
	double x0, y0, z0;

	double v_sun;

	double src_lat;
	double src_lon;

	double sun_lon;

	/* movement of the Sun: 20 km/s towards ra = 18h dec = 30.0 deg */
	const double x_sun = 20.0 * cos(18.0 * M_PI / 12.0) * cos(RAD(30.0));
	const double y_sun = 20.0 * sin(18.0 * M_PI / 12.0) * cos(RAD(30.0));
	const double z_sun = 20.0 * sin(RAD(30.0));

	const double obliquity_ecliptic = RAD(23.439);


	eq.ra  = HOUR_TO_DEG(eq.ra);
	eq.ra  = RAD(eq.ra);
	eq.dec = RAD(eq.dec);

	v_sun = - x_sun * cos(eq.ra) * cos(eq.dec)
		- y_sun * sin(eq.ra) * cos(eq.dec)
		- z_sun * sin(eq.dec);


	x0 = cos(eq.ra) * cos(eq.dec);
	y0 = sin(eq.ra) * cos(eq.dec);
	z0 = sin(eq.dec);

	x = x0;
	y = y0 * cos(obliquity_ecliptic) + z0 * sin(obliquity_ecliptic);
	z = z0 * cos(obliquity_ecliptic) - y0 * sin(obliquity_ecliptic);

	src_lat = atan2(z, sqrt(x * x + y * y));
	src_lon = atan2(y, x);

	if(d == 0.0)
		d = daynumber();

	/* mean longitude of the sun, corrected for aberation */
	sun_lon = RAD(279.583 + 0.985647 * d);

	return (v_sun - 30.0 * cos(src_lat) * sin(sun_lon - src_lon));
}


/**
 * @brief convert a relative velocity to a Doppler frequency
 *
 * @param vel the velocity (in km/s)
 * @param freq_ref a reference frequency
 *
 * @return the Doppler frequency, same power of 10 as freq_ref
 */

double doppler_freq_relative(double vel, double freq_ref)
{
	return vel * freq_ref / 299790.0;
}


/**
 * @brief convert a velocity to a Doppler frequency
 *
 * @param vel the velocity (in km/s)
 * @param freq_ref a reference frequency
 *
 * @return the Doppler frequency, same power of 10 as freq_ref
 */

double doppler_freq(double vel, double freq_ref)
{
	return  freq_ref * (1.0 - vel / 299790.0);
}

/**
 * @brief convert a frequency to a Doppler velocity
 *
 * @param freq the frequency (should be same power of 10 as reference)
 * @param freq_ref a reference frequency
 *
 * @return the Doppler velocity, same power of 10 as frequency input
 */

double doppler_vel(double freq, double freq_ref)
{
	return (freq / freq_ref - 1.0) * 299790.0;
}


/**
 * @brief convert a relative frequency to a Doppler velocity
 *
 * @param freq the frequency (should be same power of 10 as reference)
 * @param freq_ref a reference frequency
 *
 * @return the Doppler velocity, same power of 10 as frequency input
 */

double doppler_vel_relative(double freq, double freq_ref)
{
	return (freq / freq_ref) * 299790.0;
}
