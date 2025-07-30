/**
 * @file    widgets/obs_assist/obs_assist_cfg.h
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

#ifndef _WIDGETS_OBS_ASSIST_CFG_H_
#define _WIDGETS_OBS_ASSIST_CFG_H_

#include <obs_assist.h>
#include <cmd.h>



struct spectrum {
	gdouble *x;
	gdouble *y;
	gsize    n;
};

struct crossax {
	GArray *off;
	GArray *amp;

	void *plt_ref_in;
	void *plt_ref_out;

	GtkLabel *fitpar;
};


struct bswitch_pos {
	struct spectrum sp;
	gint n;
	void *ref;
};


struct _ObsAssistConfig {

	gdouble az;
	gdouble el;
	gdouble ra;
	gdouble de;
	gdouble glat;
	gdouble glon;


	gdouble az_min;
	gdouble az_max;
	gdouble az_res;
	gdouble el_min;
	gdouble el_max;
	gdouble el_res;
	gdouble lat;
	gdouble lon;


	guint id_pos;
	guint id_cap;
	guint id_aen;
	guint id_adi;
	guint id_spd;
	guint id_mov;

	gboolean acq_enabled;
	gboolean moving;
	gboolean abort;

	GList *hidden;

	struct spectrum spec;

	struct {
		gdouble az_pt;
		gdouble el_pt;
		gdouble deg;

		gdouble az_cent;	/* horizon center */
		gdouble el_cent;
		gdouble ra_cent;	/* equatorial center, for tracking */
		gdouble de_cent;

		gdouble az_cur;
		gdouble el_cur;

		gdouble az_min;
		gdouble az_max;
		gdouble el_min;
		gdouble el_max;
		gdouble az_cor;
		gdouble az_off;
		gdouble el_off;
		gdouble az_stp;
		gdouble el_stp;

		guint samples;

		gboolean track;

		GtkSpinButton *sb_az;
		GtkSpinButton *sb_el;
		GtkSpinButton *sb_deg;
		GtkSpinButton *sb_sa;

		GtkWidget *pbar_az;
		GtkWidget *pbar_el;

		GtkWidget *plt_az;
		GtkWidget *plt_el;


		struct crossax az;
		struct crossax el;

	} cross;


	struct {
		gdouble glon_lo;
		gdouble glon_hi;
		gdouble glon_stp;
		gdouble glat;

		gdouble glon_cur;
		gint rpt_cur;

		gint n_avg;
		gint n_rpt;

		GtkSpinButton *sb_deg;
		GtkSpinButton *sb_lo;
		GtkSpinButton *sb_hi;
		GtkSpinButton *sb_avg;
		GtkSpinButton *sb_rpt;
		GtkSpinButton *sb_glat;

		GtkWidget *pbar_glon;
		GtkWidget *pbar_rpt;

		GtkWidget *plt;

	} gal_latscan;


	struct {

		GArray *az;
		GArray *el;
		GArray *amp;

		gdouble az_lo;
		gdouble az_hi;
		gdouble az_stp;

		gdouble el_lo;
		gdouble el_hi;
		gdouble el_stp;

		gdouble az_cur;
		gdouble el_cur;

		gint n_avg;

		gboolean wait;

		GtkSpinButton *sb_az_deg;
		GtkSpinButton *sb_el_deg;

		GtkSpinButton *sb_az_hi;
		GtkSpinButton *sb_az_lo;

		GtkSpinButton *sb_el_hi;
		GtkSpinButton *sb_el_lo;

		GtkSpinButton *sb_avg;

		GtkWidget *pbar_az;
		GtkWidget *pbar_el;

		GtkWidget *plt;

	} azel;


	struct {
		gdouble ax_lo;
		gdouble ax_hi;
		gdouble ax_stp;

		gdouble ax_cur;
		gint rpt_cur;

		gint n_avg;

		gboolean ax;

		GtkSpinButton *sb_deg;
		GtkSpinButton *sb_lo;
		GtkSpinButton *sb_hi;
		GtkSpinButton *sb_avg;

		GtkWidget *pbar_ax;

		GtkWidget *plt;

	} spectral_axis;



	struct {
		GtkSpinButton *sb_az_off1_deg;
		GtkSpinButton *sb_az_off2_deg;

		GtkSpinButton *sb_el_off1_deg;
		GtkSpinButton *sb_el_off2_deg;

		GtkSpinButton *sb_avg;
		GtkSpinButton *sb_rpt;


		gdouble ra_cent;
		gdouble de_cent;

		gdouble az_off1;
		gdouble el_off1;

		gdouble az_off2;
		gdouble el_off2;

		gint n_avg;
		gint n_rpt;


		gdouble az_cur;
		gdouble el_cur;

		gint rpt_cur;

		enum {OFF1, TGT, OFF2} pos;

		struct spectrum pos1;
		struct spectrum pos2;
		struct spectrum tgt;

		struct bswitch_pos p1;
		struct bswitch_pos p2;
		struct bswitch_pos tg;
		struct bswitch_pos co;


		GArray *idx;
		GArray *amp;

		GtkWidget *pbar;

		GtkWidget *plt_pos1;
		GtkWidget *plt_pos2;
		GtkWidget *plt_tgt;
		GtkWidget *plt_corr;
		GtkWidget *plt_cont;


	} bswitch;



	struct {

		GArray *glon;
		GArray *glat;
		GArray *amp;

		gdouble glon_lo;
		gdouble glon_hi;
		gdouble glon_stp;

		gdouble glat_lo;
		gdouble glat_hi;
		gdouble glat_stp;

		gdouble glon_cur;
		gdouble glat_cur;

		gint n_avg;

		gboolean wait;

		GtkSpinButton *sb_glon_deg;
		GtkSpinButton *sb_glat_deg;

		GtkSpinButton *sb_glon_hi;
		GtkSpinButton *sb_glon_lo;

		GtkSpinButton *sb_glat_hi;
		GtkSpinButton *sb_glat_lo;

		GtkSpinButton *sb_avg;

		GtkWidget *pbar_glon;
		GtkWidget *pbar_glat;

		GtkWidget *plt;

	} npoint;
};





#endif /* _WIDGETS_OBS_ASSIST_CFG_H_ */
