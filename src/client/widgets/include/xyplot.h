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

	gdouble cmin;	/* data range in c-axis */
	gdouble cmax;
	gdouble clen;

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
	gdouble scale_c;


	struct {
		gdouble xmin;
		gdouble xmax;
		gdouble ymin;
		gdouble ymax;
		gboolean active;
	} sel;

	struct {
		gdouble x0;	/* start of rubber band */
		gdouble y0;

		gdouble px0;	/* rubber band selection in plot pixel ref. */
		gdouble px1;
		gdouble py0;
		gdouble py1;

		gboolean autorange;
	} rub;



	GList *graphs;
	GList *graphs_cleanup;

	GdkRGBA bg_colour;
	GdkRGBA ax_colour;
};

struct _XYPlotClass {
	GtkDrawingAreaClass parent_class;
};

enum xyplot_graph_style {STAIRS, CIRCLES, LINES, NAN_LINES, CURVES, DASHES, SQUARES};

GtkWidget *xyplot_new(void);
void xyplot_set_xlabel(GtkWidget *widget, gchar *label);
void xyplot_set_ylabel(GtkWidget *widget, gchar *label);
void xyplot_set_padding(GtkWidget *widget, gdouble pad);

void *xyplot_add_graph(GtkWidget *widget,
		       gdouble *x, gdouble *y, gdouble *c,
		       gsize size, gchar *label);

void xyplot_drop_graph(GtkWidget *widget, void *ref);
void xyplot_drop_all_graphs(GtkWidget *widget);


void xyplot_set_graph_style(GtkWidget *widget, void *ref,
			    enum xyplot_graph_style style);

void xyplot_set_graph_rgba(GtkWidget *widget, void *ref,
			   GdkRGBA colour);
int xyplot_get_graph_rgba(GtkWidget *widget, void *ref, GdkRGBA *colour);

size_t xyplot_get_selection_data(GtkWidget *widget,
				 gdouble **x, gdouble **y, gdouble **c);

void xyplot_get_sel_axis_range(GtkWidget *widget,
			   gdouble *xmin, gdouble *xmax,
			   gdouble *ymin, gdouble *ymax);

void xyplot_get_data_axis_range(GtkWidget *widget,
			   gdouble *xmin, gdouble *xmax,
			   gdouble *ymin, gdouble *ymax);
void xyplot_redraw(GtkWidget *widget);


#endif /* _WIDGETS_XYPLOT_H_ */

