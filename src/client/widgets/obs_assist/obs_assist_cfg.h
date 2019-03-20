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

		gdouble glon_cur;
		gint rpt_cur;

		gint n_avg;
		gint n_rpt;

		gboolean wait;

		GtkSpinButton *sb_deg;
		GtkSpinButton *sb_lo;
		GtkSpinButton *sb_hi;
		GtkSpinButton *sb_avg;
		GtkSpinButton *sb_rpt;

		GtkWidget *pbar_glon;
		GtkWidget *pbar_rpt;

		GtkWidget *plt;

	} gal_plane;


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

};


#endif /* _WIDGETS_OBS_ASSIST_CFG_H_ */
