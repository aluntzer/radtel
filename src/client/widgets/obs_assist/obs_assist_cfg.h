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



	struct {
		gdouble az_pt;
		gdouble el_pt;
		gdouble deg;

		gdouble az_cur;
		gdouble el_cur;
		gdouble az_cent;
		gdouble el_cent;
		gdouble az_min;
		gdouble az_max;
		gdouble el_min;
		gdouble el_max;
		gdouble az_cor;
		gdouble az_off;
		gdouble az_stp;
		gdouble el_off;

		GtkSpinButton *sb_az;
		GtkSpinButton *sb_el;
		GtkSpinButton *sb_deg;

		GtkWidget *pbar_az;
		GtkWidget *pbar_el;

		GtkWidget *plt;

	} cross;

};


#endif /* _WIDGETS_OBS_ASSIST_CFG_H_ */
