/**
 * @file    fourier_transform.h
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

#ifndef _FOURIER_TRANSFORM_H_
#define _FOURIER_TRANSFORM_H_

#include <complex.h>

#define FFT_FORWARD 0
#define FFT_INVERSE 1



int fft2(double complex *data, double complex *coeff, size_t n, int inv);

double complex *fft(const double complex *data, double complex *coeff,
		    size_t len, size_t *fftsize, int inv);

double complex *fft_extract(const double complex *data, size_t fftsize,
			    size_t len);

double complex *fft_prepare_coeff(size_t n, int inv);


void dft(double complex *in, double complex *out, int N);
void idft(double complex *in, double complex *out, int N);


#endif /* _FOURIER_TRANSFORM_H_ */
