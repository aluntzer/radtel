/**
 * @file    include/cmd.h
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

#ifndef _INCLUDE_CMD_H_
#define _INCLUDE_CMD_H_

#include <protocol.h>
#include <net_common.h>

void cmd_invalid_pkt(void);
void cmd_capabilities(void);
void cmd_success(void);
void cmd_fail(void);
void cmd_moveto_azel(double az, double el);
void cmd_recalibrate_pointing(void);
void cmd_park_telescope(void);
void cmd_spec_acq_start(uint64_t f0, uint64_t f1, uint32_t bw_div,
			uint32_t bin_div, uint32_t n_stack, uint32_t acq_max);
void cmd_spec_data(struct spec_data *s);
void cmd_getpos_azel(void);

#endif /* _INCLUDE_CMD_H_ */

