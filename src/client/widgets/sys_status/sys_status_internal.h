/**
 * @file    widgets/sys_status/sys_status_internal.h
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

#ifndef _WIDGETS_SYS_STATUS_INTERNAL_H_
#define _WIDGETS_SYS_STATUS_INTERNAL_H_

#include <sys_status.h>


GtkWidget *sys_status_info_bar_new(SysStatus *p);
GtkWidget *sys_status_pos_new(SysStatus *p);
void sys_status_handle_status_push(gpointer instance, const gchar *msg,
				   gpointer data);

GtkWidget *sys_status_create_align_lbl(const gchar *str, double align);

#endif /* _WIDGETS_SYS_STATUS_INTERNAL_H */
