/**
 * @file    widgets/xyplot/xyplot.c
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
 * @brief Create an XY plot. Note that this is not meant to be a
 *	  general-purpose plotting widget (but it could be turned into one if
 *	  needed)
 * @note if needed, a possible speed-up for overlays may be to keep the
 *	 rectangle of the previous, draw over the old overlay, then draw the
 *	 new one instead of copying it every time
 *
 */

#include <cairo.h>
#include <gtk/gtk.h>

#include <string.h>
#include <math.h>
#include <float.h>

#include <xyplot.h>


#define ARRAY_SIZE(x)	(sizeof(x)/sizeof(x[0]))

G_DEFINE_TYPE(XYPlot, xyplot, GTK_TYPE_DRAWING_AREA)



static void xyplot_save_pdf(GtkWidget *widget, gpointer data)
{
	g_message("save!");
}


/**
 * @brief build the right-click popup menu
 */

static void xyplot_build_popup_menu(GtkWidget *widget)
{
	XYPlot *p;
	GtkWidget *menuitem;


	p = XYPLOT(widget);

	p->menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label("Export as PDF");
	
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(xyplot_save_pdf), widget);
	
	gtk_menu_shell_append(GTK_MENU_SHELL(p->menu), menuitem);

	gtk_widget_show_all(p->menu);
}


/**
 * @brief show the right-click popup menu
 */

static void xyplot_popup_menu(GtkWidget *widget)
{
	XYPlot *p;

	
	p = XYPLOT(widget);

	gtk_menu_popup_at_pointer(GTK_MENU(p->menu), NULL);
}


/**
 * @brief create a pango layout of a buffer
 *
 * @param cr the cairo context
 * @param buf a text buffer
 * @param len the text length
 *
 * @return a PangoLayout
 */

static PangoLayout *xyplot_create_layout(cairo_t *cr,
					  const char *buf,
					  const size_t len)
{
	PangoLayout *layout;

	layout = pango_cairo_create_layout(cr);
	pango_layout_set_markup(layout, buf, len);
	
	return layout;
}


/**
 * @brief render and release a PangoLayout
 *
 * @param cr a cairo context
 * @param layout a pango layout
 * @param x a x coordinate
 * @param y a y coordinate
 */

static void xyplot_render_layout(cairo_t *cr, PangoLayout *layout,
				 const int x, const int y)
{
	cairo_move_to(cr, x, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
}



/**
 * @brief draw the background
 *
 * @param cr the cairo context to draw on
 */

static void xyplot_draw_bg(cairo_t *cr)
{
	cairo_save(cr);

	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
	cairo_paint(cr);

	cairo_set_source_rgba(cr,1.0, 1.0, 1.0, 1.0);
	cairo_stroke(cr);

	cairo_restore(cr);
}

/**
 * @brief write text vertically centered and right aligned at x/y coordinate
 *
 * @param cr the cairo context to draw on
 * @param x the x coordinate
 * @param y the y coordinate
 * @param buf a pointer to the text buffer
 */

static void xyplot_write_text_ralign(cairo_t *cr,
				     const double x, const double y,
				     const char *buf)
{
	cairo_text_extents_t te;


	cairo_save(cr);

	cairo_text_extents(cr, buf, &te);

	/* translate origin to center of text location */
	cairo_translate(cr, x, y);

	/* translate origin so text will be centered */
	cairo_translate(cr, -te.width, te.height * 0.5);

	/* start new path at origin */
	cairo_move_to(cr, 0.0, 0.0);

	cairo_show_text(cr, buf);

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief write text with center at x/y coordinate and a given rotation
 *
 * @param cr the cairo context to draw on
 * @param x the x coordinate
 * @param y the y coordinate
 * @param buf a pointer to the text buffer
 * @param rot a text rotation in radians
 */

static void xyplot_write_text_centered(cairo_t *cr,
				       const double x, const double y,
				       const char *buf, const double rot)
{
	cairo_text_extents_t te;


	cairo_save(cr);

	cairo_text_extents(cr, buf, &te);

	/* translate origin to center of text location */
	cairo_translate(cr, x, y);

	cairo_rotate(cr, rot);

	/* translate origin so text will be centered */
	cairo_translate(cr, -te.width * 0.5 , te.height * 0.5);

	/* start new path at origin */
	cairo_move_to(cr, 0.0, 0.0);

	cairo_show_text(cr, buf);

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief set current dimensions of plot frame
 * */

static void xyplot_update_plot_size(GtkWidget *widget,
				    const double x, const double y,
				    const double w, const double h)
{
	XYPlot *plot;

	plot = XYPLOT(widget);


	plot->plot_x = x;
	plot->plot_y = y;

	plot->plot_w = w;
	plot->plot_h = h;

	plot->scale_x =  w / plot->x_ax.len;
	plot->scale_y =  h / plot->y_ax.len;
}


/**
 * @brief draw the outer plot frame and the axis labels
 */

static void xyplot_draw_plot_frame(GtkWidget *widget, cairo_t *cr,
				   const double width, const double height)
{
	double x, y;
	double w, h;

	cairo_text_extents_t te_x;
	cairo_text_extents_t te_y;

	XYPlot *plot;



	cairo_save(cr);

	plot = XYPLOT(widget);

	/* get extents of the x/y axis labels */
	cairo_text_extents(cr, plot->xlabel, &te_x);
	cairo_text_extents(cr, plot->ylabel, &te_y);

	/* start of the plot frame; add extra space for y label */
	x = plot->pad + 4.0 * te_y.height;
	y = plot->pad;

	/* size of frame; subtract paddings and extra text space */
	w = width  - 2.0 * (plot->pad + 2.0 * te_y.height);
	h = height - 2.0 * (plot->pad + 2.0 * te_x.height);

	xyplot_update_plot_size(widget, x, y, w, h);


	cairo_set_line_width(cr, 2.0);

	/* draw the frame */
	cairo_rectangle(cr, x, y, w , h);

	/* place both labels twice their extent height from the frame, so
	 * there is sufficient space to render the tick labels
	 */

	/* draw the x-axis label */
	xyplot_write_text_centered(cr,
				   x + 0.5 * w ,
				   y + h + 4.0 * te_x.height,
				   plot->xlabel,
				   0.0);

	/* draw the y-axis label */
	xyplot_write_text_centered(cr,
				   te_y.height,
				   x + 0.5 * h - 4.0 * te_y.height,
				   plot->ylabel,
				   -90.0 * M_PI / 180.0);

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief modify cairo context so coordinate origin is in lower left
 *	  corner of plot frame
 */

static void xyplot_transform_origin(GtkWidget *widget, cairo_t *cr)
{
	cairo_matrix_t matrix;

	XYPlot *p;


	p = XYPLOT(widget);

	/* translate to lower left corner */
	cairo_translate(cr, p->plot_x, p->plot_y + p->plot_h);

	/* flip cairo transformation matrix for more intuitive use */
	cairo_get_matrix(cr, &matrix);
	matrix.yy = - matrix.yy;
	cairo_set_matrix(cr, &matrix);
}


/**
 * @brief draw tick marks on x-axis
 *
 * @param cr the cairo context to draw on
 * @param width the area width
 * @param height the area height
 *
 */

static void xyplot_draw_ticks_x(GtkWidget *widget, cairo_t *cr,
				const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;

	XYPlot *p;


	cairo_save(cr);

	p = XYPLOT(widget);

	/* background to white */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, 2.0);

	xyplot_transform_origin(widget, cr);

	idx = p->x_ax.min;
	inc = p->x_ax.step;
	stp = p->x_ax.max + 0.5 * inc;
	min = p->x_ax.min;
	scl = p->scale_x;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, (idx - min) * scl, 0.0);
		cairo_rel_line_to(cr, 0.0, 10.);

		if(idx < stp) {
			cairo_move_to(cr, (idx + 0.5 * inc - min) * scl, 0.0);
			cairo_rel_line_to(cr, 0.0, 5.);
		}
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw tick marks on y-axis
 *
 * @param cr the cairo context to draw on
 * @param width the area width
 * @param height the area height
 *
 */

static void xyplot_draw_ticks_y(GtkWidget *widget, cairo_t *cr,
				const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;

	XYPlot *p;


	cairo_save(cr);

	p = XYPLOT(widget);

	/* background to white */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, 2.0);

	xyplot_transform_origin(widget, cr);

	/* horizontal grid lines */
	idx = p->y_ax.min;
	inc = p->y_ax.step;
	stp = p->y_ax.max + 0.5 * inc;
	min = p->y_ax.min;
	scl = p->scale_y;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, 0.0, (idx - min) * scl);
		cairo_rel_line_to(cr, 10.0, 0.0);

		if(idx < stp) {
			cairo_move_to(cr, 0.0, (idx + 0.5 * inc - min) * scl);
			cairo_rel_line_to(cr, 5.0, 0.0);
		}
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw labels to y-axis ticks
 */

static void xyplot_draw_tickslabels_x(GtkWidget *widget, cairo_t *cr,
				      const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double off;

	char buf[14];	/* should be large enough */

	XYPlot *p;

	cairo_text_extents_t te;


	cairo_save(cr);

	p = XYPLOT(widget);

	/* translate to lower left corner */
	cairo_translate(cr, p->plot_x, p->plot_y + p->plot_h);

	/* get text extents to determine the height of the current font */
	cairo_text_extents(cr, "0", &te);

	/* horizontal ticks */
	idx = p->x_ax.min;
	inc = p->x_ax.step;
	stp = p->x_ax.max + 0.5 * inc;
	min = p->x_ax.min;
	scl = p->scale_x;
	off = 1.5 * te.height;

	for ( ; idx < stp; idx += inc) {
		snprintf(buf, ARRAY_SIZE(buf), "%.2g", idx);
		xyplot_write_text_centered(cr, (idx - min) * scl,
					   off, buf, 0.0);
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw labels to y-axis ticks
 */

static void xyplot_draw_tickslabels_y(GtkWidget *widget, cairo_t *cr,
				      const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;

	char buf[14];	/* should be large enough */

	XYPlot *p;

	cairo_text_extents_t te;


	cairo_save(cr);

	p = XYPLOT(widget);

	/* translate to lower left corner */
	cairo_translate(cr, p->plot_x, p->plot_y + p->plot_h);

	/* get text extents to determine the width of a character of the current
	 * font and use it as offset from the y-axis
	 */
	cairo_text_extents(cr, "0", &te);

	/* vertical ticks */
	idx = p->y_ax.min;
	inc = p->y_ax.step;
	stp = p->y_ax.max + 0.5 * inc;
	min = p->y_ax.min;
	scl = p->scale_y;

	for ( ; idx < stp; idx += inc) {
		snprintf(buf, ARRAY_SIZE(buf), "%.2g", idx);
		xyplot_write_text_ralign(cr, -te.width, (min - idx) * scl, buf);
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw vertical grid lines (parallel to y-axis)
 *
 * @param cr the cairo context to draw on
 * @param width the area width
 * @param height the area height
 *
 */

static void xyplot_draw_grid_y(GtkWidget *widget, cairo_t *cr,
			       const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double end;

	XYPlot *p;


	const double dashes[] = {2.0, 2.0};


	cairo_save(cr);

	p = XYPLOT(widget);

	xyplot_transform_origin(widget, cr);

	/* background to white */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, 1.0);
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);

	/* vertical grid lines */
	idx = p->x_ax.min;
	inc = p->x_ax.step;
	stp = p->x_ax.max + 0.5 * inc;
	min = p->x_ax.min;
	scl = p->scale_x;
	end = p->plot_h;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, (idx - min) * scl, 0);
		cairo_rel_line_to(cr, 0, end);
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw horizontal grid lines (parallel to x-axis)
 *
 * @param cr the cairo context to draw on
 * @param width the area width
 * @param height the area height
 *
 */

static void xyplot_draw_grid_x(GtkWidget *widget, cairo_t *cr,
			       const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double end;

	XYPlot *p;

	const double dashes[] = {2.0, 2.0};


	cairo_save(cr);

	p = XYPLOT(widget);

	xyplot_transform_origin(widget, cr);

	/* background to white */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, 1.0);
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);

	/* horizontal grid lines */
	idx = p->y_ax.min;
	inc = p->y_ax.step;
	stp = p->y_ax.max + 0.5 * inc;
	min = p->y_ax.min;
	scl = p->scale_y;
	end = p->plot_w;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, 0, (idx - min) * scl);
		cairo_rel_line_to(cr, end, 0);
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw the plot data as stairs (gnuplot histeps)
 */

static void xyplot_draw_stairs(GtkWidget *widget, cairo_t *cr)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;

	XYPlot *p;


	p = XYPLOT(widget);

	/* can't connect a single point by a line */
	if (p->data_len < 2)
		return;

	x  = p->data_x;
	y  = p->data_y;
	sx = p->scale_x;
	sy = p->scale_y;

	cairo_save(cr);

	xyplot_transform_origin(widget, cr);
	
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 2.0);
	
	cairo_move_to(cr, (x[0] - (x[1] - x[0]) * 0.5 -  p->x_ax.min) * sx, 0.0);

	cairo_rel_line_to(cr, 0.0, (y[0] - p->y_ax.min) * sy);
	cairo_rel_line_to(cr, (x[1] - x[0]) * sx, 0.0);

        for(i = 1; i < p->data_len; i++) {
		cairo_rel_line_to(cr, 0.0, (y[i] - y[i - 1]) * sy);
		cairo_rel_line_to(cr, (x[i] - x[i - 1]) * sx, 0.0);
	}

	cairo_rel_line_to(cr, 0.0, - (y[i - 1] - p->y_ax.min) * sy);


	cairo_stroke(cr);

	cairo_restore(cr);
}



/**
 * @brief draw the plot data as circles
 */

static void xyplot_draw_circles(GtkWidget *widget, cairo_t *cr)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;

	XYPlot *p;


	p = XYPLOT(widget);

	x  = p->data_x;
	y  = p->data_y;
	sx = p->scale_x;
	sy = p->scale_y;

	cairo_save(cr);

	xyplot_transform_origin(widget, cr);

	cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
	
        for(i = 0; i < p->data_len; i++) {
		cairo_arc(cr, (x[i] - p->x_ax.min) * sx,
			      (y[i] - p->y_ax.min) * sy,
			       4.0, 0.0, 2.0 * M_PI);
		cairo_fill(cr);    			    
	}

	cairo_restore(cr);
}


/**
 * @brief draw the plot data connected straight lines
 */

static void xyplot_draw_lines(GtkWidget *widget, cairo_t *cr)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;

	XYPlot *p;


	p = XYPLOT(widget);

	/* can't connect a single point by a line */
	if (p->data_len < 2)
		return;

	x  = p->data_x;
	y  = p->data_y;
	sx = p->scale_x;
	sy = p->scale_y;

	cairo_save(cr);

	xyplot_transform_origin(widget, cr);
	
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1.0);
	cairo_set_line_width(cr, 2.0);
	
	cairo_move_to(cr, (x[0] - p->x_ax.min) * sx, (y[0] - p->y_ax.min) * sy);

        for(i = 1; i < p->data_len; i++) {
		cairo_rel_line_to(cr, (x[i] - x[i - 1]) * sx,
				      (y[i] - y[i - 1]) * sy);
	}

	
	cairo_stroke(cr);
	
	cairo_restore(cr);
}


/**
 * @brief set the label for the X-Axis
 */

void xyplot_set_xlabel(GtkWidget *widget, gchar *label)
{
	XYPlot *plot;


	plot = XYPLOT(widget);

	plot->xlabel = label;
	gtk_widget_queue_draw(widget);
}


/**
 * @brief set the label for the Y-Axis
 */

void xyplot_set_ylabel(GtkWidget *widget, gchar *label)
{
	XYPlot *plot;


	plot = XYPLOT(widget);

	plot->ylabel = label;
	gtk_widget_queue_draw(widget);
}


/**
 * @brief set padding around plot
 */

void xyplot_set_padding(GtkWidget *widget, gdouble pad)
{
	XYPlot *plot;


	plot = XYPLOT(widget);

	plot->pad = pad;
	gtk_widget_queue_draw(widget);
}


/**
 * @brief calculate a nice number
 *
 * @see Paul S. Heckbert's article in Andrew S. Glassner: Graphics Gems
 */

double xyplot_nicenum(const double num, const gboolean round)
{
	int exp;

	double f;
	double nf;


	exp = floor(log10(num));

	f = num / pow(10, exp);

	if(round) {
		if (f < 1.5) {
			nf = 1.0;
		} else if (f < 3.0) {
			nf = 2.0;
		} else if(f < 7.0) {
			nf = 5.0;
		} else {
			nf = 10.0;
		}
	} else {
		if (f <= 1.0) {
			nf = 1.0;
		} else if (f < 2.0) {
			nf = 2.0;
		} else if(f<5.0) {
			nf=5.0;
		} else {
			nf=10.0;
		}
	}

	return nf * pow(10, exp);
}


/**
 * @brief auto-set the plot axes given the data
 */

static void xyplot_auto_axes(GtkWidget *widget)
{

	XYPlot *xyplot;

	xyplot = XYPLOT(widget);


	xyplot->x_ax.len  = xyplot_nicenum(xyplot->xlen, FALSE);
	xyplot->x_ax.step = xyplot_nicenum(xyplot->x_ax.len /
		       		    (xyplot->x_ax.ticks_maj - 1.0),
		       		    TRUE);
	xyplot->x_ax.min  = floor(xyplot->xmin / xyplot->x_ax.step) *
		            xyplot->x_ax.step;

	xyplot->x_ax.max  = ceil(xyplot->xmax / xyplot->x_ax.step) *
		            xyplot->x_ax.step;

	/* make sure there always is some space left and right */
	if (xyplot->x_ax.min == xyplot->xmin)
		xyplot->x_ax.min -= xyplot->x_ax.step;

	if (xyplot->x_ax.max == xyplot->xmax)
		xyplot->x_ax.max += xyplot->x_ax.step;

	xyplot->x_ax.len  = xyplot->x_ax.max - xyplot->x_ax.min;

	xyplot->x_ax.prec = MAX(-floor(log10(xyplot->x_ax.step)), 0);



	xyplot->y_ax.len  = xyplot_nicenum(xyplot->ylen, FALSE);
	xyplot->y_ax.step = xyplot_nicenum(xyplot->y_ax.len /
		       		    (xyplot->y_ax.ticks_maj - 1.0),
		       		    TRUE);
	xyplot->y_ax.min  = floor(xyplot->ymin / xyplot->y_ax.step) *
		            xyplot->y_ax.step;

	xyplot->y_ax.max  = ceil(xyplot->ymax / xyplot->y_ax.step) *
		            xyplot->y_ax.step;

	/* as above, for bottom and top */
	if (xyplot->y_ax.min == xyplot->ymin)
		xyplot->y_ax.min -= xyplot->y_ax.step;

	if (xyplot->y_ax.max == xyplot->ymax)
		xyplot->y_ax.max += xyplot->y_ax.step;


	xyplot->y_ax.len  = xyplot->y_ax.max - xyplot->y_ax.min;

	xyplot->y_ax.prec = MAX(-floor(log10(xyplot->y_ax.step)), 0);
}


/**
 * @brief auto-set the ranges of the supplied data
 *
 */

static void xyplot_auto_range(GtkWidget *widget)
{
	size_t i;

	gdouble tmp;

	XYPlot *xyplot;



	xyplot = XYPLOT(widget);

	xyplot->xmin = DBL_MAX;
	xyplot->ymin = DBL_MAX;

	xyplot->xmax = -DBL_MAX;
	xyplot->ymax = -DBL_MAX;


	for(i = 0; i < xyplot->data_len; i++) {

		tmp = xyplot->data_x[i];

		if(tmp < xyplot->xmin)
			xyplot->xmin = tmp;

		if(tmp > xyplot->xmax)
			xyplot->xmax = tmp;

	}

	for(i = 0; i < xyplot->data_len; i++) {

		tmp = xyplot->data_y[i];

		if(tmp < xyplot->ymin)
			xyplot->ymin = tmp;

		if(tmp > xyplot->ymax)
			xyplot->ymax = tmp;

	}


	xyplot->xlen = xyplot->xmax - xyplot->xmin;
	xyplot->ylen = xyplot->ymax - xyplot->ymin;
}


/**
 * @brief draws the plot
 *
 * @note the plot is rendered onto it's own surface, so we don't need
 *	 to redraw it every time we want to add or update an overlay
 */

static void xyplot_plot(GtkWidget *widget)
{
	unsigned int width, height;

	XYPlot *p;
	cairo_t *cr;


	p = XYPLOT(widget);
	
	
	cr = cairo_create(p->plot);

	width  = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	xyplot_draw_bg(cr);

	xyplot_draw_plot_frame(widget, cr, width, height);


	if (p->data_len) {
		xyplot_draw_grid_x(widget, cr, width, height);
		xyplot_draw_grid_y(widget, cr, width, height);
		xyplot_draw_stairs(widget, cr);
		xyplot_draw_lines(widget, cr);
		xyplot_draw_circles(widget, cr);
		xyplot_draw_ticks_x(widget, cr, width, height);
		xyplot_draw_ticks_y(widget, cr, width, height);
		xyplot_draw_tickslabels_x(widget, cr, width, height);
		xyplot_draw_tickslabels_y(widget, cr, width, height);
	} else {
		xyplot_write_text_centered(cr, 0.5 * width, 0.5 * height,
					   "NO DATA", 0.0);
	}

	cairo_destroy(cr);

	/* duplicate to render surface */
	cr = cairo_create(p->render);
	cairo_set_source_surface(cr, p->plot, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);

	
	gtk_widget_queue_draw(widget);
}


/**
 * @brief the draw signal callback
 *
 * @note draws the current render surface, which is typically the plot surface
 *	 and optionally some overlay
 */

static gboolean xyplot_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	XYPlot *p;

	p = XYPLOT(widget);

	cairo_set_source_surface(cr, p->render, 0.0, 0.0);

	cairo_paint(cr);


	return TRUE;
}


/**
 * @brief handle mouse cursor enter/leave events
 */

static gboolean xyplot_pointer_crossing_cb(GtkWidget *widget,
					   GdkEventCrossing *event)
{
	GdkWindow  *window;
	GdkDisplay *display;
	GdkCursor  *cursor;

       	
	display = gtk_widget_get_display(widget);
	window  = gtk_widget_get_window(widget);
		

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		cursor = gdk_cursor_new_from_name(display, "cell");
		break;
	case GDK_LEAVE_NOTIFY:
	default:
		cursor = gdk_cursor_new_from_name(display, "default");
		break;
	}


	gdk_window_set_cursor(window, cursor);

	g_object_unref (cursor);

	return TRUE;
}



/**
 * @brief handle mouse cursor motion events
 */

static gboolean xyplot_motion_notify_event_cb(GtkWidget *widget,
					      GdkEventMotion *event)
{
	int w, h, off;

	int x0, y0;

	gdouble x, y; 
	gdouble px, py;
	
	XYPlot *p;
	cairo_t *cr;
	PangoLayout *layout;
	GdkDisplay *display;
	

	char buf[256];


	if (!event->is_hint)
		goto exit;

	

	p = XYPLOT(widget);

	cr = cairo_create(p->render);

	/* paint plot surface to render surface */	
	cairo_set_source_surface(cr, p->plot, 0, 0);
	cairo_paint(cr);
	cairo_set_source_surface(cr, p->render, 0, 0);



	/* get plot pixel reference */
	px = event->x - p->plot_x;
	py = p->plot_y + p->plot_h - event->y;

	if (px < 0.0)
		goto cleanup;

	if (px > p->plot_w)
		goto cleanup;

	if (py < 0.0)
		goto cleanup;
	
	if (py > p->plot_h)
		goto cleanup;


	/* get data range reference */
	x = px / p->scale_x + p->x_ax.min;
	y = py / p->scale_y + p->y_ax.min;

	
	snprintf(buf, ARRAY_SIZE(buf),
		 "<span foreground='#000000'"
		 "	background='#FFFFFF'"
		 "	font_desc='Sans Bold 8'>"
		 "<tt>"
		 "X: %+2.2g\n"
		 "Y: %+2.2g\n"
		 "</tt>"
		 "</span>",
		 x, y);

	layout = xyplot_create_layout(cr, buf, strlen(buf));
	pango_layout_get_pixel_size(layout, &w, &h);

	/* correction for cursor size */
	display = gtk_widget_get_display(widget);
	off = (int) gdk_display_get_default_cursor_size(display);

	/* left or right of cursor ? */
	if (w < ((int) (p->plot_w - px) - off))
		x0 = event->x + off; 
	else
		x0 = event->x - w - off; 
	
	/* below or above cursor? */	
	if (h > ((int) (p->plot_h - py) - off))
		y0 = event->y; 
	else
		y0 = event->y - off; 


	xyplot_render_layout(cr, layout, x0, y0);


cleanup:
	cairo_destroy (cr);

	/* _draw_area() may leave artefacts if the pointer is moved to fast */
	gtk_widget_queue_draw(widget);

exit:
	return TRUE;
}





/** 
 * @brief button press events
 */

static gboolean xyplot_button_press_cb(GtkWidget *widget, GdkEventButton *event,
				       gpointer data)
{



	if (event->type != GDK_BUTTON_PRESS)
		goto exit;

	if (event->button != 3)
		goto exit;

	xyplot_popup_menu(widget);

exit:
	return TRUE;
}


/** 
 * @brief create a new surface on configure event
 */

static gboolean xyplot_configure_event_cb(GtkWidget *widget,
					  GdkEventConfigure *event,
					  gpointer data)
{
	unsigned int width, height;

	GdkWindow *win;
	XYPlot *p;


	win = gtk_widget_get_window(widget);

	if (!win)
		goto exit;

	
	p = XYPLOT(widget);

	if (p->render)
	    cairo_surface_destroy(p->render);


	width  = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	p->plot = gdk_window_create_similar_surface(win, CAIRO_CONTENT_COLOR,
						       width, height);
	
	p->render = gdk_window_create_similar_surface(win, CAIRO_CONTENT_COLOR,
						       width, height);
	
	xyplot_plot(widget);

exit:
	return TRUE;
}



/**
 * @brief initialise the XYPlot class
 */

static void xyplot_class_init(XYPlotClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the plot parameters
 */

static void xyplot_init(XYPlot *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_XYPLOT(p));

	/* connect the relevant signals of the DrawingArea */
	g_signal_connect(G_OBJECT(&p->parent), "draw",
			  G_CALLBACK(xyplot_draw_cb), NULL);
	
	g_signal_connect(G_OBJECT(&p->parent), "motion-notify-event",
			 G_CALLBACK(xyplot_motion_notify_event_cb), NULL);
	
	g_signal_connect(G_OBJECT(&p->parent), "configure-event",
			 G_CALLBACK(xyplot_configure_event_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "enter-notify-event",
			  G_CALLBACK (xyplot_pointer_crossing_cb), NULL);
	
	g_signal_connect (G_OBJECT(&p->parent), "leave-notify-event",
			  G_CALLBACK (xyplot_pointer_crossing_cb), NULL);
	
	g_signal_connect (G_OBJECT(&p->parent), "button-press-event",
			  G_CALLBACK (xyplot_button_press_cb), NULL);
	

	gtk_widget_set_events(GTK_WIDGET(&p->parent), GDK_EXPOSURE_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_POINTER_MOTION_MASK
			       | GDK_POINTER_MOTION_HINT_MASK
			       | GDK_ENTER_NOTIFY_MASK
			       | GDK_LEAVE_NOTIFY_MASK);

	xyplot_build_popup_menu(GTK_WIDGET(p));

	/* initialisation of plotting internals */
	p->xlabel = "X-Axis";
	p->ylabel = "Y-Axis";
	p->pad    = 20.0;

	p->data_x    = NULL;
	p->data_y    = NULL;
	p->data_len = 0;

	p->x_ax.min       = 0.0;
	p->x_ax.max       = 0.0;
	p->x_ax.len       = 0.0;
	p->x_ax.step      = 0.0;
	p->x_ax.ticks_maj = 5.0;
	p->x_ax.prec      = 0.0;

	p->y_ax.min       = 0.0;
	p->y_ax.max       = 0.0;
	p->y_ax.len       = 0.0;
	p->y_ax.step      = 0.0;
	p->y_ax.ticks_maj = 5.0;
	p->y_ax.prec      = 0.0;
}


/**
 * @brief set the data to plot
 *
 * @note we expect len(x) == len(y);
 *
 * @note a valid call to this function will g_free() the previously used data,
 *	 so if you want to keep them, only supply copies
 *
 * @note if one of the pointers is NULL, the original data is kept
 */

void xyplot_set_data(GtkWidget *widget, gdouble *x, gdouble *y, gsize size)
{
	XYPlot *plot;

	plot = XYPLOT(widget);


	if (!x)
		goto error;
	if (!y)
		goto error;

	g_free(plot->data_x);
	g_free(plot->data_y);
	
	plot->data_x    = x;
	plot->data_y    = y;
	plot->data_len = size;

	xyplot_auto_range(widget);
	xyplot_auto_axes(widget);



	xyplot_plot(widget);

	return;

error:
	g_warning("%s: data array is NULL", __func__);

	return;
}




/**
 * @brief create a new XYPlot widget
 */

GtkWidget *xyplot_new(void)
{
	XYPlot *xyplot;

	xyplot = g_object_new(TYPE_XYPLOT, NULL);

	return GTK_WIDGET(xyplot);
}



int main(int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *plot;
	
#define LEN   8
#define X0   -3.0
#define INC   1.0

	gdouble *xdata;
	gdouble *ydata;

	gdouble d = X0;
	int i;
	
	
	gtk_init( &argc, &argv );

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "XYPlot");
	g_signal_connect( window, "destroy", G_CALLBACK (gtk_main_quit), NULL );
	gtk_container_set_border_width( GTK_CONTAINER(window), 10 );
	gtk_widget_show (window);

	plot = xyplot_new();
	gtk_container_add(GTK_CONTAINER(window), plot);
	gtk_widget_show(plot);

	xyplot_set_xlabel(plot, "Frequency");
	xyplot_set_ylabel(plot, "Amplitude");

	xdata = g_malloc(LEN * sizeof(gdouble));
	ydata = g_malloc(LEN * sizeof(gdouble));


	for (i = 0; i < LEN; i++) {
		xdata[i] = d;
		ydata[i] = d * d;

		d += INC;
	}

	xyplot_set_data(plot, xdata, ydata, LEN);

	gtk_widget_show_all(window);

	gtk_main();


	return 0;
}

