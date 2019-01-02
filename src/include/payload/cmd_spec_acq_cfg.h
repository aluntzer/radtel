/**
 * @file    include/payload/cmd_spec_acq_cfg.h
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
 * @brief payload structure for CMD_SPEC_ACQ_CFG
 *
 */

#ifndef _INCLUDE_PAYLOAD_CMD_SPEC_ACQ_CFG_H_
#define _INCLUDE_PAYLOAD_CMD_SPEC_ACQ_CFG_H_

/**
 * CMD_SPEC_ACQ_cfg packet payload structure
 */

struct spec_acq_cfg {

	uint64_t freq_start_hz;		/* start frequency */
	uint64_t freq_stop_hz;		/* stop frequency */
	
	uint32_t bw_div;		/* bandwith divider */
	uint32_t bin_div;		/* bins per bandwidth divider */

	uint32_t n_stack;		/* spectrae to integrate, 0 or 1 are
					 * considered as no stacking
					 */
	uint32_t acq_max;		/* number of stacked spectrae to return,
					 * 0 == infinite
					 */
};

#endif /* _INCLUDE_PAYLOAD_CMD_SPEC_ACQ_CFG_H_ */
