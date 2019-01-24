/**
 * @file    widgets/includes/sswdnd.h
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

#ifndef _WIDGETS_SSWDND_H_
#define _WIDGETS_SSWDND_H_


#include <gtk/gtk.h>


#define TYPE_SSWDND		(sswdnd_get_type())
#define SSWDND(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SSWDND, SSWDnD))
#define SSWDND_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_SSWDND, SSWDnDClass))
#define IS_SSWDND(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_SSWDND))
#define IS_SSWDND_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_SSWDND))
#define SSWDND_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_SSWDND, SSWDnDClass))

typedef struct _SSWDnD		SSWDnD;
typedef struct _SSWDnDClass	SSWDnDClass;


struct _SSWDnD {
	GtkStackSwitcher parent;
};

struct _SSWDnDClass {
	GtkStackSwitcherClass parent_class;
};


GtkWidget *sswdnd_new(void);

void sswdnd_add_named(GtkWidget *p, GtkWidget *w, const gchar *name);
void sswdnd_add_header_buttons(GtkWidget *sswdnd, GtkWidget *header);


#endif /* _WIDGETS_SSWDND_H_ */

