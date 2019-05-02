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
#include <glib.h>

int process_pkt(struct packet *pkt, gboolean priv, gpointer ref);

/* command processing functions */
void proc_pr_invalid_pkt(struct packet *pkt, gpointer ref);
void proc_pr_capabilities(struct packet *pkt);
void proc_pr_moveto_azel(struct packet *pkt, gpointer ref);
void proc_pr_recalibrate_pointing(struct packet *pkt, gpointer ref);
void proc_pr_park_telescope(struct packet *pkt, gpointer ref);
void proc_pr_spec_acq_cfg(struct packet *pkt, gpointer ref);
void proc_pr_getpos_azel(struct packet *pkt, gpointer ref);
void proc_pr_spec_acq_enable(struct packet *pkt, gpointer ref);
void proc_pr_spec_acq_disable(struct packet *pkt, gpointer ref);
void proc_pr_spec_acq_cfg_get(struct packet *pkt, gpointer ref);
void proc_pr_control(struct packet *pkt, gpointer ref);
void proc_pr_message(struct packet *pkt, gpointer ref);
void proc_pr_nick(struct packet *pkt, gpointer ref);

#endif /* _SERVER_INCLUDE_PKT_PROC_H_ */

