/**
 * @file    levmar.c
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
 * @brief Provides an implementation of the Levenberg-Marquardt algorithm
 *	  for data fitting.
 *
 * @see http://users.ics.forth.gr/~lourakis/publ/2005_levmar.pdf for a
 *	description of the algorithm
 *
 * @note this is not an ideal implementation, it just works for my purpose
 *	 if you want to do some serious analysis, look elsewhere
 */


#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>


#include <stdio.h>
#include "levmar.h"

/* this is pretty permissive */
#define LM_TOL (1e6 * DBL_EPSILON)

#define LM_DEFAULT_MAX_ITER	1000
#define LM_DEFAULT_LAMBDA	1e-6
#define LM_DEFAULT_LAMBDA_INC	10.
#define LM_DEFAULT_LAMBDA_DEC	(1.0 / LM_DEFAULT_LAMBDA_INC)



/**
 * @bief square and inverse the weights
 *
 * @param ctrl	a struct lm_ctrl
 * @param n	the number of elements in the weights arrays
 *
 * @returns an allocated array of inverse squared weights
 */


static double *lm_isqw(struct lm_ctrl *ctrl, const double *w, size_t n)
{
	int i;
	double *isqw;

	isqw = (double *) malloc(n * sizeof(double));

	for (i = 0; i < n; i++)
		isqw[i] = 1.0 / (w[i] * w[i]);

	return isqw;
}


/**
 * @brief compute chi-squared for the dataset
 *
 * @param ctrl	a struct lm_ctrl
 * @param par	an array of parameters
 * @param x	an array of x-axis values (function arguments)
 * @param y	an array of y-axis values (function values)
 * @param isqw	an array of inversed squared weights for the y axis values
 *		(may be NULL)
 * @param n	the number of elements in each of the x, y, isqw arrays
 *
 * @returns the chi-squared error
 */

static double lm_chisq(struct lm_ctrl *ctrl, double *par,
		       const double *x, const double *y, double *isqw, size_t n_elem)
{
	int i;

	double dsq;
	double err = 0.0;


	for (i = 0; i < n_elem; i++) {

		dsq = ctrl->fit(par, x[i]) - y[i];
		dsq *= dsq;

		if (isqw) {
			printf("xxx0x0x0x0x0\n");
			dsq *= isqw[i];
		}

		err += dsq;
	}

	return err;
}


/**
 * @brief compute the numerical gradient
 *
 * @param ctrl	 a struct lm_ctrl
 * @param g[out] a gradient array
 * @param x	 an x-axis value (function value)
 */

static void lm_numeric_param_gradient(struct lm_ctrl *ctrl, double *g, double x)
{
	size_t i;

	double tmp;
	double step;
	double val;

	const double eps = LM_TOL;


	for (i = 0; i < ctrl->n_par; i++) {

		tmp  = ctrl->par[i];
		step = eps * fabs(tmp);

		if (step < DBL_EPSILON)
			step = eps;

		val = ctrl->fit(ctrl->par, x);

		/* modify "par" in-place and restore; this is more efficient
		 * than making a copy
		 */
		ctrl->par[i] = tmp + step;
		g[i] = (ctrl->fit(ctrl->par, x) - val) / step;
		ctrl->par[i] = tmp;
	}
}


/**
 * @brief calculate the Jacobian and the Hessian
 *
 * @param ctrl	a struct lm_ctrl
 * @param J	the Jacobian determinant (same size as parameter array)
 * @param H	the (approximate) Hessian (same size as parameter array squared)
 *
 * @param x	an array of x-axis values (function arguments)
 * @param y	an array of y-axis values (function values)
 * @param isqw	an array of inversed squared weights for the y axis values
 *		(may be NULL)
 * @param n	the number of elements in each of the x, y, isqw arrays
 */

static void lm_H_J(struct lm_ctrl *ctrl, double *J, double *H,
		   const double *x, const double *y, double *isqw, double n)
{
	size_t i, j, k;

	double d;
	double w = 1.0; /* if unweighted */

	double *g;


	g = (double *) malloc(ctrl->n_par * sizeof(double));
	if (!g)
		return;

	bzero(J, ctrl->n_par * sizeof(double));
	bzero(H, ctrl->n_par * ctrl->n_par * sizeof(double));

	for (i = 0; i < n; i++) {

		if (isqw) /* if weighted, update */
			w = isqw[i];

		if (ctrl->grad)
			ctrl->grad(g, ctrl->par, x[i]);
		else
			lm_numeric_param_gradient(ctrl, g, x[i]);

		for (j = 0; j < ctrl->n_par; j++) {

			d = y[i] - ctrl->fit(ctrl->par, x[i]);

			J[j] += d * g[j] * w;

			for (k = 0; k <= j; k++)
				H[j * ctrl->n_par + k] += g[j] * g[k] * w;
		}
	}

	free(g);
}


/**
 * @brief perform a Cholesky decomposition
 *
 * @param L a lower triangular matrix (real entries only, so A = L L^T)
 * @param A a Hermitian positive-definite matrix
 * @param n the dimensions of the system (note: m==n)
 *
 * @returns 0 on succes, <0 if A is not positive-definite
 */

static int cholesky_decomp(double *L, double *A, size_t n)
{
	size_t i,j,k;

	double sum;


	for (i = 0; i < n; i++) {
		for (j = 0; j < i; j++) {

			sum = 0;

			for (k = 0; k < j; k++)
				sum += L[i * n + k] * L[j * n + k];

			L[i * n + j] = (A[i * n + j] - sum) / L[j * n + j];
		}

		sum = 0;

		for (j = 0; j < i; j++)
			sum += L[i * n + j] * L[i * n + j];

		sum = A[i * n +i] - sum;

		if (sum < LM_TOL)
			return -1; /* not positive-definite */

		L[i * n + i] = sqrt(sum);
	}


	return 0;
}


/**
 * @brief solve the linear least squares problem Ax = b using Cholesky
 *	  decomposition of A = L L^T
 *
 * @param L a lower triangular matrix of coefficients
 * @param x a solution vector
 * @param b the right side of the system
 * @param n the dimensions of the system (note: m==n)
 */

static void solve_axb_cholesky(double *L, double *x, double *b, ssize_t n)
{
	ssize_t i, j;

	double sum;


	for (i = 0; i < n; i++) {

		sum = 0.0;

		for (j = 0; j < i; j++)
			sum += L[i * n + j] * x[j];

		x[i] = (b[i] - sum) / L[i * n + i];
	}

	for (i = (n - 1); i >= 0; i--) {

		sum = 0.0;

		for (j = i + 1; j < n; j++)
			sum += L[j * n + i] * x[j];

		x[i] = (x[i] - sum) / L[i * n + i];
	}
}



/**
 * @brief solve the system N delta = J^T and update parameters
 */

static void lm_update_param(struct lm_ctrl *ctrl, double *J, double *H,
			    const double *x, const double *y, double *isqw,
			    double n)
{

	size_t i;

	double err;

	double *delta;
	double *L;
	double *newpar;

	double newerr;

	newpar = (double *) malloc(ctrl->n_par * sizeof(double));
	delta = (double *) malloc(ctrl->n_par * sizeof(double));
	L = (double *) malloc(ctrl->n_par * ctrl->n_par * sizeof(double));


	/* See if we converge. If we do, solve N delta = J^T for
	 * delta and update the solution, otherwise, adjust the
	 * dampening factor for a steeper descent into the gradient
	 */

	while (1) {

		err = ctrl->final_err;

		if (ctrl->final_it >= ctrl->max_iter)
			break;


		for (i = 0; i < ctrl->n_par; i++)
			H[i * ctrl->n_par + i] *= (1.0 + ctrl->lambda);

		if (!cholesky_decomp(L, H, ctrl->n_par)) {

			solve_axb_cholesky(L, delta, J, ctrl->n_par);

			for (i = 0; i < ctrl->n_par; i++)
				newpar[i] = ctrl->par[i] + delta[i];

			newerr = lm_chisq(ctrl, newpar, x, y, isqw, n);

			ctrl->final_delta_err = newerr - err;

			if (ctrl->final_delta_err <= 0.0)
				break;
		}

		/* if cholesky_decomp() failed or final delta is invalid,
		 * adjust lambda so we take a more aggressive step in the
		 * gradient direction and try again.
		 */

		ctrl->lambda *= ctrl->lambda_inc;

		ctrl->final_it++;
	}


	/* We are closer to the minimum, reduce the dampening factor
	 * for better convergence. Note that this is not a very sophisticated
	 * strategy, but it works sufficiently well. A more advanced approach
	 * would be to adjust lambda based on the local curvature of the
	 * function.
	 */

	ctrl->lambda *= ctrl->lambda_dec;

	ctrl->final_err = newerr;

	for (i = 0; i < ctrl->n_par; i++)
		ctrl->par[i] = newpar[i];

	free(delta);
	free(L);
	free(newpar);
}


/**
 * @brief perform a Levenberg-Marquardt least-squares minimization
 *
 * @param par	an array of function parameters
 * @param npar	the number of function parameters
 * @param x	an array of x-axis values (function arguments) XXX <- could be NULL
 * @param y	an array of y-axis values (function values)
 * @param w	an array of weights for the y axis values (may be NULL)
 * @param n	the number of elements in each of the x, y, w arrays
 *
 */

int lm_min(struct lm_ctrl *ctrl, const double *x, const double *y,
	   const double *w, size_t n)
{

	double *J;
	double *H;

	double *isqw = NULL;


	/** XXX sanity check control structure here */

	J = (double *) malloc(ctrl->n_par * sizeof(double));
	H = (double *) malloc(ctrl->n_par * ctrl->n_par * sizeof(double));

	if (!J)
		return -1;

	if (!H) {
		free(J);
		return -1;
	}

	/* if weights were supplied, square and inverse them */
	if (w)
		isqw = lm_isqw(ctrl, w, n);

	/* calculate initial error */
	ctrl->final_err = lm_chisq(ctrl, ctrl->par, x, y, isqw, n);


	for (ctrl->final_it = 0; ctrl->final_it < ctrl->max_iter; ctrl->final_it++) {

		lm_H_J(ctrl, J, H, x, y, isqw, n);

		lm_update_param(ctrl, J, H, x, y, isqw, n);

		if (-ctrl->final_delta_err < ctrl->target_derr)
			break;

	}

	free(isqw);
	free(J);
	free(H);

	/* if this is the case, we most likely did not converge */
	return (ctrl->final_it < ctrl->max_iter);
}


/**
 * @brief set the fit function parameters
 *
 * @param fit	the function to be fit
 * @param grad	a function to compute the gradient of the input parameters
 *		(optional, set NULL for numeric gradient)
 *
 * @param par	an array of initial function parameters
 * @param npar	the number of function parameters
 */

void lm_set_fit_param(struct lm_ctrl *ctrl,
		      double (*fit) (double *, double),
		      void   (*grad)(double *, double *, double arg),
		      double *par, size_t n_par)
{
	ctrl->par   = par;
	ctrl->n_par = n_par;
	ctrl->fit   = fit;
	ctrl->grad  = grad;
}


/**
 * @brief initialise control structure and set default parameters
 */

void lm_init(struct lm_ctrl *ctrl)
{
	bzero(ctrl, sizeof(struct lm_ctrl));


	ctrl->max_iter	 = LM_DEFAULT_MAX_ITER;
	ctrl->lambda     = LM_DEFAULT_LAMBDA;
	ctrl->lambda_inc = LM_DEFAULT_LAMBDA_INC;
	ctrl->lambda_dec = LM_DEFAULT_LAMBDA_DEC;

	/* xxx */
	ctrl->target_derr = 1e-12;
}
