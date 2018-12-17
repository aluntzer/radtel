/**
 * @file    widgets/radio/
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

#ifndef _WIDGETS_RADIO_H_
#define _WIDGETS_RADIO_H_


#include <gtk/gtk.h>


#define TYPE_RADIO                  (radio_get_type())
#define RADIO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_RADIO, Radio))
#define RADIO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_RADIO, RadioClass))
#define IS_RADIO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_RADIO))
#define IS_RADIO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_RADIO))
#define RADIO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_RADIO, RadioClass))

typedef struct _Radio		Radio;
typedef struct _RadioClass	RadioClass;
typedef struct _RadioConfig	RadioPrivate;


struct _Radio {
	GtkBox parent;
	RadioPrivate *cfg;
};

struct _RadioClass {
	GtkBoxClass parent_class;
};

GtkWidget *radio_new(void);


#endif /* _WIDGETS_RADIO_H_ */

