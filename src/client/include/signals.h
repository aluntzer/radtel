/**
 * @file    client/include/signals.h
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

#ifndef _CLIENT_INCLUDE_SIGNALS_H_
#define _CLIENT_INCLUDE_SIGNALS_H_

#include <protocol.h>

void sig_pr_success(void);
void sig_pr_fail(uint16_t trans_id);
void sig_pr_capabilities(const struct capabilities *c);
void sig_pr_capabilities_load(const struct capabilities_load *c);
void sig_pr_spec_data(const struct spec_data *c);
void sig_pr_getpos_azel(const struct getpos *pos);
void sig_pr_spec_acq_enable(void);
void sig_pr_spec_acq_disable(void);
void sig_pr_spec_acq_cfg(const struct spec_acq_cfg *acq);
void sig_pr_status_acq(const struct status *s);
void sig_pr_status_slew(const struct status *s);
void sig_pr_status_move(const struct status *s);
void sig_pr_status_rec(const struct status *s);
void sig_pr_moveto_azel(const gdouble az, const gdouble el);
void sig_pr_nopriv(uint16_t trans_id);
void sig_pr_message(const gchar *msg);
void sig_pr_userlist(const gchar *msg);
void sig_pr_hot_load_enable(void);
void sig_pr_hot_load_disable(void);
void sig_pr_video_uri(const gchar *msg);

void sig_status_push(const gchar *msg);
void sig_tracking(gboolean track, double ra, double de);
void sig_shutdown(void);
void sig_connected(void);

gpointer *sig_get_instance(void);
void sig_init(void);

#endif /* _CLIENT_INCLUDE_SIGNALS_H_ */

