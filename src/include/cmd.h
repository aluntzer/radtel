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
 * @brief all functions with the cmd_ prefix are client -> server only
 *
 */

#ifndef _INCLUDE_CMD_H_
#define _INCLUDE_CMD_H_

#include <protocol.h>
#include <net_common.h>


/* command packet generation functions */

struct packet *cmd_invalid_pkt_gen(uint16_t trans_id);
struct packet *cmd_capabilities_gen(uint16_t trans_id);
struct packet *cmd_success_gen(uint16_t trans_id);
struct packet *cmd_fail_gen(uint16_t trans_id);
struct packet *cmd_moveto_azel_gen(uint16_t trans_id, double az, double el);
struct packet *cmd_recalibrate_pointing_gen(uint16_t trans_id);
struct packet *cmd_park_telescope_gen(uint16_t trans_id);
struct packet *cmd_spec_acq_cfg_gen(uint16_t trans_id,
				    uint64_t f0, uint64_t f1, uint32_t bw_div,
				    uint32_t bin_div, uint32_t n_stack,
				    uint32_t acq_max);
struct packet *cmd_getpos_azel_gen(uint16_t trans_id);
struct packet *cmd_spec_acq_enable_gen(uint16_t trans_id);
struct packet *cmd_spec_acq_disable_gen(uint16_t trans_id);
struct packet *cmd_spec_acq_cfg_get_gen(uint16_t trans_id);
struct packet *cmd_control_gen(uint16_t trans_id, const uint8_t *digest,
			       uint16_t len);
struct packet *cmd_message_gen(uint16_t trans_id, const uint8_t *message,
			       uint16_t len);
struct packet *cmd_nick_gen(uint16_t trans_id, const uint8_t *nick,
			    uint16_t len);


/* command generation and sending functions */

void cmd_invalid_pkt(uint16_t trans_id);
void cmd_capabilities(uint16_t trans_id);
void cmd_success(uint16_t trans_id);
void cmd_fail(uint16_t trans_id);
void cmd_moveto_azel(uint16_t trans_id, double az, double el);
void cmd_recalibrate_pointing(uint16_t trans_id);
void cmd_park_telescope(uint16_t trans_id);
void cmd_spec_acq_cfg(uint16_t trans_id,
		      uint64_t f0, uint64_t f1, uint32_t bw_div,
		      uint32_t bin_div, uint32_t n_stack, uint32_t acq_max);
void cmd_getpos_azel(uint16_t trans_id);
void cmd_spec_acq_enable(uint16_t trans_id);
void cmd_spec_acq_disable(uint16_t trans_id);
void cmd_spec_acq_cfg_get(uint16_t trans_id);
void cmd_control(uint16_t trans_id, const uint8_t *digest, uint16_t len);
void cmd_message(uint16_t trans_id, const uint8_t *message, uint16_t len);
void cmd_nick(uint16_t trans_id, const uint8_t *nick, uint16_t len);


#endif /* _INCLUDE_CMD_H_ */

