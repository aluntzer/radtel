/**
 * @file    server/include/pkt_proc.h
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

#ifndef _SERVER_INCLUDE_PKT_PROC_H_
#define _SERVER_INCLUDE_PKT_PROC_H_

#include <protocol.h>

void process_pkt(struct packet *pkt);

/* command processing functions */
void proc_pr_invalid_pkt(void);
void proc_pr_capabilities(void);
void proc_pr_moveto_azel(struct packet *pkt);
void proc_pr_recalibrate_pointing(void);
void proc_pr_park_telescope(void);
void proc_pr_spec_acq_cfg(struct packet *pkt);
void proc_pr_getpos_azel(void);
void proc_pr_spec_acq_enable(void);
void proc_pr_spec_acq_disable(void);

#endif /* _SERVER_INCLUDE_PKT_PROC_H_ */

