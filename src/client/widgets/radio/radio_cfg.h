/**
 * @file    widgets/radio/radio_cfg.h
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

#ifndef _WIDGETS_RADIO_CFG_H_
#define _WIDGETS_RADIO_CFG_H_

#include <radio.h>
#include <cmd.h>

struct _RadioConfig {
	gdouble prec;
	struct capabilities c;

	gdouble az;
	gdouble el;

	gdouble lat;
	gdouble lon;

	gdouble freq_ref_mhz;

	gboolean mode_freq;
	gboolean mode_lohi;

	GtkSpinButton *sb_frq_lo;
	GtkSpinButton *sb_frq_hi;
	GtkSpinButton *sb_frq_ce;
	GtkSpinButton *sb_frq_bw;

	GtkSpinButton *sb_vel_lo;
	GtkSpinButton *sb_vel_hi;
	GtkSpinButton *sb_vel_ce;
	GtkSpinButton *sb_vel_bw;


	GtkToggleButton *rb_frq;
	GtkToggleButton *rb_vel;
	GtkToggleButton *rb_lohi;
	GtkToggleButton *rb_cbw;





	GtkLabel *sb_frq_lo_center_lbl;
	GtkLabel *sb_frq_hi_bw_lbl;


	GtkSpinButton *sb_bw_div;
	GtkSpinButton *sb_bin_div;
	GtkLabel *sb_bw_lbl;
	GtkLabel *sb_bin_lbl;

	GtkSpinButton *sb_avg;

	GtkSpinButton *sb_limit;

	GtkLabel *freq_cfg;
	GtkLabel *bw_cfg;
	GtkLabel *avg_cfg;

	GtkSwitch *sw_acq;

	GtkSwitch *sw_dpl;


	int bw_div;		/* bandwidth divider */
	int bin_div;		/* per-bandwidth bin divider */

	gboolean tracking;

	guint id_to;

	guint id_cap;
	guint id_acq;
	guint id_cfg;
	guint id_ena;
	guint id_dis;
	guint id_pos;


	guint id_vh;
	guint id_vl;
	guint id_vc;
	guint id_vs;
	guint id_fh;
	guint id_fl;
	guint id_fc;
	guint id_fs;

};


#endif /* _WIDGETS_RADIO_CFG_H_ */
