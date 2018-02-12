/**
 * @file    widgets/sky/
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

#ifndef _WIDGETS_SKY_H_
#define _WIDGETS_SKY_H_


#include <gtk/gtk.h>


#define TYPE_SKY                  (sky_get_type())
#define SKY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SKY, SKY))
#define SKY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_SKY, SKYClass))
#define IS_SKY(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_SKY))
#define IS_SKY_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_SKY))
#define SKY_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_SKY, SKYClass))

typedef struct _SKY       SKY;
typedef struct _SKYClass  SKYClass;
typedef struct _SKYConfig SKYConfig;



struct _SKY {
	GtkDrawingArea parent;
	SKYConfig *cfg;			/* private stuff */
};

struct _SKYClass {
	GtkDrawingAreaClass parent_class;
};

GtkWidget *sky_new(void);
void sky_set_xlabel(GtkWidget *widget, gchar *label);
void sky_set_ylabel(GtkWidget *widget, gchar *label);
void sky_set_padding(GtkWidget *widget, gdouble pad);
void sky_set_data(GtkWidget *widget, gdouble *x, gdouble *y, gsize size);


#endif /* _WIDGETS_SKY_H_ */

