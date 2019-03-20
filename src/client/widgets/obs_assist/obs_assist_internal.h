/**
 * @file    widgets/obs_assist/obs_assist_internal.h
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

#ifndef _WIDGETS_OBS_ASSIST_INTERNAL_H_
#define _WIDGETS_OBS_ASSIST_INTERNAL_H_

#include <obs_assist.h>

struct spectrum;



GtkWidget *obs_assist_cross_scan_new(ObsAssist *p);
GtkWidget *obs_assist_gal_plane_scan_new(ObsAssist *p);
GtkWidget *obs_assist_azel_scan_new(ObsAssist *p);

GtkWidget *obs_assist_limits_exceeded_warning(const gchar *direction,
					      const gchar *axis,
					      const double limit_deg);
void obs_assist_on_ignore_warning(GtkWidget *w, gpointer data);
void obs_assist_close_cancel(GtkWidget *widget, gpointer data);
void obs_assist_abort(GtkWidget *w, gpointer data);

void obs_assist_clear_spec(ObsAssist *p);

GtkWidget *obs_assist_create_default(GtkWidget *w);

void obs_assist_hide_procedure_selectors(ObsAssist *p);



#endif /* _WIDGETS_OBS_ASSIST_INTERNAL_H_ */
