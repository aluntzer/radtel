/**
 * @file    fitfunc.c
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
 * @brief provides fit utility functions
 *
 */


#include <stdio.h>
#include <stddef.h>

#include <math.h>

#include <levmar.h>



/**
 * @brief a 4-parameter gaussian
 *
 * @param p the parameter array (4 elements)
 *
 * @param x the function argument
 *
 * @returns the function value
 */

double gaussian(double p[4], double x)
{
	return  p[3] + p[0] * exp( - (x - p[2]) * (x - p[2]) /
				     (2.0 * p[1] * p[1]));
}


/**
 * @brief get the FWHM of the gaussian)
 *
 * @param p the parameter array (4 elements, see gaussian())
 *
 * @returns the FWHM
 */

double gaussian_fwhm(double p[4])
{
	return 2.0 * sqrt(2.0 * log(2.0)) * p[1];
}


/**
 * @brief get the peak shift of the gaussian)
 *
 * @param p the parameter array (4 elements, see gaussian())
 *
 * @returns the peak shift from 0
 */

double gaussian_peak(double p[4])
{
	return p[2];
}


/**
 * @brief get the height of the gaussian
 *
 * @param p the parameter array (4 elements, see gaussian())
 *
 * @returns the height (multiples of unity)
 */

double gaussian_height(double p[4])
{
	return p[0];
}


/**
 * @brief get the offset of the gaussian
 *
 * @param p the parameter array (4 elements, see gaussian())
 *
 * @returns the offset (vertical shift from 0)
 */

double gaussian_offset(double p[4])
{
	return p[3];
}


/**
 * @brief helper function to guess initial parameters for the gaussian
 *
 * @param par the parameter array (4 elements, see gaussian())
 *
 * @param x the array of x values
 * @param y the array of y values
 * @param n the number of data elements
 */

void gaussian_guess_param(double par[4],
			  const double *x, const double *y,
			  size_t n)
{
	size_t i;

	double tmp;

	double sig = 0.0;
	double mean = 0.0;

	double xmin = __DBL_MAX__;
	double xmax = __DBL_MIN__;
	double ymin = __DBL_MAX__;
	double ymax = __DBL_MIN__;


	if (!n)
		return;

	for (i = 0; i < n; i++) {

		if (x[i] > xmax)
			xmax = x[i];

		if (x[i] < xmin)
			xmin = x[i];

		if (y[i] > ymax)
			ymax = y[i];

		if (y[i] < ymin)
			ymin = y[i];

		mean += x[i];
	}

	mean /= (double) n;

	for (i = 0; i < n; i++) {
		tmp = x[i] - mean;
		sig += tmp * tmp;
	}

	sig /= (double) n;

	sig = sqrt(sig);

	par[0] = (ymax - ymin);		/* amplitude */
	par[1] = sig;			/* sigma */
	par[2] = mean;			/* center shift */
	par[3] = ymin;			/* baseline shift */
}


/**
 * @brief function to fit a guassian
 *
 * @param x the array of x values
 * @param y the array of y values
 * @param n the number of data elements
 *
 * @returns 0 on success, otherwise error
 *
 * @note the number of data must be at least the number of parameters
 */

int gaussian_fit(double par[4],
		 const double *x, const double *y,
		 size_t n)
{
	struct lm_ctrl lm;


	if (n < 4)
		return -1;

	lm_init(&lm);
	lm_set_fit_param(&lm, &gaussian, NULL, par, 4);


	lm_min(&lm, x, y, NULL, n);

#if DEBUG
	printf("GAUSSIAN: Peak: %10g, Height: %10g, FWHM: %10g, Shift %10g\n",
	       par[2], par[0], fabs(par[1]) * 2.0 * sqrt(log(2.0)), par[3]);
#endif

	return 0;
}



