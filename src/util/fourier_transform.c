/**
 * @file    fourier_transform.c
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
 * @brief provides an implementation of a FFT (Cooley-Tuckey-ish)
 *
 * @note this is not an ideal implementation, it just works for my purpose
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <complex.h>


/**
 * @brief essentially 2^(log2(n -1) + 1)
 */

static size_t get_next_pow_2_bound(size_t n)
{
	size_t c = 0;

	n = n - 1;
	while (n >>= 1)
		c++;

	return (1 << (c + 1));
}


static void fft_internal(double complex *tmp, double complex *out,
			 const double complex *c, size_t n, int stp)
{
	size_t i;

	double complex t;


	if (stp >= n)
		return;

	fft_internal(out,        tmp,      c, n, 2 * stp);
	fft_internal(out + stp, tmp + stp, c, n, 2 * stp);

	for (i = 0; i < n; i += 2 * stp) {

		t = c[i / 2] * out[stp + i];

		tmp[i / 2]     = out[i] + t ;
		tmp[(i + n)/2] = out[i] - t;
	}
}


/**
 * @brief prepare fft coefficients
 *
 * @param n the length of the desired transform
 * @param fwd direction of the transform (1: fft, 0: ifft)
 *
 * @note if n is not a power of 2, it will be adjusted upwards accordingly
 *
 * @returns the array of n/2 coefficients, or NULL on error
 */

double complex *fft_prepare_coeff(size_t n, int inv)
{
	size_t i;

	double sig;

	double complex *c;


	n  = get_next_pow_2_bound(n);

	c = malloc(n / 2 * sizeof(double complex));
	if (!c)
		return NULL;

	if (inv)
		sig =  1.0;
	else
		sig = -1.0;

	for (i = 0; i < n / 2; i++)
		c[i] = cexp(sig * I * 2.0 * M_PI * (double) i / (double) n);


	return c;
}


/**
 * @brief convenience function to retrieve a copy of the relevant
 *	  data section for non-power-of-two fft results
 *
 * @param data the fft data
 * @param fftsize the power-of-two size of the fft
 *
 * @param len the lenght of the data to be extracted
 *
 * @returns a pointer to the extracted data or NULL on error
 *
 */

double complex *fft_extract(const double complex *data, size_t fftsize,
			    size_t len)
{
	double complex *buf;


	if (fftsize < len)
		return NULL;

	if (!data)
		return NULL;

       	buf = malloc(len * sizeof(double complex));
	if (!buf)
		return NULL;


	memcpy(buf, data, len * sizeof(double complex));

	return buf;
}


/**
 * @brief perform an in-place fft on a buffer
 *
 * @param data the data buffer
 * @param coeff precomputed coefficients (may be NULL)
 * @param n the length of the buffer (must be a power of two)
 * @param fwd direction of the fft (1: fft, 0: ifft)
 *
 * @note if coeff is NULL, the coefficients will be computed for you
 *
 * @returns 0 on succes, -1 on error
 */

int fft2(double complex *data, double complex *coeff, size_t n, int inv)
{
	size_t i;

	double complex *c;
	double complex *tmp;


	if (!n)
		return 0;

	/* require powers of two */
	if ((n & (n - 1)))
	       return -1;

	if (!data)
		return -1;

	/* create a work copy */
	tmp = malloc(n * sizeof(double complex));
	if (!tmp)
		return -1;

	memcpy(tmp, data, n * sizeof(double complex));

	if (!coeff)
		c = fft_prepare_coeff(n, inv);
	else
		c = coeff;

	if (!c) {
		free(tmp);
		return -1;
	}


	fft_internal(data, tmp, c, n, 1);

	/* normalise inverse transform*/
	if (inv) {
		for (i = 0; i < n; i++)
			data[i] /= (double) n;
	}

	free(tmp);

	if (!coeff)
		free(c);

	return 0;
}



/**
 * @brief perform an fft on a buffer of arbitrary size
 *
 * @param data the data buffer
 * @param coeff precomputed coefficients (may be NULL)
 * @param n the length of the buffer
 * @param fwd direction of the fft (1: fft, 0: ifft)
 * @param[out] fftsize the size of the actual fft
 *
 * @note if coeff is NULL, the coefficients will be computed for you
 *
 * @returns a pointer to the result of the FFT or NULL on error
 *	    the length of the result is returned in fftsize
 */

double complex *fft(const double complex *data, double complex *coeff,
		    size_t len, size_t *fftsize, int inv)
{
	size_t i;

	size_t n;

	double complex *c;
	double complex *pad;


	(*fftsize) = 0;

	if (!len)
		return NULL;

	if (!data)
		return NULL;

	n  = get_next_pow_2_bound(len);



	/* create a padded copy of the data */
	pad = malloc(n * sizeof(double complex));
	if (!pad)
		return NULL;

	memcpy(pad, data, len * sizeof(double complex));
	bzero(&pad[len],  (n - len) * sizeof(double complex));

	if (fft2(pad, coeff, n, inv) < 0) {
		free(pad);
		return NULL;
	}


	(*fftsize) = n;

	return pad;
}


/**
 * @brief slow reference DFT
 */

void dft(double complex *in, double complex *out, int N)
{
	int i, j;

	double  a;

	double complex s;


	for (i = 0; i < N; i++) {

		s = 0.0;

		for (j = 0; j < N; j++) {

			a = - 2.0 * M_PI * i * j / N;

			s += in[j] * (cos(a) + I * sin(a));

		}

		out[i] = s;
	}
}


/**
 * @brief slow reference DFT
 */

void idft(double complex *in, double complex *out, int N)
{
	int i, j;

	double a;

	double complex s;


	for (i = 0; i < N; i++) {

		s = 0.0;

		for (j = 0; j < N; j++) {

			a =  2.0 * M_PI * i * j / N;

			s += in[j] * (cos(a) + I * sin(a));
		}

		out[i] = s / N;
	}
}
