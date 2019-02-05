/**
 * @file    widgets/includes/spectrum.h
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

#ifndef _WIDGETS_SPECTRUM_H_
#define _WIDGETS_SPECTRUM_H_


#include <gtk/gtk.h>


#define TYPE_SPECTRUM			(spectrum_get_type())
#define SPECTRUM(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SPECTRUM, Spectrum))
#define SPECTRUM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_SPECTRUM, SpectrumClass))
#define IS_SPECTRUM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_SPECTRUM))
#define IS_SPECTRUM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_SPECTRUM))
#define SPECTRUM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_SPECTRUM, SpectrumClass))

typedef struct _Spectrum	Spectrum;
typedef struct _SpectrumClass	SpectrumClass;
typedef struct _SpectrumConfig	SpectrumPrivate;


struct _Spectrum {
	GtkBox parent;
	SpectrumPrivate *cfg;
};

struct _SpectrumClass {
	GtkBoxClass parent_class;
};


GtkWidget *spectrum_new(void);


#endif /* _WIDGETS_SPECTRUM_H_ */

