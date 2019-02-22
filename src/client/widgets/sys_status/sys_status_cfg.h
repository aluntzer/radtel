/**
 * @file    widgets/sys_status/sys_status_cfg.h
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

#ifndef _WIDGETS_SYS_STATUS_CFG_H_
#define _WIDGETS_SYS_STATUS_CFG_H_

#include <sys_status.h>
#include <cmd.h>

struct _SysStatusConfig {

	gdouble az;
	gdouble el;

	gdouble lat;
	gdouble lon;

	GtkLabel  *lbl_az;
	GtkLabel  *lbl_el;
	GtkLabel  *lbl_ra;
	GtkLabel  *lbl_de;
	GtkLabel  *lbl_glat;
	GtkLabel  *lbl_glon;

	GtkWidget  *spin_acq;
	GtkWidget  *spin_slew;

	GtkWidget *info_bar;
	GtkLabel  *info_bar_lbl;

	guint id_to;
	guint id_pos;

	guint id_cap;
	guint id_acq;
	guint id_slw;
};


#endif /* _WIDGETS_SYS_STATUS_CFG_H_ */
