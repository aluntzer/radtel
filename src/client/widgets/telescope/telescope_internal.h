/**
 * @file    widgets/telescope/telescope_internal.h
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

#ifndef _WIDGETS_TELESCOPE_INTERNAL_H_
#define _WIDGETS_TELESCOPE_INTERNAL_H_

#include <telescope.h>

GtkWidget *telescope_coord_ctrl_new(Telescope *p);
GtkWidget *telescope_pos_ctrl_new(Telescope *p);
GtkWidget *telescope_get_pos_new(Telescope *p);
GtkWidget *telescope_park_ctrl_new(Telescope *p);
GtkWidget *telescope_recal_pointing_new(Telescope *p);
GtkWidget *telescope_track_sky_new(Telescope *p);

void telescope_update_movement_range(Telescope *p);
void telescope_update_azel_internal(Telescope *p);
void telescope_tracker_ctrl(gpointer instance, gboolean state,
			    gdouble az, gdouble el, Telescope *p);

void telescope_tracker_getpos_azel_cb(gpointer instance,
				      struct getpos *pos, Telescope *p);

void telescope_tracker_moveto_azel_cb(gpointer instance,
				      gdouble az, gdouble el, Telescope *p);


#endif /* _WIDGETS_TELESCOPE_INTERNAL_H_ */
