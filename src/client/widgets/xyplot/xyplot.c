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
 *
 * TODO verify XYPLOT on public interfaces
 */

#include <cairo.h>
#include <cairo-pdf.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <string.h>
#include <math.h>
#include <float.h>

#include <xyplot.h>

#if (G_ENCODE_VERSION(2,58)) > GLIB_VERSION_CUR_STABLE
#include <fuck_you_ubuntu.h>
#endif

#define ARRAY_SIZE(x)	(sizeof(x)/sizeof(x[0]))

G_DEFINE_TYPE(XYPlot, xyplot, GTK_TYPE_DRAWING_AREA)

/* default colours */
#define BG_R	0.200
#define BG_G	0.224
#define BG_B	0.231

#define AXES_R	0.7
#define AXES_G	0.7
#define AXES_B	0.7

#define GRAPH_R	0.804
#define GRAPH_G	0.592
#define GRAPH_B	0.047


struct graph {
	gdouble *data_x;	/* the data to plot */
	gdouble *data_y;
	gdouble *data_c;
	gsize    data_len;

	gdouble xmin;		/* contained data ranges */
	gdouble xmax;

	gdouble ymin;
	gdouble ymax;

	gdouble cmin;
	gdouble cmax;

	gdouble dx;		/* minimum delta */
	gdouble dy;

	gchar *label;

	GdkRGBA colour;
	enum xyplot_graph_style style;

	grefcount ref;

	XYPlot *parent;
};


static void xyplot_plot(XYPlot *p);
static void xyplot_plot_render(XYPlot *p, cairo_t *cr,
			       unsigned int width, unsigned int height);


static void xyplot_auto_range(XYPlot *p);
static void xyplot_auto_axes(XYPlot *p);



// stolen from gnuplot
static double get_color_value_from_formula(int formula, double x)
{
	double DEG2RAD = M_PI/180.0;
	/* the input gray x is supposed to be in interval [0,1] */
	if (formula < 0) {		/* negate the value for negative formula */
		x = 1 - x;
		formula = -formula;
	}
	switch (formula) {
		case 0:
			return 0;
		case 1:
			return 0.5;
		case 2:
			return 1;
		case 3:			/* x = x */
			break;
		case 4:
			x = x * x;
			break;
		case 5:
			x = x * x * x;
			break;
		case 6:
			x = x * x * x * x;
			break;
		case 7:
			x = sqrt(x);
			break;
		case 8:
			x = sqrt(sqrt(x));
			break;
		case 9:
			x = sin(90 * x * DEG2RAD);
			break;
		case 10:
			x = cos(90 * x * DEG2RAD);
			break;
		case 11:
			x = fabs(x - 0.5);
			break;
		case 12:
			x = (2 * x - 1) * (2.0 * x - 1);
			break;
		case 13:
			x = sin(180 * x * DEG2RAD);
			break;
		case 14:
			x = fabs(cos(180 * x * DEG2RAD));
			break;
		case 15:
			x = sin(360 * x * DEG2RAD);
			break;
		case 16:
			x = cos(360 * x * DEG2RAD);
			break;
		case 17:
			x = fabs(sin(360 * x * DEG2RAD));
			break;
		case 18:
			x = fabs(cos(360 * x * DEG2RAD));
			break;
		case 19:
			x = fabs(sin(720 * x * DEG2RAD));
			break;
		case 20:
			x = fabs(cos(720 * x * DEG2RAD));
			break;
		case 21:
			x = 3 * x;
			break;
		case 22:
			x = 3 * x - 1;
			break;
		case 23:
			x = 3 * x - 2;
			break;
		case 24:
			x = fabs(3 * x - 1);
			break;
		case 25:
			x = fabs(3 * x - 2);
			break;
		case 26:
			x = (1.5 * x - 0.5);
			break;
		case 27:
			x = (1.5 * x - 1);
			break;
		case 28:
			x = fabs(1.5 * x - 0.5);
			break;
		case 29:
			x = fabs(1.5 * x - 1);
			break;
		case 30:
			if (x <= 0.25)
				return 0;
			if (x >= 0.57)
				return 1;
			x = x / 0.32 - 0.78125;
			break;
		case 31:
			if (x <= 0.42)
				return 0;
			if (x >= 0.92)
				return 1;
			x = 2 * x - 0.84;
			break;
		case 32:
			if (x <= 0.42)
				x *= 4;
			else
				x = (x <= 0.92) ? -2 * x + 1.84 : x / 0.08 - 11.5;
			break;
		case 33:
			x = fabs(2 * x - 0.5);
			break;
		case 34:
			x = 2 * x;
			break;
		case 35:
			x = 2 * x - 0.5;
			break;
		case 36:
			x = 2 * x - 1;
			break;

		default:
			g_warning("Fatal: undefined color formula\n");
			break;
	}
	if (x <= 0)
		return 0;
	if (x >= 1)
		return 1;
	return x;
}



static void xyplot_export_pdf(GtkWidget *w, XYPlot *p)
{
	GtkWidget *dia;
	GtkFileChooser *chooser;
	gint res;

	GtkWidget *win;

	cairo_t *cr;
	cairo_surface_t *cs;

	gchar *fname;



	win = gtk_widget_get_toplevel(GTK_WIDGET(w));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	dia = gtk_file_chooser_dialog_new("Export PDF",
					  GTK_WINDOW(win),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  "_Cancel",
					  GTK_RESPONSE_CANCEL,
					  "_Save",
					  GTK_RESPONSE_ACCEPT,
					  NULL);

	chooser = GTK_FILE_CHOOSER(dia);

	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

	gtk_file_chooser_set_current_name(chooser, "plot.pdf");


	res = gtk_dialog_run(GTK_DIALOG(dia));

	g_free(fname);

	if (res == GTK_RESPONSE_ACCEPT) {

		fname = gtk_file_chooser_get_filename(chooser);
		cs = cairo_pdf_surface_create(fname, 1280, 720);
		cr = cairo_create(cs);

		xyplot_plot_render(p, cr, 1280, 720);

		cairo_destroy(cr);
		cairo_surface_destroy(cs);

		g_free(fname);
	}

	gtk_widget_destroy(dia);
}


static void xyplot_drop_graph_cb(GtkWidget *w, struct graph *g)
{
	xyplot_drop_graph(GTK_WIDGET(g->parent), g);
}


static void xyplot_import_graph_xy_asc(const gchar *fname, XYPlot *p)
{
	FILE *f;

	gdouble a[3];

	GArray *gx, *gy, *gc;

	gint n;

	gchar *t;
	gchar line[1024];



	f = g_fopen(fname, "r");

	if (!f) {
		g_warning("%s: error opening file %s", __func__, fname);
		return;
	}

	gx = g_array_new(FALSE, FALSE, sizeof(gdouble));
	gy = g_array_new(FALSE, FALSE, sizeof(gdouble));
	gc = g_array_new(FALSE, FALSE, sizeof(gdouble));



	while (fgets(line, 1024, f)) {

		t = strtok(line, "\n\t ");

		n = 0;
		while (t) {

			if (t[0] == '\n') {

				/* XXX refactor */
				n = gx->len;

				if (gc->len) {
					if (n > gc->len) {
						g_warning("Mixed XY and XYZ data. Will stupidly clamp "
							  "number of input samples to %d with "
							  "unpredictable results. Have fun.", gc->len);
						n = gc->len;
					}
				}

				xyplot_add_graph(GTK_WIDGET(p),
						 (gdouble *) gx->data,
						 (gdouble *) gy->data,
						 (gdouble *) gc->data,
						 n, g_strdup(fname));

				xyplot_redraw(GTK_WIDGET(p));

				g_array_free(gx, FALSE);
				g_array_free(gy, FALSE);
				g_array_free(gc, FALSE);

				gx = g_array_new(FALSE, FALSE, sizeof(gdouble));
				gy = g_array_new(FALSE, FALSE, sizeof(gdouble));
				gc = g_array_new(FALSE, FALSE, sizeof(gdouble));
			}

			if (t[0] == '#')
				break; /* comment, go to next line */

			a[n++] = g_ascii_strtod(t, NULL);

			if (n > 2)
				break;

			t = strtok(NULL, "\t ");
		}

		if (n == 3) {
			g_array_append_val(gx, a[0]);
			g_array_append_val(gy, a[1]);
			g_array_append_val(gc, a[2]);
		} else if (n == 2) {
			g_array_append_val(gx, a[0]);
			g_array_append_val(gy, a[1]);
		}
	}

	fclose(f);


	n = gx->len;

	if (gc->len) {
		if (n > gc->len) {
			g_warning("Mixed XY and XYZ data. Will stupidly clamp "
				  "number of input samples to %d with "
				  "unpredictable results. Have fun.", gc->len);
			n = gc->len;
		}
	}

	xyplot_add_graph(GTK_WIDGET(p),
			 (gdouble *) gx->data,
			 (gdouble *) gy->data,
			 (gdouble *) gc->data,
			 n, g_strdup(fname));

	xyplot_redraw(GTK_WIDGET(p));

	g_array_free(gx, FALSE);
	g_array_free(gy, FALSE);
	g_array_free(gc, FALSE);
}

static void xyplot_export_graph_xy_asc(const gchar *fname, struct graph *g,
				       const gchar *mode)
{
	FILE *f;

	size_t i;


	f = g_fopen(fname, mode);

	if (!f) {
		g_message("%s: error opening file %s", __func__, fname);
		return;
	}

	/* append mode: add empty newline to indicate new dataset */
	if (mode[0] == 'a')
		fprintf(f, "\n");

	if (!g->data_c) {
		fprintf(f, "#\t%s\t%s\n", g->parent->xlabel, g->parent->ylabel);

		for (i = 0; i < g->data_len; i++)
			fprintf(f, "\t%f\t%f\n", g->data_x[i], g->data_y[i]);
	} else {

		fprintf(f, "#\t%s\t%s\tz-axis\n",
			g->parent->xlabel,
			g->parent->ylabel);

		for (i = 0; i < g->data_len; i++) {
			fprintf(f, "\t%f\t%f\t%f\n",
				g->data_x[i],
				g->data_y[i],
				g->data_c[i]);
		}

	}


	fclose(f);
}


static void xyplot_export_xy_graph_cb(GtkWidget *w, struct graph *g)
{
	GtkWidget *dia;
	GtkFileChooser *chooser;
	gint res;

	GtkWidget *win;

	gchar *fname;


	win = gtk_widget_get_toplevel(GTK_WIDGET(w));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	g_ref_count_inc(&g->ref);

	if (!strlen(g->label))
		fname = g_strdup_printf("xydata.dat");
	else
		fname = g_strdup_printf("%s.dat", g->label);

	dia = gtk_file_chooser_dialog_new("Export Data",
					  GTK_WINDOW(win),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  "_Cancel",
					  GTK_RESPONSE_CANCEL,
					  "_Save",
					  GTK_RESPONSE_ACCEPT,
					  NULL);

	chooser = GTK_FILE_CHOOSER(dia);

	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

	gtk_file_chooser_set_current_name(chooser, fname);


	res = gtk_dialog_run(GTK_DIALOG(dia));

	g_free(fname);

	if (res == GTK_RESPONSE_ACCEPT) {

		fname = gtk_file_chooser_get_filename(chooser);


		xyplot_export_graph_xy_asc(fname, g, "w");

		g_free(fname);
	}

	gtk_widget_destroy(dia);

	g_ref_count_dec(&g->ref);
}


static void xyplot_export_xy_plot_cb(GtkWidget *w, XYPlot *p)
{
	GtkWidget *dia;
	GtkFileChooser *chooser;
	gint res;

	GtkWidget *win;

	gchar *fname;

	GList *elem;

	struct graph *g;


	win = gtk_widget_get_toplevel(GTK_WIDGET(w));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}


	fname = g_strdup_printf("xyzdata.dat");

	dia = gtk_file_chooser_dialog_new("Export XY Data",
					  GTK_WINDOW(win),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  "_Cancel",
					  GTK_RESPONSE_CANCEL,
					  "_Save",
					  GTK_RESPONSE_ACCEPT,
					  NULL);

	chooser = GTK_FILE_CHOOSER(dia);

	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

	gtk_file_chooser_set_current_name(chooser, fname);


	res = gtk_dialog_run(GTK_DIALOG(dia));

	g_free(fname);

	if (res == GTK_RESPONSE_ACCEPT) {

		fname = gtk_file_chooser_get_filename(chooser);

		/* clear file */
		fclose(g_fopen(fname, "w"));

		for (elem = p->graphs; elem; elem = elem->next) {
			g = (struct graph *) elem->data;
			xyplot_export_graph_xy_asc(fname, g, "a");
		}

		g_free(fname);
	}

	gtk_widget_destroy(dia);
}



static void xyplot_import_xy_graph_cb(GtkWidget *w, XYPlot *p)
{
	GtkWidget *dia;
	GtkFileChooser *chooser;
	gint res;

	GtkWidget *win;

	gchar *fname = NULL;


	win = gtk_widget_get_toplevel(GTK_WIDGET(w));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	dia = gtk_file_chooser_dialog_new("Import Data",
					  GTK_WINDOW(win),
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  "_Cancel",
					  GTK_RESPONSE_CANCEL,
					  "_Open",
					  GTK_RESPONSE_ACCEPT,
					  NULL);

	chooser = GTK_FILE_CHOOSER(dia);

	res = gtk_dialog_run(GTK_DIALOG(dia));


	if (res == GTK_RESPONSE_ACCEPT) {

		fname = gtk_file_chooser_get_filename(chooser);

		xyplot_import_graph_xy_asc(fname, p);

		g_free(fname);
	}

	gtk_widget_destroy(dia);
}


static void xyplot_drop_all_graphs_cb(GtkWidget *w, XYPlot *p)
{
	xyplot_drop_all_graphs(GTK_WIDGET(p));
}


static void xyplot_rubberband_minmax_order(XYPlot *p)
{
	gdouble tmp;

	if (p->rub.px0 > p->rub.px1) {
		tmp = p->rub.px0;
		p->rub.px0 = p->rub.px1;
		p->rub.px1 = tmp;
	}

	if (p->rub.py0 > p->rub.py1) {
		tmp = p->rub.py0;
		p->rub.py0 = p->rub.py1;
		p->rub.py1 = tmp;
	}
}


static void xyplot_autorange_cb(GtkWidget *w, XYPlot *p)
{
	p->autorange_x = TRUE;
	p->autorange_y = TRUE;

	p->rub.px0 = 0.0;
	p->rub.px1 = 0.0;
	p->rub.py0 = 0.0;
	p->rub.py1 = 0.0;

	xyplot_auto_range(p);
	xyplot_auto_axes(p);

	xyplot_plot(p);
}


static void xyplot_clear_selection_cb(GtkWidget *w, XYPlot *p)
{
	p->sel.xmin = 0.0;
	p->sel.xmax = 0.0;
	p->sel.ymin = 0.0;
	p->sel.ymax = 0.0;

	p->sel.active = FALSE;
	/* XXX to clear fit */
	g_signal_emit_by_name(p, "xyplot-fit-selection");

	xyplot_plot(p);
}


static void xyplot_col_resp_cb(GtkDialog *dia, gint resp_id, struct graph *g)
{
	g_ref_count_inc(&g->ref);

	if (resp_id == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dia), &g->colour);

		xyplot_plot(XYPLOT(g->parent));
	}

	gtk_widget_destroy(GTK_WIDGET(dia));

	g_ref_count_dec(&g->ref);
}


static void xyplot_choose_colour_cb(GtkWidget *w, struct graph *g)
{
	GtkWidget *dia;
	GtkWidget *win;


	win = gtk_widget_get_toplevel(GTK_WIDGET(g->parent));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	g_ref_count_inc(&g->ref);

	dia = gtk_color_chooser_dialog_new("Choose Colour", GTK_WINDOW(win));

	gtk_window_set_modal(GTK_WINDOW(dia), TRUE);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dia), &g->colour);

	g_signal_connect(dia, "response", G_CALLBACK(xyplot_col_resp_cb), g);

	gtk_widget_show_all(dia);

	g_ref_count_dec(&g->ref);
}



/* XXX meh...this is so ridiculously redundant; need to think of a better
 * solution, like force a redraw on the drawing area; atm I don't see how we can
 * get there at the moment
 */

static void xyplot_col_bg_resp_cb(GtkDialog *dia, gint resp_id, XYPlot *p)
{
	if (resp_id == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dia), &p->bg_colour);

		xyplot_plot(p);
	}

	gtk_widget_destroy(GTK_WIDGET(dia));
}


static void xyplot_choose_plot_bgcolour_cb(GtkWidget *w, XYPlot *p)
{
	GtkWidget *dia;
	GtkWidget *win;


	win = gtk_widget_get_toplevel(GTK_WIDGET(p));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	dia = gtk_color_chooser_dialog_new("Choose Colour", GTK_WINDOW(win));

	gtk_window_set_modal(GTK_WINDOW(dia), TRUE);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dia), &p->bg_colour);
	g_signal_connect(dia, "response",
			 G_CALLBACK(xyplot_col_bg_resp_cb), p);

	gtk_widget_show_all(dia);
}


static void xyplot_col_ax_resp_cb(GtkDialog *dia, gint resp_id, XYPlot *p)
{
	if (resp_id == GTK_RESPONSE_OK) {
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dia), &p->ax_colour);

		xyplot_plot(p);
	}

	gtk_widget_destroy(GTK_WIDGET(dia));
}


static void xyplot_choose_plot_axcolour_cb(GtkWidget *w, XYPlot *p)
{
	GtkWidget *dia;
	GtkWidget *win;


	win = gtk_widget_get_toplevel(GTK_WIDGET(p));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	dia = gtk_color_chooser_dialog_new("Choose Colour", GTK_WINDOW(win));

	gtk_window_set_modal(GTK_WINDOW(dia), TRUE);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dia), &p->ax_colour);
	g_signal_connect(dia, "response",
			 G_CALLBACK(xyplot_col_ax_resp_cb), p);

	gtk_widget_show_all(dia);
}


static void xyplot_radio_menu_toggled_cb(GtkWidget *w, struct graph *g)
{
	gchar *lbl;

	if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
		return;

	g_ref_count_inc(&g->ref);

	g_object_get(G_OBJECT(w), "label", &lbl, NULL);

	if (!g_strcmp0(lbl, "Stairs"))
		g->style = STAIRS;
	else if (!g_strcmp0(lbl, "Circles"))
		g->style = CIRCLES;
	else if (!g_strcmp0(lbl, "Lines"))
		g->style = LINES;
	else if (!g_strcmp0(lbl, "NaN Lines"))
		g->style = NAN_LINES;
	else if (!g_strcmp0(lbl, "Bézier"))
		g->style = CURVES;
	else if (!g_strcmp0(lbl, "Dashes"))
		g->style = DASHES;
	else if (!g_strcmp0(lbl, "Squares"))
		g->style = SQUARES;


	g_free(lbl);

	xyplot_plot(g->parent);

	g_ref_count_dec(&g->ref);
}


static GtkWidget *xyplot_create_graph_style_menu(struct graph *g)
{
	GtkWidget *w;
	GtkWidget *sub;

	GSList *grp;

	sub = gtk_menu_new();


	w = gtk_radio_menu_item_new_with_label(NULL, "Stairs");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);
	if (g->style == STAIRS)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);

	grp = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(w));

	w = gtk_radio_menu_item_new_with_label(grp, "Circles");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	if (g->style == CIRCLES)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);

	w = gtk_radio_menu_item_new_with_label(grp, "Lines");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);
	if (g->style == LINES)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);

	w = gtk_radio_menu_item_new_with_label(grp, "NaN Lines");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);
	if (g->style == NAN_LINES)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);

	w = gtk_radio_menu_item_new_with_label(grp, "Bézier");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);
	if (g->style == CURVES)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);

	w = gtk_radio_menu_item_new_with_label(grp, "Dashes");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);
	if (g->style == DASHES)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);


	w = gtk_radio_menu_item_new_with_label(grp, "Squares");
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	g_signal_connect(w, "toggled",
			 G_CALLBACK(xyplot_radio_menu_toggled_cb), g);
	if (g->style == SQUARES)
		g_object_set(G_OBJECT(w), "active", TRUE, NULL);



	return sub;
}



static GtkWidget *xyplot_create_graph_menu(struct graph *g)
{
	GtkWidget *w;
	GtkWidget *sub;

	sub = gtk_menu_new();

	w = gtk_menu_item_new_with_label("Style");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w),
				  xyplot_create_graph_style_menu(g));
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);


	w = gtk_menu_item_new_with_label("Colour");
	g_signal_connect(w, "activate", G_CALLBACK(xyplot_choose_colour_cb), g);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);

	w = gtk_menu_item_new_with_label("Drop Graph");
	g_signal_connect(w, "activate", G_CALLBACK(xyplot_drop_graph_cb), g);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);

	w = gtk_menu_item_new_with_label("Export Data");
	g_signal_connect(w, "activate",
			 G_CALLBACK(xyplot_export_xy_graph_cb), g);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);

	return sub;
}


static void xyplot_popup_menu_add_graphs(XYPlot *p)
{
	GtkWidget *w;
	GtkWidget *sub;

	GList *l;

	struct graph *g;


	w = gtk_menu_item_new_with_label("Graphs");
	gtk_menu_shell_append(GTK_MENU_SHELL(p->menu), w);
	gtk_widget_show(w);

	sub = gtk_menu_new();

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), sub);


	for (l = p->graphs; l; l = l->next) {

		g = (struct graph *) l->data;

		w = gtk_menu_item_new_with_label(g->label);
		gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);

		gtk_widget_show(w);

		gtk_menu_item_set_submenu(GTK_MENU_ITEM(w),
					  xyplot_create_graph_menu(g));
	}
}


static void xyplot_popup_menu_add_plot_cfg(XYPlot *p)
{
	GtkWidget *w;
	GtkWidget *sub;


	w = gtk_menu_item_new_with_label("Plot");
	gtk_menu_shell_append(GTK_MENU_SHELL(p->menu), w);
	gtk_widget_show(w);

	sub = gtk_menu_new();

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(w), sub);

	w = gtk_menu_item_new_with_label("Autorange");
	g_signal_connect(w, "activate",
			 G_CALLBACK(xyplot_autorange_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);


	w = gtk_menu_item_new_with_label("Background Colour");
	g_signal_connect(w, "activate",
			 G_CALLBACK(xyplot_choose_plot_bgcolour_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);

	w = gtk_menu_item_new_with_label("Axes Colour");
	g_signal_connect(w, "activate",
			 G_CALLBACK(xyplot_choose_plot_axcolour_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);


	w = gtk_menu_item_new_with_label("Clear Plot");
	g_signal_connect(w, "activate",
			 G_CALLBACK(xyplot_drop_all_graphs_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);

	if (p->sel.active) {
		w = gtk_menu_item_new_with_label("Clear Selection");
		g_signal_connect(w, "activate",
				 G_CALLBACK(xyplot_clear_selection_cb), p);
		gtk_menu_shell_append(GTK_MENU_SHELL(sub), w);
	}
}




/**
 * @brief build the right-click popup menu
 */

static void xyplot_build_popup_menu(XYPlot *p)
{
	GtkWidget *menuitem;


	if (p->menu)
		gtk_widget_destroy(p->menu);

	p->menu = gtk_menu_new();


	xyplot_popup_menu_add_graphs(p);
	xyplot_popup_menu_add_plot_cfg(p);


	menuitem = gtk_menu_item_new_with_label("Import Data");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(xyplot_import_xy_graph_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Export Data");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(xyplot_export_xy_plot_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->menu), menuitem);


	menuitem = gtk_menu_item_new_with_label("Export as PDF");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(xyplot_export_pdf), p);
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

	xyplot_build_popup_menu(p);

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

static void xyplot_draw_bg(XYPlot *p, cairo_t *cr)
{
	cairo_save(cr);

	cairo_set_source_rgba(cr, p->bg_colour.red,  p->bg_colour.green,
				  p->bg_colour.blue, p->bg_colour.alpha);

	cairo_paint(cr);

	cairo_set_source_rgba(cr, p->bg_colour.red,  p->bg_colour.green,
				  p->bg_colour.blue, p->bg_colour.alpha);

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
 * @brief write text vertically centered and left aligned at x/y coordinate
 *
 * @param cr the cairo context to draw on
 * @param x the x coordinate
 * @param y the y coordinate
 * @param buf a pointer to the text buffer
 */

static void xyplot_write_text_lalign(cairo_t *cr,
				     const double x, const double y,
				     const char *buf)
{
	cairo_text_extents_t te;


	cairo_save(cr);

	cairo_text_extents(cr, buf, &te);

	/* translate origin to center of text location */
	cairo_translate(cr, x, y);

	/* translate origin so text will be centered */
	cairo_translate(cr, 0.0, te.height * 0.5);

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

static void xyplot_update_plot_size(XYPlot *p,
				    const double x, const double y,
				    const double w, const double h)
{
	p->plot_x = x;
	p->plot_y = y;

	p->plot_w = w;
	p->plot_h = h;

	p->scale_x =  w / p->x_ax.len;
	p->scale_y =  h / p->y_ax.len;
}


/**
 * @brief draw the outer plot frame and the axis labels
 */

static void xyplot_draw_plot_frame(XYPlot *p, cairo_t *cr,
				   const double width, const double height)
{
	double x, y;
	double w, h;

	cairo_text_extents_t te_x;
	cairo_text_extents_t te_y;
	cairo_text_extents_t te_x2;
	cairo_text_extents_t te_y2;
	cairo_text_extents_t te_title;

	cairo_save(cr);

	/* get extents of the x/y axis labels */
	cairo_text_extents(cr, p->xlabel, &te_x);
	cairo_text_extents(cr, p->ylabel, &te_y);

	/* get extents of the x2/y2 axis labels */
	cairo_text_extents(cr, p->x2label, &te_x2);
	cairo_text_extents(cr, p->y2label, &te_y2);

	/* get extents of the plot title */
	cairo_text_extents(cr, p->title, &te_title);



	/* start of the plot frame; add extra space for y label */
	x = p->pad + 4.0 * te_y.height;
	y = p->pad + 4.0 * te_x2.height + 4.0 * te_title.height;

	/* size of frame; subtract paddings and extra text space */
	w = width  - 2.0 * (p->pad + 2.0 * (te_y.height + te_y2.height));
	h = height - 2.0 * (p->pad + 2.0 * (te_x.height + te_x2.height
					    + te_title.height));


	xyplot_update_plot_size(p, x, y, w, h);


	cairo_set_line_width(cr, 2.0);

	/* draw the frame */
	cairo_rectangle(cr, x, y, w , h);

	/* place both labels twice their extent height from the frame, so
	 * there is sufficient space to render the tick labels
	 */

	/* draw the x-axis label */
	if (p->xlabel)
		xyplot_write_text_centered(cr,
					   x + 0.5 * w ,
					   y + h + 4.0 * te_x.height,
					   p->xlabel,
					   0.0);

	/* draw the y-axis label */
	if (p->ylabel)
		xyplot_write_text_centered(cr,
					   te_y.height,
					   x + 0.5 * h - 0.25 * te_y.width,
					   p->ylabel,
					   -90.0 * M_PI / 180.0);

	/* draw the x2-axis label */
	if (p->x2label)
		xyplot_write_text_centered(cr,
					   x + 0.5 * w ,
					   y - 4.0 * te_x2.height,
					   p->x2label,
					   0.0);

	/* draw the y2-axis label */
	if (p->y2label)
		xyplot_write_text_centered(cr,
					   width - 2.0 * te_y2.height,
					   x + 0.5 * h - 0.25 * te_y2.width,
					   p->y2label,
					   90.0 * M_PI / 180.0);

	cairo_stroke(cr);

	/* draw the plot title */
	if (p->title) {

		cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0);

		xyplot_write_text_centered(cr,
					   x + 0.5 * w,
					   2.0 * te_title.height,
					   p->title,
					   0.0);

		cairo_stroke(cr);
	}



	cairo_restore(cr);
}


/**
 * @brief modify cairo context so coordinate origin is in lower left
 *	  corner of plot frame
 */

static void xyplot_transform_origin(XYPlot *p, cairo_t *cr)
{
	cairo_matrix_t matrix;


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

static void xyplot_draw_ticks_x(XYPlot *p, cairo_t *cr,
				const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;

	const gdouble lw = 3.0;


	cairo_save(cr);

	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
				  p->ax_colour.blue, p->ax_colour.alpha);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, lw);

	xyplot_transform_origin(p, cr);

	idx = p->x_ax.tick_min + p->x_ax.step;
	inc = p->x_ax.step;
	stp = p->x_ax.max;
	min = p->x_ax.min;
	scl = p->scale_x;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, (idx - min) * scl, 0.0);
		cairo_rel_line_to(cr, 0.0, 10.);

		if ((idx + 0.5 * inc) < stp) {
			cairo_move_to(cr, (idx + 0.5 * inc - min) * scl, 0.0);
			cairo_rel_line_to(cr, 0.0, 5.);
		}
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}

/**
 * @brief draw tick marks on x2-axis
 *
 * @param cr the cairo context to draw on
 * @param width the area width
 * @param height the area height
 *
 */

static void xyplot_draw_ticks_x2(XYPlot *p, cairo_t *cr,
				 const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double y;

	const gdouble lw = 3.0;


	if (!p->conv_to_x2)
		return;


	cairo_save(cr);

	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
				  p->ax_colour.blue, p->ax_colour.alpha);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, lw);

	xyplot_transform_origin(p, cr);

	idx = p->x_ax.tick_min + p->x_ax.step;
	inc = p->x_ax.step;
	stp = p->x_ax.max;
	min = p->x_ax.min;
	scl = p->scale_x;
	y   = (p->y_ax.max - p->y_ax.min) * p->scale_y;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, (idx - min) * scl, y);
		cairo_rel_line_to(cr, 0.0, -10.);

		if ((idx + 0.5 * inc) < stp) {
			cairo_move_to(cr, (idx + 0.5 * inc - min) * scl,
				      y);
			cairo_rel_line_to(cr, 0.0, -5.);
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

static void xyplot_draw_ticks_y(XYPlot *p, cairo_t *cr,
				const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;

	const gdouble lw = 3.0;

	cairo_save(cr);

	/* background color */
	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
				  p->ax_colour.blue, p->ax_colour.alpha);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, lw);

	xyplot_transform_origin(p, cr);

	/* horizontal grid lines */
	idx = p->y_ax.tick_min + p->y_ax.step;
	inc = p->y_ax.step;
	stp = p->y_ax.max;
	min = p->y_ax.min;
	scl = p->scale_y;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, 0.0, (idx - min) * scl);
		cairo_rel_line_to(cr, 10.0, 0.0);

		if ((idx + 0.5 * inc) < stp) {
			cairo_move_to(cr, 0.0, (idx + 0.5 * inc - min) * scl);
			cairo_rel_line_to(cr, 5.0, 0.0);
		}
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw tick marks on y2-axis
 *
 * @param cr the cairo context to draw on
 * @param width the area width
 * @param height the area height
 *
 */

static void xyplot_draw_ticks_y2(XYPlot *p, cairo_t *cr,
				 const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double x;

	const gdouble lw = 3.0;


	if (!p->conv_to_y2)
		return;

	cairo_save(cr);

	/* background color */
	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
				  p->ax_colour.blue, p->ax_colour.alpha);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, lw);

	xyplot_transform_origin(p, cr);

	/* horizontal grid lines */
	idx = p->y_ax.tick_min + p->y_ax.step;
	inc = p->y_ax.step;
	stp = p->y_ax.max;
	min = p->y_ax.min;
	scl = p->scale_y;
	x   = (p->x_ax.max - p->x_ax.min) * p->scale_x;

	for ( ; idx < stp; idx += inc) {
		cairo_move_to(cr, x, (idx - min) * scl);
		cairo_rel_line_to(cr, -10.0, 0.0);

		if ((idx + 0.5 * inc) < stp) {
			cairo_move_to(cr, x, (idx + 0.5 * inc - min) * scl);
			cairo_rel_line_to(cr, -5.0, 0.0);
		}
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw labels to x-axis ticks
 */

static void xyplot_draw_tickslabels_x(XYPlot *p, cairo_t *cr,
				      const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double off;

	char buf[14];	/* should be large enough */

	cairo_text_extents_t te;


	cairo_save(cr);

	/* translate to lower left corner */
	cairo_translate(cr, p->plot_x, p->plot_y + p->plot_h);

	/* get text extents to determine the height of the current font */
	cairo_text_extents(cr, "0", &te);

	/* horizontal ticks */
	idx = p->x_ax.tick_min + p->x_ax.step;
	inc = p->x_ax.step;
	stp = p->x_ax.max;
	min = p->x_ax.min;
	scl = p->scale_x;
	off = 1.5 * te.height;

	if (min < p->x_ax.min)
		min -= inc;

	for ( ; idx < stp; idx += inc) {
		snprintf(buf, ARRAY_SIZE(buf), "%.6g", idx);
		xyplot_write_text_centered(cr, (idx - min) * scl,
					   off, buf, 0.0);
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw labels to x2-axis ticks
 */

static void xyplot_draw_tickslabels_x2(XYPlot *p, cairo_t *cr,
				       const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double off;
	double x2;

	char buf[14];	/* should be large enough */

	cairo_text_extents_t te;


	if (!p->conv_to_x2)
		return;

	cairo_save(cr);

	/* translate to upper right corner */
	cairo_translate(cr, p->plot_x, p->plot_y);

	/* get text extents to determine the height of the current font */
	cairo_text_extents(cr, "0", &te);

	/* horizontal ticks */
	idx = p->x_ax.tick_min + p->x_ax.step;
	inc = p->x_ax.step;
	stp = p->x_ax.max;
	min = p->x_ax.min;
	scl = p->scale_x;
	off = -1.5 * te.height;

	if (min < p->x_ax.min)
		min -= inc;

	for ( ; idx < stp; idx += inc) {
		x2 = p->conv_to_x2(idx, p->x2_userdata);
		snprintf(buf, ARRAY_SIZE(buf), "%4g", x2);
		xyplot_write_text_centered(cr, (idx - min) * scl,
					   off, buf, 0.0);
	}

	cairo_stroke(cr);

	cairo_restore(cr);
}



/**
 * @brief draw labels to y-axis ticks
 */

static void xyplot_draw_tickslabels_y(XYPlot *p, cairo_t *cr,
				      const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;

	char buf[14];	/* should be large enough */

	cairo_text_extents_t te;


	cairo_save(cr);

	/* translate to lower left corner */
	cairo_translate(cr, p->plot_x, p->plot_y + p->plot_h);

	/* get text extents to determine the width of a character of the current
	 * font and use it as offset from the y-axis
	 */
	cairo_text_extents(cr, "0", &te);

	/* vertical ticks */
	idx = p->y_ax.tick_min + p->y_ax.step;
	inc = p->y_ax.step;
	stp = p->y_ax.max;
	min = p->y_ax.min;
	scl = p->scale_y;

	for ( ; idx < stp; idx += inc) {
		snprintf(buf, ARRAY_SIZE(buf), "%.6g", idx);
		xyplot_write_text_ralign(cr, -te.width, (min - idx) * scl, buf);
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw labels to y2-axis ticks
 */

static void xyplot_draw_tickslabels_y2(XYPlot *p, cairo_t *cr,
				       const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double y2;

	char buf[14];	/* should be large enough */

	cairo_text_extents_t te;


	if (!p->conv_to_y2)
		return;

	cairo_save(cr);

	/* translate to lower right corner */
	cairo_translate(cr, p->plot_x + p->plot_w, p->plot_y + p->plot_h);

	/* get text extents to determine the width of a character of the current
	 * font and use it as offset from the y-axis
	 */
	cairo_text_extents(cr, "0", &te);

	/* vertical ticks */
	idx = p->y_ax.tick_min + p->y_ax.step;
	inc = p->y_ax.step;
	stp = p->y_ax.max;
	min = p->y_ax.min;
	scl = p->scale_y;

	for ( ; idx < stp; idx += inc) {
		y2 = p->conv_to_y2(idx, p->y2_userdata);
		snprintf(buf, ARRAY_SIZE(buf), "%.6g", y2);
		xyplot_write_text_lalign(cr, te.width, (min - idx) * scl, buf);
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

static void xyplot_draw_grid_y(XYPlot *p, cairo_t *cr,
			       const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double end;

	const double dashes[] = {2.0, 2.0};


	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	/* background color */
	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
				  p->ax_colour.blue, p->ax_colour.alpha);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, 1.0);
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);

	/* vertical grid lines */
	idx = p->x_ax.tick_min + p->x_ax.step;
	inc = p->x_ax.step;
	stp = p->x_ax.max;
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

static void xyplot_draw_grid_x(XYPlot *p, cairo_t *cr,
			       const double width, const double height)
{
	double idx;
	double stp;
	double inc;
	double min;
	double scl;
	double end;

	const double dashes[] = {2.0, 2.0};


	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	/* background color */
	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
				  p->ax_colour.blue, p->ax_colour.alpha);

	/* disable antialiasing for sharper line */
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	cairo_set_line_width(cr, 1.0);
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);

	/* horizontal grid lines */
	idx = p->y_ax.tick_min + p->y_ax.step;
	inc = p->y_ax.step;
	stp = p->y_ax.max;
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
 *
 * XXX the relative line drawing method runs into trouble
 * when data is very large (~1Megapoints); to fix this, we should
 * do intermediate stroke() and move_to() to split the path into smaller
 * chunks; the same applies to draw_lines
 */

static void xyplot_draw_stairs(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;


	/* can't connect a single point by a line */
	if (g->data_len < 2)
		return;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;

	cairo_save(cr);


	xyplot_transform_origin(p, cr);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);
	cairo_set_line_width(cr, 2.0);

	cairo_move_to(cr,
		      (x[0] - (x[1] - x[0]) * 0.5 -  p->x_ax.min) * sx,
		      0.0);

	cairo_rel_line_to(cr, 0.0, (y[0] - p->y_ax.min) * sy);
	cairo_rel_line_to(cr, (x[1] - x[0]) * sx, 0.0);

	for (i = 1; i < g->data_len - 1; i++) {
		cairo_rel_line_to(cr, 0.0, (y[i] - y[i - 1]) * sy);
		cairo_line_to(cr, (0.5 * (x[i] + x[i + 1])- p->x_ax.min) * sx,
			          (y[i] - p->y_ax.min) * sy);
	}

	cairo_rel_line_to(cr, 0.0, (y[i] - y[i - 1]) * sy);

	cairo_line_to(cr, (x[i] + (x[i] - x[i -1]) * 0.5  - p->x_ax.min) * sx,
		          (y[i] - p->y_ax.min) * sy);

	cairo_rel_line_to(cr, 0.0, - (y[i] - p->y_ax.min) * sy);

	cairo_stroke(cr);

	cairo_restore(cr);
}




/**
 * @brief create a circle as source surface
 */

static cairo_surface_t *xyplot_create_circle_surface(cairo_t *cr,
						     struct graph *g,
						     gdouble radius,
						     gboolean fill)
{
	double w, h;
	double xc, yc;

	const double lw = 1.0;	/* fixed for now */

	cairo_t		*ct;
	cairo_surface_t *cs;



	xc = radius;
	yc = radius;
	w  = 2.0 * radius;
	h  = 2.0 * radius;


	cs = cairo_surface_create_similar(cairo_get_target(cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  w, h);
	ct = cairo_create(cs);
	cairo_set_source_rgba(ct, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);

	if (!fill) {
		cairo_set_line_width(cr, lw);
		radius -= lw;
	}

	cairo_arc(ct, xc, yc, radius, 0.0, 2.0 * M_PI);

	if (fill)
		cairo_fill(ct);
	else
		cairo_stroke(ct);


	cairo_destroy(ct);

	return cs;
}


/**
 * @brief create a fileld circle as source surface
 */

static cairo_surface_t *xyplot_create_square_surface(cairo_t *cr,
						     struct graph *g,
						     gdouble size)
{
	cairo_t		*ct;
	cairo_surface_t *cs;


	cs = cairo_surface_create_similar(cairo_get_target(cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  size, size);
	ct = cairo_create(cs);
	cairo_set_source_rgba(ct, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);


	cairo_rectangle(ct, 0.0, 0.0, size, size);
	cairo_fill(ct);

	cairo_destroy(ct);

	return cs;
}


/**
 * @brief use PNG image as custom source surface
 */
__attribute__((unused))
static cairo_surface_t *xyplot_create_png_surface(cairo_t *cr,
						    struct graph *g,
						    gdouble scale)
{
	double w, h;

	cairo_t *ct;
	cairo_surface_t *cs;
	cairo_surface_t *ci;
	cairo_pattern_t *cp;
	cairo_matrix_t m;


	ci = cairo_image_surface_create_from_png("/tmp/mario.png");

	cp = cairo_pattern_create_for_surface(ci);
	cairo_pattern_set_filter(cp, CAIRO_FILTER_NEAREST);

	w = scale * (double) cairo_image_surface_get_width(ci);
	h = scale * (double) cairo_image_surface_get_height(ci);

	cs = cairo_surface_create_similar(cairo_get_target(cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  w, h);


	ct = cairo_create(cs);

	/* flip the matrix, plot surface is upside down */
	cairo_get_matrix(ct, &m);
	cairo_matrix_translate(&m, 0.0, h);
	cairo_matrix_scale(&m, 1.0, -1.0);
	cairo_set_matrix(ct, &m);


	cairo_scale(ct, scale, scale);

	cairo_set_source(ct, cp);

	cairo_paint(ct);

	cairo_destroy(ct);
	cairo_pattern_destroy(cp);
	cairo_surface_destroy(ci);

	return cs;
}


/**
 * @brief use Super Mario as source surface
 */

static cairo_surface_t *xyplot_create_mario_surface(cairo_t *cr,
						    struct graph *g,
						    gdouble scale)
{
	cairo_t *ct;
	cairo_surface_t *cs;
	cairo_pattern_t *cp;
	cairo_matrix_t m;

	const GdkRGBA othr = {0.847, 0.000, 0.004, 1.0};
	const GdkRGBA boot = {0.443, 0.388, 0.235, 1.0};
	const GdkRGBA skin = {0.949, 0.816, 0.710, 1.0};
	const GdkRGBA shrt = {0.439, 0.631, 0.996, 1.0};
	const GdkRGBA butn = {0.976, 0.671, 0.004, 1.0};
	const GdkRGBA hair = {0.039, 0.027, 0.000, 1.0};




	cs = cairo_surface_create_similar(cairo_get_target(cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  12.0, 16.0);
	ct = cairo_create(cs);

	/* NOTE: no overdrawing so we can use alpha */

	/* cap */
	cairo_set_source_rgba(ct, othr.red,  othr.green,
				  othr.blue, othr.alpha);

	cairo_rectangle(ct,  4,  0,  5, 1);
	cairo_rectangle(ct,  1,  1,  9, 1);

	cairo_fill(ct);

	/* head skin first */
	cairo_set_source_rgba(ct, skin.red,  skin.green,
				  skin.blue, skin.alpha);

	cairo_rectangle(ct,  3,  2,  1, 1);
	cairo_rectangle(ct,  5,  2,  2, 1);

	cairo_rectangle(ct,  1,  3,  4, 1);
	cairo_rectangle(ct,  5,  3,  3, 1);
	cairo_rectangle(ct,  9,  3,  1, 1);

	cairo_rectangle(ct,  0,  4, 3, 1);
	cairo_rectangle(ct,  4,  4, 3, 1);
	cairo_rectangle(ct,  9,  4, 1, 1);

	cairo_rectangle(ct,  5,  5,  4, 1);

	cairo_rectangle(ct,  2,  6,  7, 1);

	cairo_fill(ct);

	/* now hair */
	cairo_set_source_rgba(ct, hair.red,  hair.green,
				  hair.blue, hair.alpha);

	cairo_rectangle(ct,  4,  2,  1, 1);
	cairo_rectangle(ct,  7,  2,  3, 1);

	cairo_rectangle(ct,  4,  3,  1, 1);
	cairo_rectangle(ct,  8,  3,  1, 1);
	cairo_rectangle(ct, 10,  3,  1, 1);

	cairo_rectangle(ct,  3,  4,  1, 1);
	cairo_rectangle(ct,  7,  4,  2, 1);
	cairo_rectangle(ct, 10,  4,  1, 1);

	cairo_rectangle(ct,  1,  5,  4, 1);
	cairo_rectangle(ct,  9,  5,  2, 1);

	cairo_fill(ct);

	/* blue shirt */
	cairo_set_source_rgba(ct, shrt.red,  shrt.green,
				  shrt.blue, shrt.alpha);

	cairo_rectangle(ct,  2,  7,  2, 1);
	cairo_rectangle(ct,  5,  7,  2, 1);
	cairo_rectangle(ct,  8,  7,  2, 1);

	cairo_rectangle(ct,  1,  8,  3, 1);
	cairo_rectangle(ct,  5,  8,  2, 1);
	cairo_rectangle(ct,  8,  8,  3, 1);

	cairo_rectangle(ct,  0,  9,  4, 1);
	cairo_rectangle(ct,  8,  9,  4, 1);

	cairo_rectangle(ct,  2, 10,  1, 1);
	cairo_rectangle(ct,  9, 10,  1, 1);

	cairo_fill(ct);

	/* hands */
	cairo_set_source_rgba(ct, skin.red,  skin.green,
				  skin.blue, skin.alpha);

	cairo_rectangle(ct,  0, 10,  2, 1);
	cairo_rectangle(ct, 10, 10,  2, 1);

	cairo_rectangle(ct,  0, 11,  3, 1);
	cairo_rectangle(ct,  9, 11,  3, 1);

	cairo_rectangle(ct,  0, 12,  2, 1);
	cairo_rectangle(ct, 10, 12,  2, 1);

	cairo_fill(ct);

	/* overall */
	cairo_set_source_rgba(ct, othr.red,  othr.green,
				  othr.blue, othr.alpha);

	cairo_rectangle(ct,  4,  7,  1, 1);
	cairo_rectangle(ct,  7,  7,  1, 1);

	cairo_rectangle(ct,  4,  8,  1, 1);
	cairo_rectangle(ct,  7,  8,  1, 1);

	cairo_rectangle(ct,  4,  9,  4, 1);

	cairo_rectangle(ct,  3, 10,  1, 1);
	cairo_rectangle(ct,  5, 10,  2, 1);
	cairo_rectangle(ct,  8, 10,  1, 1);

	cairo_rectangle(ct,  3, 11,  6, 1);

	cairo_rectangle(ct,  2, 12,  8, 1);

	cairo_rectangle(ct,  2, 13,  3, 1);
	cairo_rectangle(ct,  7, 13,  3, 1);

	cairo_fill(ct);


	/* buttons */
	cairo_set_source_rgba(ct, butn.red,  butn.green,
				  butn.blue, butn.alpha);

	cairo_rectangle(ct,  4,  10,  1, 1);
	cairo_rectangle(ct,  7,  10,  1, 1);

	cairo_fill(ct);

	/* boots */
	cairo_set_source_rgba(ct, boot.red,  boot.green,
				  boot.blue, boot.alpha);

	cairo_rectangle(ct,  1, 14,  3, 1);
	cairo_rectangle(ct,  8, 14,  3, 1);

	cairo_rectangle(ct,  0, 15,  4, 1);
	cairo_rectangle(ct,  8, 15,  4, 1);

	cairo_fill(ct);



	/* now scale */
	cp = cairo_pattern_create_for_surface(cs);
	cairo_pattern_set_filter(cp, CAIRO_FILTER_NEAREST);

	cairo_surface_destroy(cs);
	cs = cairo_surface_create_similar(cairo_get_target(cr),
					  CAIRO_CONTENT_COLOR_ALPHA,
					  12.0 * scale, 16.0 * scale);

	cairo_destroy(ct);
	ct = cairo_create(cs);

	/* flip the matrix, plot surface is upside down */
	cairo_get_matrix(ct, &m);
	cairo_matrix_translate(&m, 0.0, 16.0 * scale);
	cairo_matrix_scale(&m, 1.0, -1.0);
	cairo_set_matrix(ct, &m);


	cairo_scale(ct, scale, scale);

	cairo_set_source(ct, cp);

	cairo_paint(ct);

	cairo_destroy(ct);
	cairo_pattern_destroy(cp);

	return cs;
}


/**
 * @brief draw data points using a source surface
 */

static void xyplot_draw_from_surface(XYPlot *p, cairo_t *cr, struct graph *g,
				     cairo_surface_t *cs, gdouble w, gdouble h)
{
	size_t i;

	gdouble sx, sy;
	gdouble dx, dy;

	gdouble *x, *y;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;


	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	w = 0.5 * w;
	h = 0.5 * h;

	for (i = 0; i < g->data_len; i++) {

		dx = (x[i] - p->x_ax.min) * sx - w;
		dy = (y[i] - p->y_ax.min) * sy - h;

		/* do not draw points outside of frame */
		if (dx < 0.)
			continue;

		if (dx > p->plot_w)
			continue;

		if (dy > p->plot_h)
			continue;

		if (dy < 0.0)
			continue;

		cairo_set_source_surface (cr, cs, dx, dy);
		cairo_paint (cr);
	}

	cairo_restore(cr);
}


/**
 * @brief draw the plot data as circles
 */

static void xyplot_draw_circles(XYPlot *p, cairo_t *cr, struct graph *g)
{
	cairo_surface_t *cs;


	cs = xyplot_create_circle_surface(cr, g, 4.0, TRUE);

	xyplot_draw_from_surface(p, cr, g, cs, 4.0, 4.0);

	cairo_surface_destroy(cs);
}


/**
 * @brief draw the plot data as squares
 */

static void xyplot_draw_squares(XYPlot *p, cairo_t *cr, struct graph *g)
{
	cairo_surface_t *cs;


	cs = xyplot_create_square_surface(cr, g, 4.0);

	xyplot_draw_from_surface(p, cr, g, cs, 4.0, 4.0);

	cairo_surface_destroy(cs);
}


/**
 * @brief draw the plot data as Mario
 */

static void xyplot_draw_mario(XYPlot *p, cairo_t *cr, struct graph *g)
{
	cairo_surface_t *cs;


	cs = xyplot_create_mario_surface(cr, g, 2.0);

	xyplot_draw_from_surface(p, cr, g, cs, 2. * 12., 2. * 16.);

	cairo_surface_destroy(cs);
}



/**
 * @brief draw the plot data as impulses
 */

static void xyplot_draw_impulses(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;

	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	cairo_set_source_rgba(cr, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);
#if 0
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
#endif
	cairo_set_line_width(cr, 2.0);



	for (i = 0; i < g->data_len; i++) {
		cairo_move_to(cr, (x[i] -  p->x_ax.min) * sx, 0.0);
		cairo_rel_line_to(cr, 0.0, (y[i] - p->y_ax.min) * sy);
		cairo_stroke(cr);

		cairo_arc(cr,
			  (x[i] - p->x_ax.min) * sx,
			  (y[i] - p->y_ax.min) * sy,
			  4.0, 0.0, 2.0 * M_PI);
		cairo_fill(cr);
	}

	cairo_restore(cr);
}



/**
 * @brief draw the plot data as 2d map data
 */

static void xyplot_draw_map(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy, sc;

	gdouble dx, dy, wx, wy;

	gdouble *x, *y, *c;

	gdouble r, cg, b, grey;



	sx = p->scale_x;
	sy = p->scale_y;
	sc = 1.0 / (p->cmax - p->cmin);

	/* use global delta step if no local delta step */

	dx = g->dx;
	if (dx == DBL_MAX)
		dx = p->dx;

	dy = g->dy;
	if (dy == DBL_MAX)
		dy = p->dy;


	wx = sx * dx;
	wy = sy * dy;

	dx = 0.5 * wx;
	dy = 0.5 * wy;


	x  = g->data_x;
	y  = g->data_y;
	c  = g->data_c;

	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	for (i = 0; i < g->data_len; i++) {
		grey = (c[i] - p->cmin) * sc;

		r = get_color_value_from_formula( 7, grey);
		cg = get_color_value_from_formula( 5, grey);
		b = get_color_value_from_formula(15, grey);

		cairo_set_source_rgba(cr, r, cg, b, 0.8);
		cairo_rectangle(cr, (x[i] - p->x_ax.min) * sx - dx,
				    (y[i] - p->y_ax.min) * sy - dy,
				    wx, wy);
		cairo_fill(cr);
		cairo_stroke(cr);
	}

	cairo_restore(cr);
}



/**
 * @brief draw the plot data connected straight dashed lines
 */

static void xyplot_draw_dashes(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;

	const double dashes[] = {10.0, 10.0};

	/* can't connect a single point by a line */
	if (g->data_len < 2)
		return;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;

	cairo_save(cr);

	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);

	xyplot_transform_origin(p, cr);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);
	cairo_set_line_width(cr, 2.0);

	cairo_move_to(cr,
		      (x[0] - p->x_ax.min) * sx,
		      (y[0] - p->y_ax.min) * sy);

	for (i = 1; i < g->data_len; i++) {
		cairo_line_to(cr, (x[i] - p->x_ax.min) * sx,
				  (y[i] - p->y_ax.min) * sy);
	}

	cairo_stroke(cr);
	cairo_restore(cr);
}


/**
 * @brief draw the plot data connected straight lines
 */

static void xyplot_draw_lines(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;


	/* can't connect a single point by a line */
	if (g->data_len < 2)
		return;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;

	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);
	cairo_set_line_width(cr, 2.0);

	cairo_move_to(cr, (x[0] - p->x_ax.min) * sx,
		          (y[0] - p->y_ax.min) * sy);

	for (i = 1; i < g->data_len; i++) {
		cairo_line_to(cr, (x[i] - p->x_ax.min) * sx,
				  (y[i] - p->y_ax.min) * sy);

	}

	cairo_stroke(cr);
	cairo_restore(cr);
}


/**
 * @brief draw the plot data connected straight lines supporting NaN-valued
 *	  segmented sections (i.e. set NaN what is not to be drawn)
 *
 * @note the NaN checks are a lot more expensive, so this is a separate
 *	 function
 */

static void xyplot_draw_nan_lines(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;


	/* can't connect a single point by a line */
	if (g->data_len < 2)
		return;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;

	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);
	cairo_set_line_width(cr, 2.0);

	for (i = 0; i < g->data_len; i++) {
		if (!isnan(x[i]) && !isnan(y[i]))
			break;
	}


	gboolean restart = FALSE;
	cairo_move_to(cr, (x[i] - p->x_ax.min) * sx,
		          (y[i] - p->y_ax.min) * sy);

	for (i = i + 1; i < g->data_len; i++) {
		if (isnan(x[i]) || isnan(y[i])) {
			restart = TRUE;
			continue;
		}

		if (restart) {
			cairo_move_to(cr, (x[i] - p->x_ax.min) * sx,
				          (y[i] - p->y_ax.min) * sy);

			restart = FALSE;

		}


		cairo_line_to(cr, (x[i] - p->x_ax.min) * sx,
				  (y[i] - p->y_ax.min) * sy);

	}

	cairo_stroke(cr);
	cairo_restore(cr);
}



/**
 * @brief draw the plot data connected by bezier splines
 */

static void xyplot_draw_curves(XYPlot *p, cairo_t *cr, struct graph *g)
{
	size_t i;

	gdouble sx, sy;

	gdouble *x, *y;


	/* can't connect a less than three points by a curve */
	if (g->data_len < 3)
		return;

	sx = p->scale_x;
	sy = p->scale_y;

	x  = g->data_x;
	y  = g->data_y;

	cairo_save(cr);

	xyplot_transform_origin(p, cr);

	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	cairo_set_source_rgba(cr, g->colour.red,  g->colour.green,
			          g->colour.blue, g->colour.alpha);
	cairo_set_line_width(cr, 2.0);

	gdouble ax1, ax2, ax3;
	gdouble ay1, ay2, ay3;


	cairo_move_to(cr, (x[0] - p->x_ax.min) * sx,
		          (y[0] - p->y_ax.min) * sy);


	for (i = 1; i < g->data_len - 2 ; i += 2) {

		ax1 = x[i];
		ay1 = y[i];

		ax2 = x[i + 1];
		ay2 = y[i + 1];

		if (i < g->data_len - 3) {
			ax3 = (x[i + 1] + x[i + 2]) * 0.5;
			ay3 = (y[i + 1] + y[i + 2]) * 0.5;
		} else {
			ax3 = x[i + 2];
			ay3 = y[i + 2];
		}

		cairo_curve_to(cr, (ax1 - p->x_ax.min) * sx,
                                   (ay1 - p->y_ax.min) * sy,
				   (ax2 - p->x_ax.min) * sx,
				   (ay2 - p->y_ax.min) * sy,
				   (ax3 - p->x_ax.min) * sx,
				   (ay3 - p->y_ax.min) * sy);
	}

	cairo_stroke(cr);
	cairo_restore(cr);
}



static void xyplot_draw_graphs(XYPlot *p, cairo_t *cr)
{
	GList *elem;

	struct graph *g;


	for (elem = p->graphs; elem; elem = elem->next) {

		g = (struct graph *) elem->data;

		/* draw map data */
		if (g->data_c) {
			xyplot_draw_map(p, cr, g);
			continue;
		}

		switch (g->style) {
		case STAIRS:
			xyplot_draw_stairs(p, cr, g);
			break;
		case CIRCLES:
			xyplot_draw_circles(p, cr, g);
			break;
		case IMPULSES:
			xyplot_draw_impulses(p, cr, g);
			break;
		case LINES:
			xyplot_draw_lines(p, cr, g);
			break;
		case NAN_LINES:
			xyplot_draw_nan_lines(p, cr, g);
			break;
		case CURVES:
			xyplot_draw_curves(p, cr, g);
			break;
		case DASHES:
			xyplot_draw_dashes(p, cr, g);
			break;
		case SQUARES:
			xyplot_draw_squares(p, cr, g);
			break;
		case MARIO:
			xyplot_draw_mario(p, cr, g);
			break;
		default:
			break;
		}
	}
}


static void xyplot_draw_indicators(XYPlot *p, cairo_t *cr)
{
	gdouble sx, sy;
	gdouble px, py;

	const gdouble arr_size = 10.0;

	cairo_matrix_t matrix;


	sx = p->scale_x;
	sy = p->scale_y;

	cairo_save(cr);


	xyplot_transform_origin(p, cr);

	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);


	if (p->ind_x.lbl) {
		px = (p->ind_x.pos - p->x_ax.min) * sx;
		py = p->y_ax.min;

		cairo_move_to(cr, px, py);
		cairo_rel_line_to(cr, arr_size * 0.5, arr_size);
		cairo_rel_line_to(cr, -arr_size, 0.0);
		cairo_rel_line_to(cr, arr_size * 0.5, -arr_size);
		cairo_close_path(cr);
		cairo_fill(cr);
		cairo_stroke(cr);


		/* flip cairo transformation matrix for text */
		cairo_get_matrix(cr, &matrix);
		matrix.yy = - matrix.yy;
		cairo_set_matrix(cr, &matrix);

		xyplot_write_text_centered(cr,  px, -py  - arr_size * 2,
					 p->ind_x.lbl, 0.0);

		/* and back */
		cairo_get_matrix(cr, &matrix);
		matrix.yy = - matrix.yy;
		cairo_set_matrix(cr, &matrix);
	}


	if (p->ind_y.lbl) {
		px = p->x_ax.min;
		py = (p->ind_y.pos - p->y_ax.min) * sy;

		cairo_move_to(cr, px, py);
		cairo_rel_line_to(cr, arr_size, arr_size * 0.5);
		cairo_rel_line_to(cr, 0.0, -arr_size);
		cairo_rel_line_to(cr, -arr_size, arr_size * 0.5);
		cairo_close_path(cr);
		cairo_fill(cr);
		cairo_stroke(cr);


		/* flip cairo transformation matrix for text */
		cairo_get_matrix(cr, &matrix);
		matrix.yy = - matrix.yy;
		cairo_set_matrix(cr, &matrix);

		xyplot_write_text_lalign(cr,  px + arr_size * 2, -py,
					 p->ind_y.lbl);

		/* and back */
		cairo_get_matrix(cr, &matrix);
		matrix.yy = - matrix.yy;
		cairo_set_matrix(cr, &matrix);
	}





	cairo_stroke(cr);
	cairo_restore(cr);
}


/**
 * @brief set the plot title
 */

void xyplot_set_title(GtkWidget *widget, gchar *title)
{
	XYPlot *plot;


	plot = XYPLOT(widget);

	plot->title = title;
	gtk_widget_queue_draw(widget);
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

/*
 * @brief set the label for the x2-Axis
 */

void xyplot_set_x2label(GtkWidget *widget, gchar *label)
{
	XYPlot *plot;


	plot = XYPLOT(widget);

	plot->x2label = label;
	gtk_widget_queue_draw(widget);
}


/**
 * @brief set the label for the Y-Axis
 */

void xyplot_set_y2label(GtkWidget *widget, gchar *label)
{
	XYPlot *plot;


	plot = XYPLOT(widget);

	plot->y2label = label;
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

	if (round) {
		if (f < 1.0)
			nf = 0.5;
		else if (f < 1.5)
			nf = 1.0;
		else if (f < 3.0)
			nf = 2.0;
		else if (f < 7.0)
			nf = 5.0;
		else
			nf = 10.0;
	} else {
		if (f <= 1.0)
			nf = 1.0;
		else if (f < 2.0)
			nf = 2.0;
		else if (f < 5.0)
			nf = 5.0;
		else
			nf = 10.0;
	}

	return nf * pow(10, exp);
}


static void xyplot_auto_axis(XYPlot *p, XYPlotAxis *ax,
			     gdouble min, gdouble max, gdouble len)
{
	ax->min = min - len * 0.1;
	ax->max = max + len * 0.1;

	ax->step = xyplot_nicenum(xyplot_nicenum(len, TRUE) /
				  (ax->ticks_maj - 1.0), TRUE);

	ax->tick_min = floor(ax->min / ax->step) * ax->step;
	ax->tick_max =  ceil(ax->max / ax->step) * ax->step;

	ax->len  = ax->max - ax->min;
	ax->prec = MAX(-floor(log10(ax->step)), 0);
}



/**
 * @brief auto-set the plot axes given the data
 */

static void xyplot_auto_axes(XYPlot *p)
{
	xyplot_auto_axis(p, &p->x_ax, p->xmin, p->xmax, p->xlen);
	xyplot_auto_axis(p, &p->y_ax, p->ymin, p->ymax, p->ylen);
}


/**
 * @brief auto-set the ranges of the supplied data
 *
 */

static void xyplot_auto_range(XYPlot *p)
{
	gdouble tmp;

	GList *elem;

	struct graph *g;


	if (p->autorange_x) {
		p->xmin = DBL_MAX;
		p->xmax = -DBL_MAX;
	}

	if (p->autorange_y) {
		p->ymin = DBL_MAX;
		p->ymax = -DBL_MAX;
	}

	p->cmin = DBL_MAX;
	p->cmax = -DBL_MAX;

	p->dx = DBL_MAX;
	p->dy = DBL_MAX;


	for (elem = p->graphs; elem; elem = elem->next) {

		g = (struct graph *) elem->data;

		if (p->autorange_x) {
			if (g->xmin < p->xmin)
				p->xmin = g->xmin;

			if (g->xmax > p->xmax)
				p->xmax = g->xmax;
		}

		if (p->autorange_y) {
			if (g->ymin < p->ymin)
				p->ymin = g->ymin;

			if (g->ymax > p->ymax)
				p->ymax = g->ymax;
		}

		/* 3rd axis is optional */
		if (!g->data_c)
			continue;

		if (g->cmin < p->cmin)
			p->cmin = g->cmin;

		if (g->cmax > p->cmax)
			p->cmax = g->cmax;

		if (p->autorange_x) {
			if (p->dx > g->dx)
				p->dx = g->dx;
		}

		if (p->autorange_y) {
			if (p->dy > g->dy)
				p->dy = g->dy;
		}
	}

	if (p->autorange_x)
		p->xlen = p->xmax - p->xmin;

	if (p->autorange_y)
		p->ylen = p->ymax - p->ymin;

	p->clen = p->cmax - p->cmin;


	/* set an arbitrary values for a singular data range */
	if (p->autorange_x) {
		if (p->xlen == 0.0) {
			p->xmax += 0.5;
			p->xmin -= 0.5;
			p->xlen  = 1.0;
		}
	}

	if (p->autorange_y) {
		if (p->ylen == 0.0) {
			p->ymax += 0.5;
			p->ymin -= 0.5;
			p->ylen  = 1.0;
		}
	}


	/* XXX cleanup **/

	if (p->dx == DBL_MAX) {
		/* since dx is undefined, none of the samples
		 * have more than a single value for a particular axis, so
		 * grab the first and find a delta
		 * XXX find average delta, not minimum?
		 */
		for (elem = p->graphs; elem; elem = elem->next) {

			g = (struct graph *) elem->data;

			/* need 3rd axis sets only */
			if (!g->data_c)
				continue;

			tmp = g->xmin - p->xmin;
			if (tmp > 0.0)
				if (tmp < p->dx)
					p->dx = tmp;
		}
	}


	if (p->dy == DBL_MAX) {

		for (elem = p->graphs; elem; elem = elem->next) {

			g = (struct graph *) elem->data;

			/* need 3rd axis sets only */
			if (!g->data_c)
				continue;

			tmp = g->ymin - p->ymin;
			if (tmp > 0.0)
				if (tmp < p->dy)
					p->dy = tmp;
		}
	}

	if (p->dx == DBL_MAX || p->dy == DBL_MAX) {

		if (p->dy < DBL_MAX) {
			p->dx = p->dy;
		} else if (p->dx < DBL_MAX) {
			p->dy = p->dx;
		} else {
			p->dx  = p->xlen * 0.5;
			p->dy  = p->ylen * 0.5;
		}
	}
}


/**
 * @brief determine the ranges of the supplied data
 */

static void xyplot_data_range(struct graph *g)
{
	gsize i;

	gsize cnt;

	gdouble tmp;



	g->xmin = DBL_MAX;
	g->ymin = DBL_MAX;
	g->cmin = DBL_MAX;

	g->xmax = -DBL_MAX;
	g->ymax = -DBL_MAX;
	g->cmax = -DBL_MAX;

	g->dx = DBL_MAX;
	g->dy = DBL_MAX;


	cnt = 0;
	for (i = 0; i < g->data_len; i++) {

		gboolean delta = FALSE;

		tmp = g->data_x[i];

		if (tmp < g->xmin) {
			delta = TRUE;
			g->xmin = tmp;
		}

		if (tmp > g->xmax) {
			delta = TRUE;
			g->xmax = tmp;
		}

		if (delta)
			cnt++;
	}

	/* guess delta from number of distinct x values that caused the
	 * maximum or minimum to move
	 */
	if (cnt > 1)
		g->dx = fabs(g->xmax - g->xmin) / (gdouble) (cnt - 1);


	cnt = 0;
	for (i = 0; i < g->data_len; i++) {

		gboolean delta  = FALSE;;

		tmp = g->data_y[i];

		if (tmp < g->ymin) {
			delta = TRUE;
			g->ymin = tmp;
		}

		if (tmp > g->ymax) {
			delta = TRUE;
			g->ymax = tmp;
		}

		if (delta)
			cnt++;

	}

	if (cnt > 1)
		g->dy = fabs(g->ymax - g->ymin) / (gdouble) (cnt - 1);



	/* third axis is optional */
	if (!g->data_c)
		return;

	for (i = 0; i < g->data_len; i++) {

		tmp = g->data_c[i];

		if (tmp < g->cmin)
			g->cmin = tmp;

		if (tmp > g->cmax)
			g->cmax = tmp;
	}
}


/**
 * @brief draws the plot
 *
 * @note the plot is rendered onto it's own surface, so we don't need
 *	 to redraw it every time we want to add or update an overlay
 */

static void xyplot_plot_render(XYPlot *p, cairo_t *cr,
			       unsigned int width, unsigned int height)
{
	xyplot_draw_bg(p, cr);

	/* base color */
	cairo_set_source_rgba(cr, p->ax_colour.red,  p->ax_colour.green,
			      p->ax_colour.blue, p->ax_colour.alpha);

	xyplot_draw_plot_frame(p, cr, width, height);


	if (p->graphs) {

		xyplot_draw_grid_x(p, cr, width, height);
		xyplot_draw_grid_y(p, cr, width, height);
		xyplot_draw_ticks_x(p, cr, width, height);
		xyplot_draw_ticks_y(p, cr, width, height);
		xyplot_draw_ticks_x2(p, cr, width, height);
		xyplot_draw_ticks_y2(p, cr, width, height);
		xyplot_draw_tickslabels_x(p, cr, width, height);
		xyplot_draw_tickslabels_y(p, cr, width, height);
		xyplot_draw_tickslabels_x2(p, cr, width, height);
		xyplot_draw_tickslabels_y2(p, cr, width, height);

		/* create a clipping region so we won't draw parts of a graph
		 * outside of the frame
		 */
		cairo_rectangle(cr, p->plot_x, p->plot_y, p->plot_w , p->plot_h);
		cairo_clip(cr);
		cairo_new_path (cr);

		if (p->sel.active) {


			cairo_save(cr);

			xyplot_transform_origin(p, cr);

			cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0);

			cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

			cairo_set_line_width(cr, 2.0);

			cairo_rectangle(cr,
					(p->sel.xmin - p->x_ax.min) * p->scale_x,
					(p->sel.ymin - p->y_ax.min) * p->scale_y,
					(p->sel.xmax - p->sel.xmin) * p->scale_x,
					(p->sel.ymax - p->sel.ymin) * p->scale_y);

			cairo_stroke(cr);
			cairo_restore(cr);
		}

		xyplot_draw_graphs(p, cr);
		xyplot_draw_indicators(p, cr);

	} else {
		xyplot_write_text_centered(cr, 0.5 * width, 0.5 * height,
					   "NO DATA", 0.0);
	}
}


/**
 * @brief draws the plot
 *
 * @note the plot is rendered onto it's own surface, so we don't need
 *	 to redraw it every time we want to add or update an overlay
 */

static void xyplot_plot(XYPlot *p)
{
	unsigned int width;
	unsigned int height;

	gint fs = 0;

	gchar *t;
	gchar *font;

	cairo_t *cr;


	width  = gtk_widget_get_allocated_width(GTK_WIDGET(p));
	height = gtk_widget_get_allocated_height(GTK_WIDGET(p));


	cr = cairo_create(p->plot);

	/* we currently use the cairo_show_text "toy" api, so we just set the
	 * font size for now
	 */
	g_object_get(gtk_settings_get_default(), "gtk-font-name", &font, NULL);

	t = strtok(font, " ");
	t = strtok(NULL, " ");

	/* naively search for number */
	while (!fs && t) {
		fs = g_ascii_strtod(t, NULL);
		t = strtok(NULL, " ");
	}

	/* make a sensible choice */
	if ((fs < 5) || (fs > 20))
		fs = 10;

	cairo_set_font_size(cr, (double) fs);

	g_free(font);


	xyplot_plot_render(p, cr, width, height);

	cairo_destroy(cr);

	/* duplicate to render surface */
	cr = cairo_create(p->render);
	cairo_set_source_surface(cr, p->plot, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);


	gtk_widget_queue_draw(GTK_WIDGET(p));
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
		/* grab focus for keyboard input */
		gtk_widget_grab_focus(widget);
		cursor = gdk_cursor_new_from_name(display, "cell");
		break;
	case GDK_LEAVE_NOTIFY:
	default:
		cursor = gdk_cursor_new_from_name(display, "default");
		xyplot_plot(XYPLOT(widget));
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
	gdouble dx, dy;

	XYPlot *p;
	cairo_t *cr;
	PangoLayout *layout;
	GdkDisplay *display;


	char buf[256];


	if (!event->is_hint)
		goto exit;


	if (event->state & GDK_MOD1_MASK)
		return TRUE;


	p = XYPLOT(widget);

	cr = cairo_create(p->render);

	/* paint plot surface to render surface */
	cairo_set_source_surface(cr, p->plot, 0, 0);
	cairo_paint(cr);
	cairo_set_source_surface(cr, p->render, 0, 0);



	/* get plot pixel reference */
	px = event->x - p->plot_x;
	py = p->plot_y + p->plot_h - event->y;

	/* get data range reference */
	x = px / p->scale_x + p->x_ax.min;
	y = py / p->scale_y + p->y_ax.min;


	snprintf(buf, ARRAY_SIZE(buf),
		 "<span foreground='#dddddd'"
		 "	font_desc='Sans Bold 8'>"
		 "<tt>X:  %+6g\n", x );

	if (p->conv_to_x2)
		snprintf(&buf[strlen(buf)], ARRAY_SIZE(buf) - strlen(buf),
			 "X2: %+6g\n", p->conv_to_x2(x, p->x2_userdata));

	snprintf(&buf[strlen(buf)], ARRAY_SIZE(buf) - strlen(buf),
		 "Y:  %+6g\n", y );

	if (p->conv_to_y2)
		snprintf(&buf[strlen(buf)], ARRAY_SIZE(buf) - strlen(buf),
			 "Y2: %+6g\n", p->conv_to_y2(x, p->y2_userdata));

	snprintf(&buf[strlen(buf)], ARRAY_SIZE(buf) - strlen(buf),
		 "</tt></span>");


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

	if ((event->state & GDK_SHIFT_MASK) &&
	    (event->state & GDK_BUTTON1_MASK)) {

		dx = (p->shift.x0 - event->x) / p->scale_x;
		dy = (p->shift.y0 - event->y) / p->scale_y;

		p->shift.x0 = event->x;
		p->shift.y0 = event->y;

		if ((dx != 0.0) || (dy != 0.0)) {
			p->autorange_x = FALSE;
			p->autorange_y = FALSE;
		}

		p->xmin += dx;
		p->xmax += dx;
		p->ymin -= dy;
		p->ymax -= dy;

		p->xlen = p->xmax - p->xmin;
		p->ylen = p->ymax - p->ymin;

		cairo_destroy (cr);
		xyplot_auto_axes(p);

		xyplot_plot(p);

		return TRUE;
	}


	if (event->state & GDK_BUTTON1_MASK) {

		/* get plot pixel reference */
		p->rub.px0 = (p->rub.x0 - p->plot_x) / p->scale_x + p->x_ax.min;
		p->rub.py0 = (p->plot_y + p->plot_h - p->rub.y0) / p->scale_y
			     + p->y_ax.min;

		p->rub.px1 = x;
		p->rub.py1 = y;

		xyplot_rubberband_minmax_order(p);


		cairo_save(cr);

		if (event->state & GDK_CONTROL_MASK)
			cairo_set_source_rgba(cr, 1.0, 1.0, 0.0, 1.0);
		else
			cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);

		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

		cairo_set_line_width(cr, 2.0);

		cairo_rectangle(cr,
				p->rub.x0, p->rub.y0,
				event->x - p->rub.x0, event->y - p->rub.y0);

		if (event->state & GDK_CONTROL_MASK) {
			p->sel.xmin = p->rub.px0;
			p->sel.xmax = p->rub.px1;
			p->sel.ymin = p->rub.py0;
			p->sel.ymax = p->rub.py1;
		}

		cairo_stroke(cr);
		cairo_restore(cr);
	}


	xyplot_render_layout(cr, layout, x0, y0);

	cairo_destroy (cr);

	/* _draw_area() may leave artefacts if the pointer is moved to fast */
	gtk_widget_queue_draw(widget);

exit:
	return TRUE;
}


/**
 * @brief button release events
 */

static gboolean xyplot_button_release_cb(GtkWidget *widget,
					 GdkEventButton *event, gpointer data)
{
	XYPlot *p;


	gdouble xlen;
	gdouble ylen;


	p = XYPLOT(widget);

	if (event->type != GDK_BUTTON_RELEASE)
		return TRUE;

	if (event->button != 1)
		return TRUE;

	if (event->state & GDK_SHIFT_MASK)
		return TRUE;

	if (event->state & GDK_SHIFT_MASK)
		return TRUE;


	if (event->state & GDK_CONTROL_MASK) {

		g_message("FIT ALL DATA X: %g to %g and Y: %g to %g",
			  p->sel.xmin, p->sel.xmax, p->sel.ymin, p->sel.ymax);

		p->sel.active = TRUE;
		g_signal_emit_by_name(widget, "xyplot-fit-selection");

		return TRUE;
	}


	xlen = p->rub.px1 - p->rub.px0;
	ylen = p->rub.py1 - p->rub.py0;

	if (xlen == 0.0 || ylen == 0.0)
		return TRUE;

	p->xmin = p->rub.px0;
	p->xmax = p->rub.px1;
	p->ymin = p->rub.py0;
	p->ymax = p->rub.py1;
	p->xlen = xlen;
	p->ylen = ylen;

	p->autorange_x = FALSE;
	p->autorange_y = FALSE;

	xyplot_auto_axes(p);

	xyplot_plot(p);


	return TRUE;
}

static gboolean xyplot_key_press_cb(GtkWidget *widget, GdkEventKey *event,
				    gpointer data)
{
	XYPlot *p;


	p = XYPLOT(widget);

	if (event->keyval == GDK_KEY_a)
		xyplot_autorange_cb(widget, p);

	if (event->keyval == GDK_KEY_u)
		xyplot_clear_selection_cb(widget, p);

	return TRUE;
}

/**
 * @brief button press events
 */

static gboolean xyplot_button_press_cb(GtkWidget *widget, GdkEventButton *event,
				       gpointer data)
{
	gdouble x, y;
	gdouble px, py;

	XYPlot *p;


	p = XYPLOT(widget);

	if (event->type != GDK_BUTTON_PRESS)
		goto exit;



	if ((event->button == 1) &&
	    (event->state & GDK_MOD1_MASK) ) {

		/* get plot pixel reference */

		px = event->x - p->plot_x;
		py = p->plot_y + p->plot_h - event->y;

		/* get data range reference */
		x = px / p->scale_x + p->x_ax.min;
		y = py / p->scale_y + p->y_ax.min;

		g_signal_emit_by_name(p, "xyplot-clicked-xy-coord", x, y);

		goto exit;
	}




	/* get plot pixel reference for axis shift */
	if (event->state & GDK_SHIFT_MASK) {
		p->shift.x0 = event->x;
		p->shift.y0 = event->y;
	}

	if (event->button == 1) {
		p->rub.x0 = event->x;
		p->rub.y0 = event->y;
	}

	if (event->button == 3)
		xyplot_popup_menu(widget);

exit:
	return TRUE;
}



/**
 * @brief scroll events
 */

static gboolean xyplot_scroll_cb(GtkWidget *widget, GdkEvent *event,
				 gpointer data)
{
	gdouble dx, dy;
	double tmp;

	XYPlot *p;


	p = XYPLOT(widget);


	if ((p->ylen == 0.0) && (p->xlen == 0.0))
		return TRUE;

	gdk_event_get_scroll_deltas(event, &dx, &dy);


	/* use any scroll axis */
	if (dx == 0.0)
		dx = dy;
	if (dy == 0.0)
		dy = dx;


	dx = dx * p->xlen * 0.05;
	dy = dy * p->ylen * 0.05;


	if (event->scroll.state & GDK_MOD1_MASK) {
		p->xmin += dx;
		p->xmax -= dx;
		p->autorange_x = FALSE;
	} else  if (event->scroll.state & GDK_SHIFT_MASK) {
		p->ymin += dy;
		p->ymax -= dy;
		p->autorange_y = FALSE;
	} else {
		p->xmin += dx;
		p->xmax -= dx;
		p->ymin += dy;
		p->ymax -= dy;
		p->autorange_x = FALSE;
		p->autorange_y = FALSE;
	}

	if (p->ymin > p->ymax) {
		tmp = p->ymin;
		p->ymin = p->ymax;
		p->ymax = tmp;
	}

	if (p->xmin > p->xmax) {
		tmp = p->xmin;
		p->xmin = p->xmax;
		p->xmax = tmp;
	}

	p->xlen = p->xmax - p->xmin;
	p->ylen = p->ymax - p->ymin;


	xyplot_auto_axes(p);

	xyplot_plot(p);

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

	if (p->plot)
	    cairo_surface_destroy(p->plot);


	width  = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	p->plot = gdk_window_create_similar_surface(win, CAIRO_CONTENT_COLOR,
						       width, height);

	p->render = gdk_window_create_similar_surface(win, CAIRO_CONTENT_COLOR,
						       width, height);

	xyplot_plot(p);

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

	g_signal_new("xyplot-fit-selection",
		     TYPE_XYPLOT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);

	g_signal_new("xyplot-clicked-xy-coord",
		     TYPE_XYPLOT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);


	/* override widget methods go here if needed */
}


/**
 * @brief initialise the plot parameters
 */

static void xyplot_init(XYPlot *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_XYPLOT(p));

	/* initialisation of plotting internals */
	p->xlabel = NULL;
	p->ylabel = NULL;
	p->x2label = NULL;
	p->y2label = NULL;
	p->pad    = 20.0;

	p->graphs = NULL;

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


	p->bg_colour.red   = BG_R;
	p->bg_colour.green = BG_G;
	p->bg_colour.blue  = BG_B;
	p->bg_colour.alpha = 1.0;

	p->ax_colour.red   = AXES_R;
	p->ax_colour.green = AXES_G;
	p->ax_colour.blue  = AXES_B;
	p->ax_colour.alpha = 1.0;

	p->autorange_x = TRUE;
	p->autorange_y = TRUE;

	p->conv_to_x2 = NULL;
	p->conv_to_y2 = NULL;


	/* connect the relevant signals of the DrawingArea */
	g_signal_connect(G_OBJECT(&p->parent), "draw",
			  G_CALLBACK(xyplot_draw_cb), NULL);

	g_signal_connect(G_OBJECT(&p->parent), "motion-notify-event",
			 G_CALLBACK(xyplot_motion_notify_event_cb), NULL);

	g_signal_connect(G_OBJECT(&p->parent), "configure-event",
			 G_CALLBACK(xyplot_configure_event_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "enter-notify-event",
			  G_CALLBACK(xyplot_pointer_crossing_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "leave-notify-event",
			  G_CALLBACK(xyplot_pointer_crossing_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "button-press-event",
			  G_CALLBACK(xyplot_button_press_cb), NULL);
	g_signal_connect (G_OBJECT(&p->parent), "button-release-event",
			  G_CALLBACK(xyplot_button_release_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "scroll-event",
			  G_CALLBACK(xyplot_scroll_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "key-press-event",
			  G_CALLBACK(xyplot_key_press_cb), NULL);

	/* need to set can-focus for key presses, need to grab focus
	 * as well, we do this in enter-notify so we don't interfere with
	 * other widgets all the time
	 */
	gtk_widget_set_can_focus(GTK_WIDGET(p), TRUE);

	gtk_widget_set_events(GTK_WIDGET(&p->parent), GDK_EXPOSURE_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK
			       | GDK_POINTER_MOTION_HINT_MASK
			       | GDK_ENTER_NOTIFY_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_SMOOTH_SCROLL_MASK
			       | GDK_KEY_PRESS_MASK);
}


/**
 * @brief g_free() a graph and all its contents
 */

static void xyplot_free_graph(struct graph *g)
{
	if (!g)
		return;

	g_free(g->data_x);
	g_free(g->data_y);
	g_free(g->data_c);
	g_free(g->label);
	g_free(g);
}


/**
 * @brief extract all data within x/y limits
 *
 * @param p the XYPlot widget
 * @param x the array where the x-data will be stored
 * @param y the array where the y-data will be stored
 * @param c the array where the color-data will be stored (may be NULL)
 *
 * @returns the number of elements in the arrays
 *
 * XXX this function is pretty inefficient for large amounts of data, but
 *     it'll do for now
 */

static ssize_t xyplot_extract_data(XYPlot *p,
				   GArray *gx, GArray *gy, GArray *gc,
				   gdouble xmin, gdouble xmax,
				   gdouble  ymin, gdouble ymax)
{
	size_t i;
	size_t n = 0;

	GList *elem;

	struct graph *g;


	for (elem = p->graphs; elem; elem = elem->next) {

		g  = (struct graph *) elem->data;

		for (i = 0; i < g->data_len; i++) {

			if (g->data_x[i] >= xmin) {
				if (g->data_x[i] <= xmax) {
					if (g->data_y[i] >= ymin) {
						if (g->data_y[i] <= ymax) {
							g_array_append_val(gx, g->data_x[i]);
							g_array_append_val(gy, g->data_y[i]);
							if (gc && g->data_c)
								g_array_append_val(gc, g->data_c[i]);
							n++;
						}
					}
				}
			}
		}
	}


	return n;
}




/**
 * @brief get the data inside the selection box
 *
 * @param widget the XYPlot widget
 * @param x pointer-to-pointer where the x-data will be stored
 * @param y pointer-to-pointer where the y-data will be stored
 * @param c pointer-to-pointer where the color-data will be stored (may be NULL)
 *
 * @returns the number of elements in the arrays
 *
 * @note the caller must deallocate the buffers
 */

size_t xyplot_get_selection_data(GtkWidget *widget,
				 gdouble **x, gdouble **y, gdouble **c)
{
	size_t n;

	XYPlot *p;

	GArray *gx, *gy, *gc;


	p = XYPLOT(widget);

	if (!p->sel.active)
		return 0;

	gx = g_array_new(FALSE, FALSE, sizeof(gdouble));
	gy = g_array_new(FALSE, FALSE, sizeof(gdouble));

	if (c)
		gc = g_array_new(FALSE, FALSE, sizeof(gdouble));
	else
		gc = NULL;


	n = xyplot_extract_data(p, gx, gy, gc,
				p->sel.xmin, p->sel.xmax,
				p->sel.ymin, p->sel.ymax);

	(*x) = (gdouble *) gx->data;
	(*y) = (gdouble *) gy->data;
	if (gc)
		(*c) = (gdouble *) gc->data;

	g_array_free(gx, FALSE);
	g_array_free(gy, FALSE);

	if (gc)
		g_array_free(gc, FALSE);


	return n;
}


/**
 * @brief select all data in plot and emit fit selection signal
 *
 * @param widget the XYPlot widget
 *
 * @note this clears the prior active selection
 */

void xyplot_select_all_data(GtkWidget *widget)

{
	XYPlot *p;


	p = XYPLOT(widget);


	p->sel.xmin = p->xmin;
	p->sel.xmax = p->xmax;
	p->sel.ymin = p->xmin;
	p->sel.ymax = p->ymax;

	p->sel.active = TRUE;

	g_signal_emit_by_name(widget, "xyplot-fit-selection");
}



/**
 * @brief add an indicator with a label to the x-Axis
 *
 * @note a call to xyplot_erase_indicators will free the label!
 */

void xyplot_draw_indicator_x(GtkWidget *widget, gdouble x, gchar *label)
{
	XYPlot *plot;

	plot = XYPLOT(widget);


	if (plot->ind_x.lbl)
		g_free(plot->ind_x.lbl);

	plot->ind_x.lbl = label;
	plot->ind_x.pos = x;
}


/**
 * @brief add an indicator with a label to the Y-Axis
 *
 * @note a call to xyplot_erase_indicators will free the label!
 */

void xyplot_draw_indicator_y(GtkWidget *widget, gdouble y, gchar *label)
{
	XYPlot *plot;

	plot = XYPLOT(widget);


	if (plot->ind_y.lbl)
		g_free(plot->ind_y.lbl);

	plot->ind_y.lbl = label;
	plot->ind_y.pos = y;
}

/**
 * @brief clear all indicators
 */

void xyplot_erase_indicators(GtkWidget *widget)
{
	XYPlot *plot;

	plot = XYPLOT(widget);

	g_free(plot->ind_x.lbl);
	g_free(plot->ind_y.lbl);

	plot->ind_x.lbl = NULL;
	plot->ind_y.lbl = NULL;
}


/**
 * @brief set a conversion function for the x2-axis display
 * @param conv_func the conversion function, pass NULL to unset
 */

void xyplot_set_x2_conversion(GtkWidget *widget,
			      gdouble (*conv_func)(gdouble x, gpointer data),
			      gpointer userdata)
{
	XYPlot *plot;

	plot = XYPLOT(widget);


	plot->conv_to_x2  = conv_func;
	plot->x2_userdata = userdata;
}

/**
 * @brief set a conversion function for the y2-axis display
 * @param conv_func the conversion function, pass NULL to unset
 */

void xyplot_set_y2_conversion(GtkWidget *widget,
			      gdouble (*conv_func)(gdouble y, gpointer data),
			      gpointer userdata)
{
	XYPlot *plot;

	plot = XYPLOT(widget);


	plot->conv_to_y2  = conv_func;
	plot->y2_userdata = userdata;
}


/**
 * @brief drop all graphs
 *
 * @param widget the XYPlot widget
 */

void xyplot_drop_all_graphs(GtkWidget *widget)
{
	XYPlot *plot;

	GList *elem;

	plot = XYPLOT(widget);

	if (!plot)
		return;

	for (elem = plot->graphs; elem; elem = elem->next)
		xyplot_free_graph((struct graph *) elem->data);

	g_list_free(plot->graphs);
	plot->graphs = NULL;

	plot->sel.active = FALSE;
}


/**
 * @brief drop all graphs in the cleanup list
 *
 * @param widget the XYPlot widget
 */

static void xyplot_drop_all_graphs_cleanup(GtkWidget *widget)
{
	XYPlot *plot;

	GList *elem;
	GList *tmp;

	struct graph *g;


	plot = XYPLOT(widget);

	if (!plot)
		return;

	for (elem = plot->graphs_cleanup; elem; elem = elem->next) {

		g = (struct graph *) elem->data;

		if (!g_ref_count_dec(&g->ref))
			continue;

		tmp = elem->next;

		plot->graphs_cleanup = g_list_remove_link(plot->graphs_cleanup,
							  elem);
		xyplot_free_graph(g);

		if (tmp && tmp->prev)
			elem = tmp->prev;
	}

	if (!g_list_length(plot->graphs_cleanup)) {
		g_list_free(plot->graphs_cleanup);
		plot->graphs_cleanup = NULL;
	}
}



/**
 * @brief drop a dataset by reference
 *
 * @param widget the XYPlot widget
 * @param ref a reference to a graph
 *
 * @note a valid call to this function will g_free() the data and the label
 *
 */

void xyplot_drop_graph(GtkWidget *widget, void *ref)
{
	XYPlot *plot;

	GList *elem;

	struct graph *g;


	plot = XYPLOT(widget);

	if (!plot)
		return;

	if (!ref)
		return;


	g = (struct graph *) ref;

	elem = g_list_find(plot->graphs, g);

	if (!elem) {
		g_warning("%s: graph reference not found!", __func__);
		return;
	}

	plot->graphs = g_list_remove_link(plot->graphs, elem);

	if (!GTK_IS_WIDGET(plot->menu) || !gtk_widget_get_visible(plot->menu)) {

		/* free the current graph and the cleanup list */

		if (g_ref_count_dec(&g->ref)) {
			xyplot_free_graph(g);
			g_list_free(elem);
		}

		xyplot_drop_all_graphs_cleanup(widget);

	} else {
		g_ref_count_dec(&g->ref);
		plot->graphs_cleanup = g_list_append(plot->graphs_cleanup, g);
	}

#if 0
	xyplot_auto_range(plot);
	xyplot_auto_axes(plot);

	xyplot_plot(plot);
#endif
}



/**
 * @brief add a dataset to plot
 *
 * @param widget the XYPlot widget
 * @param x an array of x axis values
 * @param y an array of y axis values
 * @param c an array of c axis (color, z) values (may be NULL)
 * @param size the number of elements in the arrays
 * @param label the name of the graph (may be NULL)
 *
 *
 * @note we expect len(x) == len(y) == len(c)
 * @note the data AND the label string must be allocated on the heap,
 *	 so we can g_free()
 *
 * @returns a reference to the data set in the plot or NULL on error
 *
 * @note call xyplot_redraw() to update plot!
 *
 * XXX: need mechanism to mark "fit" data other than the graph label!
 */

void *xyplot_add_graph(GtkWidget *widget,
		       gdouble *x, gdouble *y, gdouble *c,
		       gsize size, gchar *label)
{
	XYPlot *p;

	struct graph *g;


	p = XYPLOT(widget);

	if (!p)
		return NULL;
	if (!x)
		return NULL;
	if (!y)
		return NULL;


	g = (struct graph *) g_malloc0(sizeof(struct graph));



	g->data_x   = x;
	g->data_y   = y;
	g->data_c   = c;
	g->data_len = size;
	g->label    = label;
	g->parent   = p;
	g->style    = STAIRS;

	g->colour.red   = GRAPH_R;
	g->colour.green = GRAPH_G;
	g->colour.blue  = GRAPH_B;
	g->colour.alpha = 1.0;

	xyplot_data_range(g);

	g_ref_count_init(&g->ref);
	p->graphs = g_list_append(p->graphs, g);

	xyplot_auto_range(p);
	xyplot_auto_axes(p);

	if (strcmp("FIT", label))
		if (p->sel.active)
			g_signal_emit_by_name(widget, "xyplot-fit-selection");

	return g;
}


void xyplot_set_graph_style(GtkWidget *widget, void *ref,
			    enum xyplot_graph_style style)
{
	XYPlot *plot;

	GList *elem;

	struct graph *g;


	plot = XYPLOT(widget);

	if (!plot)
		return;

	if (!ref)
		return;

	g = (struct graph *) ref;

	elem = g_list_find(plot->graphs, g);

	if (!elem) {
		g_warning("%s: graph reference not found!", __func__);
		return;
	}

	g->style = style;
}

void xyplot_set_graph_rgba(GtkWidget *widget, void *ref,
			   GdkRGBA colour)
{
	XYPlot *p;

	GList *elem;

	struct graph *g;


	p = XYPLOT(widget);

	if (!p)
		return;

	if (!ref)
		return;

	g = (struct graph *) ref;

	elem = g_list_find(p->graphs, g);

	if (!elem) {
		g_warning("%s: graph reference not found!", __func__);
		return;
	}

	g->colour.red   = colour.red;
	g->colour.green = colour.green;
	g->colour.blue  = colour.blue;
	g->colour.alpha = colour.alpha;
}


/**
 * @brief retreive the colour of a graph
 *
 * @returns 0 on success, otherwise error
 */

int xyplot_get_graph_rgba(GtkWidget *widget, void *ref, GdkRGBA *colour)
{
	XYPlot *p;

	GList *elem;

	struct graph *g;


	p = XYPLOT(widget);

	if (!p)
		return -1;

	if (!ref)
		return -1;

	g = (struct graph *) ref;

	elem = g_list_find(p->graphs, g);

	if (!elem) {
		g_warning("%s: graph reference not found!", __func__);
		return -1;
	}


	colour->red   = g->colour.red;
	colour->green = g->colour.green;
	colour->blue  = g->colour.blue;
	colour->alpha = g->colour.alpha;

	return 0;
}



void xyplot_get_sel_axis_range(GtkWidget *widget,
			   gdouble *xmin, gdouble *xmax,
			   gdouble *ymin, gdouble *ymax)
{
	XYPlot *p;


	p = XYPLOT(widget);

	if (!p)
		return;

	if (xmin)
		(*xmin) = p->sel.xmin;
	if (xmax)
		(*xmax) = p->sel.xmax;
	if (ymin)
		(*ymin) = p->sel.ymin;
	if (ymax)
		(*ymax) = p->sel.ymax;


}


void xyplot_get_data_axis_range(GtkWidget *widget,
			   gdouble *xmin, gdouble *xmax,
			   gdouble *ymin, gdouble *ymax)
{
	XYPlot *p;


	p = XYPLOT(widget);

	if (!p)
		return;

	if (xmin)
		(*xmin) = p->xmin;
	if (xmax)
		(*xmax) = p->xmax;
	if (ymin)
		(*ymin) = p->ymin;
	if (ymax)
		(*ymax) = p->ymax;


}


void xyplot_set_range_x(GtkWidget *widget, gdouble min, gdouble max)
{
	XYPlot *p;


	p = XYPLOT(widget);

	p->autorange_x = FALSE;
	p->xmin = min;
	p->xmax = max;
	p->xlen = max - min;

	xyplot_auto_range(p);
	xyplot_auto_axes(p);

	xyplot_plot(p);
}


void xyplot_set_range_y(GtkWidget *widget, gdouble min, gdouble max)
{
	XYPlot *p;


	p = XYPLOT(widget);

	p->autorange_y = FALSE;
	p->ymin = min;
	p->ymax = max;
	p->ylen = max - min;

	xyplot_auto_range(p);
	xyplot_auto_axes(p);

	xyplot_plot(p);
}



void xyplot_redraw(GtkWidget *widget)
{
	xyplot_plot(XYPLOT(widget));
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
