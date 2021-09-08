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

/* ack packet generation functions */

struct packet *ack_invalid_pkt_gen(uint16_t trans_id);
struct packet *ack_capabilities_gen(uint16_t trans_id, struct capabilities *c);
struct packet *ack_capabilities_load_gen(uint16_t trans_id, struct capabilities_load *c);
struct packet *ack_getpos_azel_gen(uint16_t trans_id, struct getpos *pos);
struct packet *ack_spec_data_gen(uint16_t trans_id, struct spec_data *s);
struct packet *ack_spec_acq_enable_gen(uint16_t trans_id);
struct packet *ack_spec_acq_disable_gen(uint16_t trans_id);
struct packet *ack_fail_gen(uint16_t trans_id);
struct packet *ack_success_gen(uint16_t trans_id);
struct packet *ack_spec_acq_cfg_gen(uint16_t trans_id,
				    struct spec_acq_cfg *acq);
struct packet *ack_status_acq_gen(uint16_t trans_id, struct status *c);
struct packet *ack_status_slew_get(uint16_t trans_id, struct status *c);
struct packet *ack_status_move_gen(uint16_t trans_id, struct status *c);
struct packet *ack_status_rec_gen(uint16_t trans_id, struct status *c);
struct packet *ack_moveto_azel_gen(uint16_t trans_id, double az, double el);
struct packet *ack_nopriv_gen(uint16_t trans_id);
struct packet *ack_userlist_gen(uint16_t trans_id, const uint8_t *userlist,
				uint16_t len);
struct packet *ack_hot_load_enable_gen(uint16_t trans_id);
struct packet *ack_hot_load_disable_gen(uint16_t trans_id);
struct packet *ack_cold_load_enable_gen(uint16_t trans_id);
struct packet *ack_cold_load_disable_gen(uint16_t trans_id);




/* command generation and sending functions */

void ack_invalid_pkt(uint16_t trans_id);
void ack_capabilities(uint16_t trans_id, struct capabilities *c);
void ack_capabilities_load(uint16_t trans_id, struct capabilities_load *c);
void ack_getpos_azel(uint16_t trans_id, struct getpos *pos);
void ack_spec_data(uint16_t trans_id, struct spec_data *s);
void ack_spec_acq_enable(uint16_t trans_id);
void ack_spec_acq_disable(uint16_t trans_id);
void ack_fail(uint16_t trans_id, gpointer ref);
void ack_success(uint16_t trans_id, gpointer ref);
void ack_spec_acq_cfg(uint16_t trans_id, struct spec_acq_cfg *acq);
void ack_status_acq(uint16_t trans_id, struct status *s);
void ack_status_slew(uint16_t trans_id, struct status *s);
void ack_status_move(uint16_t trans_id, struct status *s);
void ack_status_rec(uint16_t trans_id, struct status *s);
void ack_moveto_azel(uint16_t trans_id, double az, double el);
void ack_nopriv(uint16_t trans_id, gpointer ref);
void ack_userlist(uint16_t trans_id, const uint8_t *users, uint16_t len);
void ack_hot_load_enable(uint16_t trans_id);
void ack_hot_load_disable(uint16_t trans_id);
void ack_cold_load_enable(uint16_t trans_id);
void ack_cold_load_disable(uint16_t trans_id);

#endif /* _INCLUDE_ACK_H_ */

