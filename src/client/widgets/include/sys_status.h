/**
 * @file    widgets/includes/sys_status.h
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

#ifndef _WIDGETS_SYS_STATUS_H_
#define _WIDGETS_SYS_STATUS_H_


#include <gtk/gtk.h>


#define TYPE_SYS_STATUS			(sys_status_get_type())
#define SYS_STATUS(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SYS_STATUS, SysStatus))
#define SYS_STATUS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_SYS_STATUS, SysStatusClass))
#define IS_SYS_STATUS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_SYS_STATUS))
#define IS_SYS_STATUS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_SYS_STATUS))
#define SYS_STATUS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_SYS_STATUS, SysStatusClass))

typedef struct _SysStatus	SysStatus;
typedef struct _SysStatusClass	SysStatusClass;
typedef struct _SysStatusConfig	SysStatusPrivate;


struct _SysStatus {
	GtkBox parent;
	SysStatusPrivate *cfg;
};

struct _SysStatusClass {
	GtkBoxClass parent_class;
};


GtkWidget *sys_status_new(void);


#endif /* _WIDGETS_SYS_STATUS_H_ */

