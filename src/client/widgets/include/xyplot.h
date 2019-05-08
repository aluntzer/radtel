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
	gdouble tick_min;
	gdouble tick_max;
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
	gdouble dx;	/* delta step x (for map pixels) */

	gdouble ymin;	/* data range in y-axis */
	gdouble ymax;
	gdouble ylen;
	gdouble dy;	/* delta step y */

	gdouble cmin;	/* data range in c-axis */
	gdouble cmax;
	gdouble clen;


	XYPlotAxis x_ax;
	XYPlotAxis y_ax;

	gchar *title;

	gchar *xlabel;
	gchar *ylabel;
	gchar *x2label;
	gchar *y2label;

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
	} rub;

	struct {
		gdouble x0;	/* start of shift */
		gdouble y0;
	} shift;

	gboolean autorange_x;
	gboolean autorange_y;

	struct {
		gdouble pos;
		gchar   *lbl;
	} ind_x;

	struct {
		gdouble pos;
		gchar   *lbl;
	} ind_y;


	/* alt axis conversions */
	gdouble (*conv_to_x2)(gdouble x, gpointer data);
	gdouble (*conv_to_y2)(gdouble y, gpointer data);

	/* extra data for conversion function */
	gpointer x2_userdata;
	gpointer y2_userdata;


	GtkWidget *sc_cmin;
	GtkWidget *sc_cmax;
	GtkWidget *sc_xmin;
	GtkWidget *sc_xmax;
	GtkWidget *sc_ymin;
	GtkWidget *sc_ymax;



	GList *graphs;
	GList *graphs_cleanup;

	GdkRGBA bg_colour;
	GdkRGBA ax_colour;
};

struct _XYPlotClass {
	GtkDrawingAreaClass parent_class;
};

enum xyplot_graph_style {STAIRS, CIRCLES, LINES, NAN_LINES,
			 CURVES, DASHES, SQUARES, IMPULSES, MARIO};

GtkWidget *xyplot_new(void);

void xyplot_set_title(GtkWidget *widget, gchar *title);

void xyplot_set_xlabel(GtkWidget *widget, gchar *label);
void xyplot_set_ylabel(GtkWidget *widget, gchar *label);
void xyplot_set_x2label(GtkWidget *widget, gchar *label);
void xyplot_set_y2label(GtkWidget *widget, gchar *label);
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
void xyplot_select_all_data(GtkWidget *widget);

void xyplot_get_sel_axis_range(GtkWidget *widget,
			   gdouble *xmin, gdouble *xmax,
			   gdouble *ymin, gdouble *ymax);

void xyplot_get_data_axis_range(GtkWidget *widget,
			   gdouble *xmin, gdouble *xmax,
			   gdouble *ymin, gdouble *ymax);


void xyplot_set_range_x(GtkWidget *widget, gdouble min, gdouble max);
void xyplot_set_range_y(GtkWidget *widget, gdouble min, gdouble max);

void xyplot_redraw(GtkWidget *widget);

void xyplot_draw_indicator_x(GtkWidget *widget, gdouble x, gchar *label);
void xyplot_draw_indicator_y(GtkWidget *widget, gdouble y, gchar *label);
void xyplot_erase_indicators(GtkWidget *widget);


void xyplot_set_x2_conversion(GtkWidget *widget,
			      gdouble (*conv_func)(gdouble x, gpointer data),
			      gpointer userdata);

void xyplot_set_y2_conversion(GtkWidget *widget,
			      gdouble (*conv_func)(gdouble y, gpointer data),
			      gpointer userdata);


static const GdkRGBA COLOR_YELLOW_PHOS = {0.804, 0.592, 0.047, 0.6};
static const GdkRGBA COLOR_WHITE = {1.0, 1.0, 1.0, 0.7};
static const GdkRGBA red = {1.0, 0.0, 0.0, 1.0};



#endif /* _WIDGETS_XYPLOT_H_ */

