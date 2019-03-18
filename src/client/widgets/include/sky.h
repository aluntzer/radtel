/**
 * @file    widgets/include/sky.h
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
#define SKY(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SKY, Sky))
#define SKY_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_SKY, SkyClass))
#define IS_SKY(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_SKY))
#define IS_SKY_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_SKY))
#define SKY_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_SKY, SkyClass))

typedef struct _Sky       Sky;
typedef struct _SkyClass  SkyClass;
typedef struct _SkyConfig SkyPrivate;


struct _Sky {
	GtkDrawingArea parent;
	SkyPrivate *cfg;
};

struct _SkyClass {
	GtkDrawingAreaClass parent_class;
};

GtkWidget *sky_new(void);
void sky_set_xlabel(GtkWidget *widget, gchar *label);
void sky_set_ylabel(GtkWidget *widget, gchar *label);
void sky_set_padding(GtkWidget *widget, gdouble pad);
void sky_set_data(GtkWidget *widget, gdouble *x, gdouble *y, gsize size);


#endif /* _WIDGETS_SKY_H_ */

