/**
 * @file    fitfunc.h
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
 */

#ifndef _FITFUNC_H_
#define _FITFUNC_H_

double gaussian(double p[4], double x);

void gaussian_guess_param(double par[4],
			  const double *x, const double *y,
			  size_t n);

int gaussian_fit(double par[4],
		 const double *x, const double *y,
		 size_t n);


#endif /* _FITFUNC_H_ */
