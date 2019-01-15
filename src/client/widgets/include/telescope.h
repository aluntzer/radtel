/**
 * @file    widgets/includes/telescope.h
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

#ifndef _WIDGETS_TELESCOPE_H_
#define _WIDGETS_TELESCOPE_H_


#include <gtk/gtk.h>


#define TYPE_TELESCOPE			(telescope_get_type())
#define TELESCOPE(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TELESCOPE, Telescope))
#define TELESCOPE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_TELESCOPE, TelescopeClass))
#define IS_TELESCOPE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TELESCOPE))
#define IS_TELESCOPE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_TELESCOPE))
#define TELESCOPE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_TELESCOPE, TelescopeClass))

typedef struct _Telescope	Telescope;
typedef struct _TelescopeClass	TelescopeClass;
typedef struct _TelescopeConfig	TelescopePrivate;


struct _Telescope {
	GtkBox parent;
	TelescopePrivate *cfg;
};

struct _TelescopeClass {
	GtkBoxClass parent_class;
};


GtkWidget *telescope_new(void);


#endif /* _WIDGETS_TELESCOPE_H_ */

