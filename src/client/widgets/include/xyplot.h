/**
 * @file    widgets/xyplot/
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

#ifndef _WIDGETS_XYPLOT_H_
#define _WIDGETS_XYPLOT_H_


#include <gtk/gtk.h>


#define TYPE_XYPLOT                  (xyplot_get_type())
#define XYPLOT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_XYPLOT, XYPlot))
#define XYPLOT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_XYPLOT, XYPlotClass))
#define IS_XYPLOT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_XYPLOT))
#define IS_XYPLOT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_XYPLOT))
#define XYPLOT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_XYPLOT, XYPlotClass))

typedef struct _XYPlot      XYPlot;
typedef struct _XYPlotClass XYPlotClass;
typedef struct _XYPlotAxis  XYPlotAxis;

struct _XYPlotAxis {
	gdouble min;	/* plot range in x-axis */
	gdouble max;
	gdouble	len;
	gdouble step;
	gdouble ticks_maj;
	gdouble prec;
};


struct _XYPlot {
	/* these should be private, we'll leave them exposed */
	GtkDrawingArea parent;
	cairo_surface_t *plot;		/* the plotting surface */	
	cairo_surface_t *render;	/* the rendered surface */
	GtkWidget *menu;		/* popup menu */

	gdouble pad;	/* padding space around plot */

	gdouble xmin;	/* data range in x-axis */
	gdouble xmax;
	gdouble xlen;
	
	gdouble ymin;	/* data range in y-axis */
	gdouble ymax;
	gdouble ylen;

	XYPlotAxis x_ax;
	XYPlotAxis y_ax;

	gchar *xlabel;
	gchar *ylabel;

	gdouble plot_x;	/* plot frame starting points and size */
	gdouble plot_y;
	
	gdouble plot_w;
	gdouble plot_h;
	
	gdouble scale_x; /* plot area to data scale */
	gdouble scale_y;

	gdouble *data_x;	/* the data to plot */
	gdouble *data_y;
	gsize    data_len;

};

struct _XYPlotClass {
	GtkDrawingAreaClass parent_class;
};

GtkWidget *xyplot_new(void);
void xyplot_set_xlabel(GtkWidget *widget, gchar *label);
void xyplot_set_ylabel(GtkWidget *widget, gchar *label);
void xyplot_set_padding(GtkWidget *widget, gdouble pad);
void xyplot_set_data(GtkWidget *widget, gdouble *x, gdouble *y, gsize size);


#endif /* _WIDGETS_XYPLOT_H_ */

