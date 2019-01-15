/**
 * @file    widgets/telescope/telescope_cfg.h
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

#ifndef _WIDGETS_TELESCOPE_CFG_H_
#define _WIDGETS_TELESCOPE_CFG_H_

#include <telescope.h>
#include <cmd.h>

struct _TelescopeConfig {

	struct capabilities c;

	gdouble az_min;
	gdouble az_max;
	gdouble az_res;

	gdouble el_min;
	gdouble el_max;
	gdouble el_res;

	gdouble lat;
	gdouble lon;


	GtkComboBox   *coord_ref_cb;

	GtkSpinButton *sb_az;
	GtkSpinButton *sb_ra_glat;
	GtkLabel      *sb_az_ra_glat_lbl;

	GtkSpinButton *sb_el;
	GtkSpinButton *sb_de_glon;
	GtkLabel      *sb_el_de_glon_lbl;


	GtkLabel      *not_vis_lbl;


};


#endif /* _WIDGETS_TELESCOPE_CFG_H_ */
