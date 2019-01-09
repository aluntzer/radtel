/**
 * @file    include/payload/pr_capabilities.h
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
 * @brief payload structure for PR_CAPABILITIES
 *
 */

#ifndef _INCLUDE_PAYLOAD_PR_CAPABILITIES_H_
#define _INCLUDE_PAYLOAD_PR_CAPABILITIES_H_


/*
 * Digital SRT radiometer values:
 *
 * freq_min_hz		 1370 000 000	(1370 MHz)
 * freq_max_hz		 1800 000 000  (1800 MHz)
 * freq_inc_hz		       40 000  (  40 kHź)
 *
 * bw_max_hz		      500 000	(500 kHz)
 *
 * bw_max_div_lin;	             0  (unused)
 * bw_max_div_rad2		     2  (0 = 500 kHz, 1 = 250 kHz, 2 = 125 kHz)
 *
 * bw_max_bins			    64
 *
 * bw_max_bin_div_lin		     0 (unused)
 * bw_max_bin_div_rad2		     0 (always 64)
 *
 * PR_CAPABILITIES packet payload structure
 *
 */

struct capabilities {

	int32_t az_min_arcsec;		/* left limit      */				
	int32_t az_max_arcsec;		/* right limit     */
	int32_t az_res_arcsec;		/* step resolution */

	int32_t el_min_arcsec;		/* lower limit     */
	int32_t el_max_arcsec;		/* upper limit     */
	int32_t el_res_arcsec;		/* step resolution */

	/* frequency limits */
	uint64_t freq_min_hz;		/* lower frequency limit */
	uint64_t freq_max_hz;		/* upper frequency limit */
	uint64_t freq_inc_hz;		/* frequency increment   */


	/* filter bandwidth limits
	 * and maxium filter bandwidth divider:
	 *	bw_max_hz/bw_max_div = bw_min_hz
	 *
	 * the filter bandwith divider may be given as linear integer limit or
	 * radix-2 exponent
	 *
	 * NOTE: a value of 0 for the LINEAR divider marks it as unused
	 */

	uint32_t bw_max_hz;		/* max resolution bandwidth */
	uint32_t bw_max_div_lin;	/* max BW divider (linear)*/
	uint32_t bw_max_div_rad2;	/* max BW divider (2^n) */


	/* number of bins per bandwidth, e.g. length of FFT
	 * and maximum divider for bins per bandwidth:
	 *	max_bins/max_div = min_bins
	 *
	 * the bin divider may be given as linear integer limit or
	 * radix-2 exponent
	 *
	 * NOTE: a value of 0 for the LINEAR divider marks it as unused
	 */

	uint32_t bw_max_bins;		/* upper number of bins per bandwidth */
	uint32_t bw_max_bin_div_lin;	/* max bin per BW divider (linear) */
	uint32_t bw_max_bin_div_rad2;	/* max bin per BW divider (2^n) */

	uint32_t n_stack_max;		/* max spec averaging */
};


#endif /* _INCLUDE_PAYLOAD_PR_CAPABILITIES_H_ */
