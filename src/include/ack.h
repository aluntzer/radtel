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
 */

#ifndef _INCLUDE_ACK_H_
#define _INCLUDE_ACK_H_

#include <protocol.h>

void ack_invalid_pkt(void);
void ack_capabilities(void);
void ack_getpos_azel(struct getpos *pos);
void ack_spec_acq_enable(void);
void ack_spec_acq_disable(void);

#endif /* _INCLUDE_ACK_H_ */

