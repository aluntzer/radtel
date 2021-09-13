/**
 * @file    widgets/history/history.c
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
 * @brief a widget to control the history position
 *
 */


#include <history.h>
#include <history_cfg.h>
#include <xyplot.h>

#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <cmd.h>
#include <math.h>

#include <levmar.h>
#include <math.h>

#include <coordinates.h>


G_DEFINE_TYPE_WITH_PRIVATE(History, history, GTK_TYPE_BOX)

#define HISTORY_DEFAULT_HST_LEN 100

/* waterfall colours */
#define HISTORY_R_LO		0
#define HISTORY_G_LO		0
#define HISTORY_B_LO		0

#define HISTORY_R_MID		255
#define HISTORY_G_MID		0
#define HISTORY_B_MID		0

#define HISTORY_R_HI		255
#define HISTORY_G_HI		255
#define HISTORY_B_H		0

#define HISTORY_REFRESH_HZ_CAP  30.

#define HISTORY_REFRESH_AVG_LEN 10.

#define HISTORY_REFRESH_DUTY_CYCLE 0.8


/**
 * @brief redraw the plot if the configured time has expired
 */

static void history_plot_try_refresh(GtkWidget *w, History *p)
{
	gdouble elapsed;

	const double n  = 1.0 / HISTORY_REFRESH_AVG_LEN;
	const double n1 = HISTORY_REFRESH_AVG_LEN - 1.0;


	g_timer_stop(p->cfg->timer);

	elapsed = g_timer_elapsed(p->cfg->timer, NULL);

	if (elapsed > p->cfg->refresh) {

		/* reuse the timer to measure drawing time */
		g_timer_start(p->cfg->timer);
		xyplot_redraw(w);
		g_timer_stop(p->cfg->timer);

		elapsed = g_timer_elapsed(p->cfg->timer, NULL);

		elapsed /= HISTORY_REFRESH_DUTY_CYCLE;

		/* adapt refresh rate */
		p->cfg->refresh = (p->cfg->refresh * n1 + elapsed) * n;

		if (p->cfg->refresh < (1.0 / HISTORY_REFRESH_HZ_CAP))
			p->cfg->refresh = (1.0 / HISTORY_REFRESH_HZ_CAP);

		g_timer_start(p->cfg->timer);
	} else {
		g_timer_continue(p->cfg->timer);
	}

}


static void history_wf_slide_lo_value_changed(GtkRange *range,
					      gpointer  data)
{
	History *p;


	p = HISTORY(data);

	p->cfg->th_lo = gtk_range_get_value(range);
}


static void history_wf_slide_hi_value_changed(GtkRange *range,
					      gpointer  data)
{
	History *p;


	p = HISTORY(data);

	p->cfg->th_hi = gtk_range_get_value(range);
}


static void history_wf_slide_min_value_changed(GtkRange *range,
					      gpointer  data)
{
	History *p;


	p = HISTORY(data);

	p->cfg->wf_min = gtk_range_get_value(range);
}



/**
 * @brief average colour set callback
 */

static void history_hst_colour_set_cb(GtkColorButton *w, History *p)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &p->cfg->c_hst);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_hst, p->cfg->c_hst);
	xyplot_redraw(p->cfg->plot);
}

static void history_set_plot_style(gint active, enum xyplot_graph_style *s)
{
	switch (active) {
	case 0:
		(*s) = STAIRS;
		break;
	case 1:
		(*s) = LINES;
		break;
	case 2:
		(*s) = DASHES;
		break;
	case 3:
		(*s) = CURVES;
		break;
	case 4:
		(*s) = CIRCLES;
		break;
	case 5:
		(*s) = SQUARES;
		break;
	case 6:
		(*s) = IMPULSES;
		break;

	default:
		break;
	}
}


static void history_hst_style_changed(GtkComboBox *cb, History *p)
{
	history_set_plot_style(gtk_combo_box_get_active(cb), &p->cfg->s_hst);
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_hst, p->cfg->s_hst);
	xyplot_redraw(p->cfg->plot);
}


/**
 * @brief clear history
 */

static void history_clear_hst(History *p)
{
	g_array_set_size(p->cfg->hst_idx, 0);
	g_array_set_size(p->cfg->hst_pwr, 0);
}


/**
 * @brief append new data to power history
 */

static void history_append_hst(History *p, const gdouble *amp, gsize len)
{
	gsize i;
	gsize n;

	gdouble pwr = 0.0;

	gdouble *x;
	gdouble *y;

	gint64 *prv;
	gint64 now;


	now = g_get_monotonic_time(); //UT_seconds();

	/* remove the old graph */
	xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
	p->cfg->r_hst = NULL;
	xyplot_drop_graph(p->cfg->plot, p->cfg->r_lst);
	p->cfg->r_lst = NULL;

	/* history is disabled? */
	if (!p->cfg->n_hst)
		return;

	n = p->cfg->hst_idx->len;

	/* drop the oldest */
	if (n == p->cfg->n_hst) {
		g_array_remove_index(p->cfg->hst_idx, 0);
		g_array_remove_index(p->cfg->hst_pwr, 0);
	}

	for (i = 0; i < len; i++)
		pwr += amp[i];

	pwr /= (gdouble) len;	/* average power */

	g_array_append_val(p->cfg->hst_idx, now);
	g_array_append_val(p->cfg->hst_pwr, pwr);

	/* create new graph */
	x = (gdouble *) g_memdup2(p->cfg->hst_idx->data,
				  p->cfg->hst_idx->len * sizeof(gdouble));
	y = (gdouble *) g_memdup2(p->cfg->hst_pwr->data,
				  p->cfg->hst_pwr->len * sizeof(gdouble));

	prv = (gint64 *) p->cfg->hst_idx->data;
	for (i = 0; i < p->cfg->hst_idx->len; i++)
		x[i] = (gdouble) (prv[i] - now) / G_USEC_PER_SEC;

	p->cfg->r_hst = xyplot_add_graph(p->cfg->plot, x, y, NULL,
					 p->cfg->hst_idx->len,
					 g_strdup_printf("History"));
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_hst, p->cfg->s_hst);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_hst, p->cfg->c_hst);


	/* indicate last update as single point graph */
	x = g_malloc(sizeof(double));
	y = g_malloc(sizeof(double));

	x[0] = 0.0;
	y[0] = pwr;

	p->cfg->r_lst = xyplot_add_graph(p->cfg->plot, x, y, NULL,
					 1,
					 g_strdup_printf("Last"));
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_lst, CIRCLES);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_lst, red);

	history_plot_try_refresh(p->cfg->plot, p);
}


/**
 * @brief get an rgb colour mapping for a value
 *
 * @note the is scheme is stolen from somewhere, but I don't remember... d'oh!
 */

static void history_wf_get_rgb(gdouble val, gdouble thr_lo, gdouble thr_hi,
			       guchar *r, guchar *g, guchar *b)
{
	gdouble R, G, B;

	gdouble f;


	if (val < thr_lo) {
		(*r) = HISTORY_R_LO;
		(*g) = HISTORY_G_LO;
		(*b) = HISTORY_G_LO;
		return;
	}

	if (val > thr_hi) {
		(*r) = HISTORY_R_HI;
		(*g) = HISTORY_G_HI;
		(*b) = HISTORY_G_HI;
		return;
	}

	f = (val - thr_lo) / (thr_hi - thr_lo);

	if (f < 2.0/9.0) {

		f = f / (2.0 / 9.0);

		R = (1.0 - f) * (gdouble) HISTORY_R_LO;
		G = (1.0 - f) * (gdouble) HISTORY_G_LO;
		B = HISTORY_B_LO + f * (gdouble) (255 - HISTORY_B_LO);


	} else if (f < (3.0 / 9.0)) {

		f = (f - 2.0 / 9.0 ) / (1.0 / 9.0);

		R = 0.0;
		G = 255.0 * f;
		B = 255.0;

	} else if (f < (4.0 / 9.0) ) {

		f = (f - 3.0 / 9.0) / (1.0 / 9.0);

		R = 0.0;
		G = 255.0;
		B = 255.0 * (1.0 - f);

	} else if (f < (5.0 / 9.0)) {

		f = (f - 4.0 / 9.0 ) / (1.0 / 9.0);

		R = 255.0 * f;
		G = 255.0;
		B = 0.0;

	} else if ( f < (7.0 / 9.0)) {

		f = (f - 5.0 / 9.0 ) / (2.0 / 9.0);

		R = 255.0;
		G = 255.0 * (1.0 - f);
		B = 0.0;

	} else if( f < (8.0 / 9.0)) {

		f = (f - 7.0 / 9.0 ) / (1.0 / 9.0);

		R = 255.0;
		G = 0.0;
		B = 255.0 * f;

	} else {

		f = (f - 8.0 / 9.0 ) / (1.0 / 9.0);

		R = 255.0 * (0.75 + 0.25 * (1.0 - f));
		G = 0.5 * 255.0 * f;
		B = 255.0;
	}

	(*r) = (guchar) R;
	(*g) = (guchar) G;
	(*b) = (guchar) B;
}




/**
 * @brief append new data set to waterfall
 */

static void history_append_wf(History *p, const gdouble *amp, gsize len)
{

	int i;
       	int w, h;
	int rs, nc;

	gdouble min = DBL_MAX;
	gdouble max = DBL_MIN;
	gdouble n, n1;

	guchar *wf;
	guchar *pix;



	if (p->cfg->wf_pb) {
		if (len != gdk_pixbuf_get_width(p->cfg->wf_pb)) {
			g_object_unref(p->cfg->wf_pb);
			p->cfg->wf_pb = NULL;
		}
	}

	if (p->cfg->wf_pb) {
		if (p->cfg->wf_n_max != gdk_pixbuf_get_height(p->cfg->wf_pb)) {
			g_object_unref(p->cfg->wf_pb);
			p->cfg->wf_pb = NULL;
		}
	}


	if (!p->cfg->wf_pb) {

		if (!p->cfg->wf_n_max)
			return;

		p->cfg->wf_pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
					       len, p->cfg->wf_n_max);

		p->cfg->wf_n      = 0;
		p->cfg->wf_av_min = 0.0;
		p->cfg->wf_av_max = 0.0;

		if (!p->cfg->wf_pb) {
			g_warning("Could not create pixbuf: out of memory");
			return;
		}

		wf = gdk_pixbuf_get_pixels(p->cfg->wf_pb);

		gdk_pixbuf_fill(p->cfg->wf_pb, 0x000000ff);
	}


	wf = gdk_pixbuf_get_pixels(p->cfg->wf_pb);

	w = gdk_pixbuf_get_width(p->cfg->wf_pb);
	h = gdk_pixbuf_get_height(p->cfg->wf_pb);

	rs = gdk_pixbuf_get_rowstride(p->cfg->wf_pb);

	nc = gdk_pixbuf_get_n_channels(p->cfg->wf_pb);

	memmove(&wf[rs], wf, nc * (h - 1) * w);

	for (i = 0; i < len; i++) {
		if (amp[i] < min)
			min = amp[i];

		if (amp[i] > max)
			max = amp[i];
	}

	/* update moving average */
	if (p->cfg->wf_n < p->cfg->wf_n_max)
		p->cfg->wf_n++;

	n  = (gdouble) p->cfg->wf_n;
	n1 = (gdouble) (p->cfg->wf_n - 1);

	p->cfg->wf_av_min = (n1 * p->cfg->wf_av_min + min) / n;
	p->cfg->wf_av_max = (n1 * p->cfg->wf_av_max + max) / n;

	gtk_range_set_range(GTK_RANGE(p->cfg->s_min),
			    p->cfg->wf_av_min, p->cfg->wf_av_max);

	pix = wf;

	/* add new line */
	for (i = 0; i < len; i++) {
		history_wf_get_rgb((amp[i] - p->cfg->wf_min) / (max - min),
				   p->cfg->th_lo, p->cfg->th_hi,
				   &pix[0], &pix[1], &pix[2]);
		pix += nc;
	}


	gtk_widget_queue_draw(GTK_WIDGET(p->cfg->wf_da));
}


/**
 * @brief handle capabilities data
 */

static void history_handle_pr_spec_data(gpointer instance,
					 const struct spec_data *s,
					 gpointer data)
{
	uint64_t i;
	uint64_t f;

	gdouble *amp;


	History *p;

	p = HISTORY(data);


	if (!s->n)
		return;

	amp = g_malloc(s->n * sizeof(gdouble));

	for (i = 0, f = s->freq_min_hz; i < s->n; i++, f += s->freq_inc_hz)
		amp[i] = (gdouble) s->spec[i] * 0.001;	/* mK to K */


	history_append_hst(p, amp, s->n);
	history_append_wf(p, amp, s->n);

	g_free(amp);
}


/**
 * @brief button reset history callback
 */

static void history_reset_hst_cb(GtkWidget *w, History *p)
{
	history_clear_hst(p);

	xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
	p->cfg->r_hst = NULL;

	xyplot_drop_graph(p->cfg->plot, p->cfg->r_lst);
	p->cfg->r_lst = NULL;

	xyplot_redraw(p->cfg->plot);
}


/**
 * @brief handle running average length change
 */

static gboolean history_hst_value_changed_cb(GtkSpinButton *sb, History *p)
{
	gsize n;

	gdouble *x;
	gdouble *y;



	p->cfg->n_hst = gtk_spin_button_get_value_as_int(sb);

	/* XXX for now, use the same spb to set the height of the waterfall */
	p->cfg->wf_n_max = p->cfg->n_hst;

	if (!p->cfg->n_hst) {
		history_clear_hst(p);

		xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
		p->cfg->r_hst = NULL;

		goto exit;
	}

	/* always drop the "last data" indicator */
	xyplot_drop_graph(p->cfg->plot, p->cfg->r_lst);
	p->cfg->r_lst = NULL;

	n = p->cfg->hst_idx->len;
	if (!n)
		goto exit;/* empty */

	if (p->cfg->n_hst <= n) {
		g_array_remove_range(p->cfg->hst_idx, 0, n - p->cfg->n_hst);
		g_array_remove_range(p->cfg->hst_pwr, 0, n - p->cfg->n_hst);
	}

	/* create new graph */
	x = (gdouble *) g_memdup2(p->cfg->hst_idx->data,
				  p->cfg->hst_idx->len * sizeof(gdouble));
	y = (gdouble *) g_memdup2(p->cfg->hst_pwr->data,
				  p->cfg->hst_pwr->len * sizeof(gdouble));

	xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
	p->cfg->r_hst = xyplot_add_graph(p->cfg->plot, x, y, NULL,
					 p->cfg->hst_idx->len,
					 g_strdup_printf("History"));
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_hst, p->cfg->s_hst);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_hst, p->cfg->c_hst);
exit:
	return TRUE;
}


static gboolean history_wf_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
	History *p;

	GdkPixbuf *pbuf = NULL;

	GtkAllocation allocation;


	p = HISTORY(data);

	if (!p->cfg->wf_pb)
		return FALSE;


	gtk_widget_get_allocation(w, &allocation);

	pbuf = gdk_pixbuf_scale_simple(p->cfg->wf_pb,
				       allocation.width, allocation.height - 2,
				       GDK_INTERP_NEAREST);

	gdk_cairo_set_source_pixbuf(cr, pbuf, 0, 0);
	cairo_paint(cr);

	g_object_unref(pbuf);

	return FALSE;
}



/**
 * @brief create vertical history control bar
 */

static GtkWidget *history_sidebar_new(History *p)
{
	GtkGrid *grid;

	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());


	w = gtk_label_new("Power History");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 0, 0, 2, 1);

	w = gtk_spin_button_new_with_range(0, 10000, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->cfg->n_hst);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(grid, w, 0, 1, 2, 1);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(history_hst_value_changed_cb), p);

	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "HiSteps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Dashed Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Bézier");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Circles");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Squares");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Impulses");
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 6);	/* default circles */
	gtk_grid_attach(grid, w, 0, 2, 2, 1);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(history_hst_style_changed), p);


	w = gtk_color_button_new_with_rgba(&p->cfg->c_hst);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(w), TRUE);
	gtk_grid_attach(grid, w, 0, 3, 1, 1);
	g_signal_connect(w, "color-set",
			 G_CALLBACK(history_hst_colour_set_cb), p);


	w = gtk_button_new_with_label("Clear");
	gtk_widget_set_halign(w, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, w, 1, 3, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(history_reset_hst_cb), p);

	w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach(grid, w, 0, 4, 2, 1);


	return GTK_WIDGET(grid);
}



static void gui_create_history_controls(History *p)
{
	GtkWidget *w;

	GtkWidget *hbox;
	GtkWidget *vbox;


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

	w = xyplot_new();
	gtk_box_pack_start(GTK_BOX(hbox), w, TRUE, TRUE, 0);
	p->cfg->plot = w;

	xyplot_set_xlabel(p->cfg->plot, "Relative Sample Time [s]");
	xyplot_set_ylabel(p->cfg->plot, "Average Temperature / Bin [K]");

	w = history_sidebar_new(p);
	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(p), hbox, TRUE, TRUE, 0);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(p), hbox, TRUE, TRUE, 0);

	w = gtk_frame_new("Spectral Waterfall");
	g_object_set(w, "margin", 6, NULL);

	g_object_set(GTK_WIDGET(p->cfg->wf_da), "margin", 12, NULL);
	gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(p->cfg->wf_da));

	gtk_box_pack_start(GTK_BOX(hbox), w, TRUE, TRUE, 0);

if (0) {
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 6);
	w = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0., 1., 0.01);
	gtk_range_set_value(GTK_RANGE(w), p->cfg->th_lo);
	gtk_range_set_inverted(GTK_RANGE(w), TRUE);
	p->cfg->s_lo = GTK_SCALE(w);
	g_signal_connect(G_OBJECT(w), "value-changed",
			 G_CALLBACK(history_wf_slide_lo_value_changed), p);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	w = gtk_label_new("Lo");
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 6);
	w = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0., 1., 0.01);
	gtk_range_set_value(GTK_RANGE(w), p->cfg->th_hi);
	gtk_range_set_inverted(GTK_RANGE(w), TRUE);
	p->cfg->s_hi = GTK_SCALE(w);
	g_signal_connect(G_OBJECT(w), "value-changed",
			 G_CALLBACK(history_wf_slide_hi_value_changed), p);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	w = gtk_label_new("Hi");
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);
}


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, TRUE, 6);
	w = gtk_scale_new(GTK_ORIENTATION_VERTICAL, NULL);
	gtk_range_set_inverted(GTK_RANGE(w), TRUE);
	p->cfg->s_min = GTK_SCALE(w);
	gtk_scale_set_draw_value(GTK_SCALE(w), FALSE);
	g_signal_connect(G_OBJECT(w), "value-changed",
			 G_CALLBACK(history_wf_slide_min_value_changed), p);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	w = gtk_label_new("Lvl");
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, TRUE, 0);
}


/**
 * @brief destroy signal handler
 */

static gboolean history_destroy(GtkWidget *w, void *data)
{
	History *p;


	p = HISTORY(w);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_spd);

	g_timer_destroy(p->cfg->timer);

	return TRUE;
}


/**
 * @brief initialise the History class
 */

static void history_class_init(HistoryClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the History widget
 */

static void history_init(History *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_HISTORY(p));

	p->cfg = history_get_instance_private(p);

	p->cfg->timer   = g_timer_new();
	p->cfg->hst_idx = g_array_new(FALSE, FALSE, sizeof(gint64));
	p->cfg->hst_pwr = g_array_new(FALSE, FALSE, sizeof(gdouble));

	p->cfg->n_hst = HISTORY_DEFAULT_HST_LEN;
	p->cfg->r_hst = NULL;
	p->cfg->s_hst = IMPULSES;
	p->cfg->c_hst = COLOR_WHITE;

	p->cfg->wf_pb = NULL;
	p->cfg->wf_da = GTK_DRAWING_AREA(gtk_drawing_area_new());

	p->cfg->th_lo = 0.01;
	p->cfg->th_hi = 0.99;

	p->cfg->wf_n_max = HISTORY_DEFAULT_HST_LEN;

	p->cfg->refresh = 1.0 / HISTORY_REFRESH_HZ_CAP;

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);

	gui_create_history_controls(p);

	p->cfg->id_spd = g_signal_connect(sig_get_instance(), "pr-spec-data",
				G_CALLBACK(history_handle_pr_spec_data),
				(gpointer) p);

	g_signal_connect(p, "destroy", G_CALLBACK(history_destroy), NULL);

	g_signal_connect(G_OBJECT(p->cfg->wf_da), "draw",
			 G_CALLBACK(history_wf_draw), (gpointer) p);

	g_timer_start(p->cfg->timer);
}


/**
 * @brief create a new History widget
 */

GtkWidget *history_new(void)
{
	History *history;


	history = g_object_new(TYPE_HISTORY, NULL);

	return GTK_WIDGET(history);
}
