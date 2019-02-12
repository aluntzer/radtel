/**
 * @file    levmar.h
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

#ifndef _LEVMAR_H_
#define _LEVMAR_H_


struct lm_ctrl {

	size_t max_iter;	/* iteration limit */
	double lambda;		/* dampening factor */
	double lambda_inc;	/* lambda increment adjust factor */
	double lambda_dec;	/* lambda decrement adjust factor */

	double *par;		/* the paramter array */
	size_t n_par;		/* the number of parameters */

	double (*fit) (double *par,  double x);
	void   (*grad)(double *grad, double *par, double arg);


	size_t final_it;	/* final number of iterations */
	double target_derr;
	double final_err;
	double final_delta_err;


};

void lm_init(struct lm_ctrl *ctrl);
void lm_set_fit_param(struct lm_ctrl *ctrl,
		      double (*fit) (double *, double),
		      void   (*grad)(double *, double *, double arg),
		      double *par, size_t n_par);
int lm_min(struct lm_ctrl *ctrl, const double *x, const double *y,
	   const double *w, size_t n);

#endif /* _LEVMAR_H_ */
