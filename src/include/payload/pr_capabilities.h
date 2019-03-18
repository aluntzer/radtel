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

struct local_horizon {
	int32_t az;
	int32_t el;
};


struct capabilities {

	/* given that the earch has a circumference of about 40075161.2 meters
	 * at the equator, latitude and longitude in arcseconds should be
	 * plenty precise for our application, giving at least ~31 meters of
	 * position accuracy
	 */

	int32_t lat_arcsec;		/* station latitude  */
	int32_t lon_arcsec;		/* station longitude */

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
	 * and maximum filter bandwidth divider:
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

	/* local horizon profile defined in pairs of
	 * azimuth and elevation.
	 * NOTE: coordinate units are in degrees. If absolutely, required, this
	 * could be changed to arcminutes or arcseconds, as the data type is
	 * certainly wide enought, but I don't see the the necessity at this
	 * time.
	 */

	uint32_t n_hor;			/* number of az/el coordinate pairs */
	struct local_horizon hor[];		/* azimuth/elevation coordinate pairs */

}__attribute__((packed));


#endif /* _INCLUDE_PAYLOAD_PR_CAPABILITIES_H_ */
