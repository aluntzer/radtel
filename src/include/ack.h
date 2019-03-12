/**
 * @file    include/ack.h
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
 * @brief all functions with the ack_ prefix are server->client only
 *
 */

#ifndef _INCLUDE_ACK_H_
#define _INCLUDE_ACK_H_

#include <protocol.h>
#include <net_common.h>

void ack_invalid_pkt(uint16_t trans_id);
void ack_capabilities(uint16_t trans_id, struct capabilities *c);
void ack_getpos_azel(uint16_t trans_id, struct getpos *pos);
void ack_spec_data(uint16_t trans_id, struct spec_data *s);
void ack_spec_acq_enable(uint16_t trans_id);
void ack_spec_acq_disable(uint16_t trans_id);
void ack_fail(uint16_t trans_id);
void ack_success(uint16_t trans_id);
void ack_spec_acq_cfg(uint16_t trans_id, struct spec_acq_cfg *acq);
void ack_status_acq(uint16_t trans_id, struct status *s);
void ack_status_slew(uint16_t trans_id, struct status *s);
void ack_status_move(uint16_t trans_id, struct status *s);

#endif /* _INCLUDE_ACK_H_ */

