/**
 * @file    include/payload/cmd_spec_data.h
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
 * @brief payload structure for CMD_SPEC_DATA
 *
 */

#ifndef _INCLUDE_PAYLOAD_CMD_SPEC_DATA_H_
#define _INCLUDE_PAYLOAD_CMD_SPEC_DATA_H_


struct spec_data {

	uint64_t freq_min_hz;		/* lower frequency limit */
	uint64_t freq_max_hz;		/* upper frequency limit */
	uint64_t freq_inc_hz;		/* frequency increment   */

	/* NOTE: spectral data unit is milli-Kelvins ! */
	uint32_t n;			/* number of data points */ 
	uint32_t spec[];		/* the actual spectral data */
};


#endif /* _INCLUDE_PAYLOAD_CMD_SPEC_DATA_H_ */
