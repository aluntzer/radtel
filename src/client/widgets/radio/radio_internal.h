/**
 * @file    widgets/radio/radio_internal.h
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

#ifndef _WIDGETS_RADIO_INTERNAL_H_
#define _WIDGETS_RADIO_INTERNAL_H_

#include <radio.h>

GtkWidget *radio_acq_freq_range_ctrl_new(Radio *p);
GtkWidget *radio_acq_input_mode_ctrl_new(Radio *p);
GtkWidget *radio_spec_acq_ctrl_new(Radio *p);
GtkWidget *radio_spec_avg_ctrl_new(Radio *p);
GtkWidget *radio_acq_res_ctrl_new(Radio *p);
GtkWidget *radio_spec_cfg_ctrl_get_new(Radio *p);
GtkWidget *radio_spec_cfg_ctrl_set_new(Radio *p);
GtkWidget *radio_spec_acq_num_ctrl_new(Radio *p);
GtkWidget *radio_spec_doppler_ctrl_new(Radio *p);
GtkWidget *radio_vrest_ctrl_new(Radio *p);
GtkWidget *radio_hot_load_ctrl_new(Radio *p);

void radio_update_avg_lbl(Radio *p);
void radio_update_conf_bw_lbl(Radio *p);
void radio_update_conf_freq_lbl(Radio *p);
void radio_update_freq_range(Radio *p);
void radio_update_bin_divider(Radio *p);
void radio_update_bw_divider(Radio *p);
void radio_update_avg_range(Radio *p);
void radio_update_hot_load_lbl(Radio *p);

gboolean radio_spec_acq_cmd_spec_acq_enable(gpointer instance, gpointer data);
gboolean radio_spec_acq_cmd_spec_acq_disable(gpointer instance, gpointer data);

gboolean radio_hot_load_cmd_hot_load_enable(gpointer instance, gpointer data);
gboolean radio_hot_load_cmd_hot_load_disable(gpointer instance, gpointer data);

void radio_input_block_signals(Radio *p);
void radio_input_unblock_signals(Radio *p);
void radio_input_freq_val_refresh(Radio *p);


#endif /* _WIDGETS_RADIO_INTERNAL_H_ */
