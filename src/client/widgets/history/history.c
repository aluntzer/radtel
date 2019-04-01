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

G_DEFINE_TYPE_WITH_PRIVATE(History, history, GTK_TYPE_BOX)

#define HISTORY_DEFAULT_HST_LEN 100
#define HISTORY_DEFAULT_HST_GAP   5


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
	p->cfg->idx = 0;
	g_array_set_size(p->cfg->hst_idx, 0);
	g_array_set_size(p->cfg->hst_pwr, 0);
}


/**
 * @brief append new data set to list of running averages
 */

static void history_append_hst(History *p, const gdouble *amp, gsize len)
{
	gsize i;
	gsize n;

	gdouble pwr = 0.0;

	gdouble *x;
	gdouble *y;

	gdouble tmp;




	/* remove the old graph */
	xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
	p->cfg->r_hst = NULL;

	/* history is disabled? */
	if (!p->cfg->n_hst)
		return;

	n = p->cfg->hst_idx->len;

	/* drop the oldest */
	if (n == p->cfg->n_hst) {
		g_array_remove_index(p->cfg->hst_idx, 0);
		g_array_remove_index(p->cfg->hst_pwr, 0);
	}

	/* we could use a date later on, but this needs plot support first;
	 * for now, we just rotate an index
	 */
	if (p->cfg->idx >= p->cfg->n_hst)
		p->cfg->idx = 0;
	else
		p->cfg->idx++;

	for (i = 0; i < len; i++)
		pwr += amp[i];

	pwr /= (gdouble) len;	/* average power */

	tmp = (gdouble) p->cfg->idx;
	g_array_append_val(p->cfg->hst_idx, tmp);
	g_array_append_val(p->cfg->hst_pwr, pwr);

	/* create new graph */
	x = (gdouble *) g_memdup(p->cfg->hst_idx->data,
				 p->cfg->hst_idx->len * sizeof(gdouble));
	y = (gdouble *) g_memdup(p->cfg->hst_pwr->data,
				 p->cfg->hst_pwr->len * sizeof(gdouble));


	p->cfg->r_hst = xyplot_add_graph(p->cfg->plot, x, y, NULL,
					 p->cfg->hst_idx->len,
					 g_strdup_printf("History"));
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_hst, p->cfg->s_hst);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_hst, p->cfg->c_hst);

	/* also redraws */
	xyplot_set_range_x(p->cfg->plot, 0.0, (gdouble) p->cfg->n_hst);
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
}


/**
 * @brief handle running average length change
 */

static gboolean history_hst_value_changed_cb(GtkSpinButton *sb, History *p)
{
	gsize n;
	gsize tmp;

	gdouble *x;
	gdouble *y;



	p->cfg->n_hst = gtk_spin_button_get_value_as_int(sb);

	if (!p->cfg->n_hst) {
		history_clear_hst(p);

		xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
		p->cfg->r_hst = NULL;
		goto exit;
	}

	n = p->cfg->hst_idx->len;
	if (!n)
		goto exit;/* empty */

	if (p->cfg->n_hst <= n) {
		g_array_remove_range(p->cfg->hst_idx, 0, n - p->cfg->n_hst);
		g_array_remove_range(p->cfg->hst_pwr, 0, n - p->cfg->n_hst);
	}

	/* create new graph */
	x = (gdouble *) g_memdup(p->cfg->hst_idx->data,
				 p->cfg->hst_idx->len * sizeof(gdouble));
	y = (gdouble *) g_memdup(p->cfg->hst_pwr->data,
				 p->cfg->hst_pwr->len * sizeof(gdouble));

	xyplot_drop_graph(p->cfg->plot, p->cfg->r_hst);
	p->cfg->r_hst = xyplot_add_graph(p->cfg->plot, x, y, NULL,
					 p->cfg->hst_idx->len,
					 g_strdup_printf("History"));
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_hst, p->cfg->s_hst);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_hst, p->cfg->c_hst);

	xyplot_set_range_x(p->cfg->plot, 0.0, (gdouble) p->cfg->n_hst);

exit:
	return TRUE;
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

	w = gtk_spin_button_new_with_range(0, 100, 1);
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


	w = xyplot_new();
	gtk_box_pack_start(GTK_BOX(p), w, TRUE, TRUE, 0);
	p->cfg->plot = w;

	xyplot_set_xlabel(p->cfg->plot, "Sample");
	xyplot_set_ylabel(p->cfg->plot, "Average Flux [K]");

	w = history_sidebar_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
}


/**
 * @brief destroy signal handler
 */

static gboolean history_destroy(GtkWidget *w, void *data)
{
	History *p;


	p = HISTORY(w);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_spd);

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

	p->cfg->hst_idx = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->hst_pwr = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->idx = INT_MAX;

	p->cfg->n_hst = HISTORY_DEFAULT_HST_LEN;
	p->cfg->r_hst = NULL;
	p->cfg->s_hst = IMPULSES;
	p->cfg->c_hst = COLOR_WHITE;


	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_HORIZONTAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);

	gui_create_history_controls(p);

	p->cfg->id_spd = g_signal_connect(sig_get_instance(), "pr-spec-data",
				G_CALLBACK(history_handle_pr_spec_data),
				(gpointer) p);

	g_signal_connect(p, "destroy", G_CALLBACK(history_destroy), NULL);
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
