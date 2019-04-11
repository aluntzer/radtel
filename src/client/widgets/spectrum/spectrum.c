/**
 * @file    widgets/spectrum/spectrum.c
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
 * @brief a widget to control the spectrum position
 *
 */


#include <spectrum.h>
#include <spectrum_cfg.h>
#include <xyplot.h>

#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <cmd.h>

#include <levmar.h>
#include <fitfunc.h>
#include <math.h>

G_DEFINE_TYPE_WITH_PRIVATE(Spectrum, spectrum, GTK_TYPE_BOX)

#define SPECTRUM_DEFAULT_AVG_LEN 10
#define SPECTRUM_DEFAULT_PER_LEN 10



/**
 * @brief handle connected
 */

static void spectrum_connected(gpointer instance, gpointer data)
{
	/* fetch the config */
	cmd_spec_acq_cfg_get(PKT_TRANS_ID_UNDEF);
}


/**
 * @brief signal handler for toggle switch
 */

static gboolean spectrum_acq_toggle_cb(GtkWidget *w,
					 gboolean state, gpointer data)
{
	if (gtk_switch_get_active(GTK_SWITCH(w)))
		cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
	else
		cmd_spec_acq_disable(PKT_TRANS_ID_UNDEF);

	return TRUE;
}



/**
 * @brief helper function to change to state of our acquistion toggle button
 *	  without it emitting the "toggle" signal
 */

static void spectrum_acq_toggle_button(GtkSwitch *s, gboolean state)
{
	const GCallback cb = G_CALLBACK(spectrum_acq_toggle_cb);


	/* block/unblock the state-set handler of the switch for all copies of
	 * the widget, so we can change the state without emitting a signal
	 */

	g_signal_handlers_block_matched(s, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	gtk_switch_set_state(s, state);

	g_signal_handlers_unblock_matched(s, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
}


/**
 * @brief signal handler for acquisition button "on" status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

gboolean spectrum_acq_cmd_spec_acq_enable(gpointer instance, gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);

	spectrum_acq_toggle_button(p->cfg->sw_acq, TRUE);

	return FALSE;
}


/**
 * @brief signal handler for acquisition button "off" status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

gboolean spectrum_acq_cmd_spec_acq_disable(gpointer instance, gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);

	spectrum_acq_toggle_button(p->cfg->sw_acq, FALSE);

	return FALSE;
}


/**
 * @brief handle status acq
 *
 * @note we use the acq status to update the state of the acqusition control
 *       button, because there is no status-get command and I don't want to add
 *       one because the spectrometer backend is supposed to push the acq status
 *       anyway
 */

static void spectrum_handle_pr_status_acq(gpointer instance,
					  const struct status *s,
					  gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);

	if (!s->busy)
		return;

	if (!gtk_switch_get_active(p->cfg->sw_acq))
		spectrum_acq_cmd_spec_acq_enable(instance, data);
}



/**
 * @brief handle spectral acquisition configuration data
 */

static void spectrum_handle_pr_spec_acq_cfg(gpointer instance,
					    const struct spec_acq_cfg *acq,
					    gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);

	p->cfg->acq = (*acq);
}


/**
 * @brief plot a gaussian
 */

static void spectrum_plot_gaussian(GtkWidget *w, gdouble par[4], size_t n,
				   struct fitdata *fit)
{
	size_t i;


	gdouble *x;
	gdouble *y;

	gdouble pmin, pmax;
	gdouble smin, smax, symin, symax;

	const GdkRGBA red = {1.0, 0.0, 0.0, 1.0};


	x = g_malloc(n * sizeof(gdouble));
	y = g_malloc(n * sizeof(gdouble));


	xyplot_get_data_axis_range(w, &pmin, &pmax, NULL, NULL);
	xyplot_get_sel_axis_range(w, &smin, &smax, &symin,&symax);


	for (i = 0; i < n; i++) {
		x[i] = pmin + ((gdouble) i) * (pmax - pmin) / ((double) n);
		y[i] = gaussian(par, x[i]);
	}


	/* graph outside box */
	xyplot_drop_graph(w, fit->plt_ref_in);
	fit->plt_ref_in = xyplot_add_graph(w, x, y, NULL, n,
					   g_strdup_printf("FIT"));

	xyplot_set_graph_style(w, fit->plt_ref_in, DASHES);
	xyplot_set_graph_rgba(w, fit->plt_ref_in, red);


	/* graph inside box */
	x = g_malloc(n * sizeof(gdouble));
	y = g_malloc(n * sizeof(gdouble));

	for (i = 0; i < n; i++) {
		x[i] = smin + ((gdouble) i) * (smax - smin) / ((double) n);
		y[i] = gaussian(par, x[i]);

		if ((y[i] > symax) || (y[i] < symin)) {
			x[i] = NAN;
			y[i] = NAN;
		}

	}

	xyplot_drop_graph(w, fit->plt_ref_out);
	fit->plt_ref_out = xyplot_add_graph(w, x, y, NULL, n, g_strdup_printf("FIT"));

	xyplot_set_graph_style(w, fit->plt_ref_out, NAN_LINES);
	xyplot_set_graph_rgba(w, fit->plt_ref_out, red);

	xyplot_redraw(w);
}



/**
 * @brief update remote frequency setting on coord signal
 */

static gboolean spectrum_plt_clicked_coord(GtkWidget *w, gdouble x,
					   __attribute__((unused)) gdouble y,
					   gpointer data)
{
	uint64_t f, bw2;

	gchar *msg;

	struct spec_acq_cfg acq;

	Spectrum *p;



	p = SPECTRUM(data);


	f = (uint64_t) (x * 1e6); /* to Hz */

	acq = p->cfg->acq;

	bw2 = (acq.freq_stop_hz - acq.freq_start_hz) / 2;


	/* we do not really care whether the configuration is valid,
	 * we'll just try to set it
	 */

	acq.freq_start_hz = f - bw2;
	acq.freq_stop_hz  = f + bw2;

	cmd_spec_acq_cfg(PKT_TRANS_ID_UNDEF,
			 acq.freq_start_hz, acq.freq_stop_hz, acq.bw_div,
			 acq.bin_div, 0, 0);

	msg = g_strdup_printf("Acquisition frequency range update: "
			      "%6.2f - %6.2f MHz",
			      (gdouble) acq.freq_start_hz * 1e-6,
			      (gdouble) acq.freq_stop_hz * 1e-6);

	sig_status_push(msg);

	g_free(msg);

	return TRUE;
}


/**
 * @brief fit selection box callback
 */

static gboolean spectrum_plt_fitbox_selected(GtkWidget *w, gpointer data)
{
	int ret;
	size_t n;

	gdouble *x;
	gdouble *y;
	gchar *lbl;

	gdouble par[4];

	struct fitdata *fit;


	if (!data)
		return TRUE;

	fit = (struct fitdata *) data;

	n = xyplot_get_selection_data(w, &x, &y, NULL);
	if (!n) {
		xyplot_drop_graph(w, fit->plt_ref_in);
		xyplot_drop_graph(w, fit->plt_ref_out);
		return TRUE;
	}

	gaussian_guess_param(par, x, y, n);

	ret = gaussian_fit(par, x, y, n);

	g_free(x);
	g_free(y);

	if (ret)
		return TRUE;


	lbl = g_strdup_printf("Last Fit Results:\n\n"
			      "<tt>"
			      "Peak:\n"
			      "<b>%8.2f [MHz]</b>\n\n"
			      "Height:\n"
			      "<b>%8.2f [K]</b>\n\n"
			      "FWHM:\n"
			      "<b>%8.2f [deg]</b>\n\n"
			      "</tt>",
			      par[2], par[0],
			      fabs(par[1]) * 2.0 * sqrt(log(2.0)));

	gtk_label_set_markup(fit->fitpar, lbl);
	g_free(lbl);

	/* XXX plot a fixed 100 points for now */
	spectrum_plot_gaussian(w, par, 100, fit);

	return TRUE;
}


/**
 * @brief average colour set callback
 */

static void spectrum_avg_colour_set_cb(GtkColorButton *w, Spectrum *p)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &p->cfg->c_avg);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_avg, p->cfg->c_avg);
	xyplot_redraw(p->cfg->plot);
}

/**
 * @brief data colour set callback
 */

static void spectrum_per_colour_set_cb(GtkColorButton *w, Spectrum *p)
{
	GdkRGBA c;

	GList *elem;


	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &p->cfg->c_per);

	/* flip colours, but leave alpha intact */
	for (elem = p->cfg->per; elem; elem = elem->next) {
		xyplot_get_graph_rgba(p->cfg->plot, elem->data, &c);
		c.red   = p->cfg->c_per.red;
		c.green = p->cfg->c_per.green;
		c.blue  = p->cfg->c_per.blue;
		xyplot_set_graph_rgba(p->cfg->plot, elem->data, c);
	}

	/* redraw plot */
	xyplot_redraw(p->cfg->plot);
}



static void spectrum_set_plot_style(gint active, enum xyplot_graph_style *s)
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
	default:
		break;
	}
}

static void spectrum_data_style_changed(GtkComboBox *cb, Spectrum *p)
{
	GList *elem;


	spectrum_set_plot_style(gtk_combo_box_get_active(cb), &p->cfg->s_per);

	for (elem = p->cfg->per; elem; elem = elem->next)
		xyplot_set_graph_style(p->cfg->plot, elem->data, p->cfg->s_per);

	xyplot_redraw(p->cfg->plot);
}


static void spectrum_avg_style_changed(GtkComboBox *cb, Spectrum *p)
{
	spectrum_set_plot_style(gtk_combo_box_get_active(cb), &p->cfg->s_avg);
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_avg, p->cfg->s_avg);
	xyplot_redraw(p->cfg->plot);
}



/**
 * @brief g_free() a spectrum and all its contents
 */

static void spectrum_free(struct spectrum *sp)
{
	if (!sp)
		return;

	g_free(sp->x);
	g_free(sp->y);
	g_free(sp);
}



/**
 * @brief drop and fee list of data sets
 */

static void spectrum_drop_data(Spectrum *p)
{
	GList *elem;

	for (elem = p->cfg->per; elem; elem = elem->next)
		xyplot_drop_graph(p->cfg->plot, elem->data);

	g_list_free(p->cfg->per);
	p->cfg->per = NULL;
}


/**
 * @brief append new data set to list of running averages
 */

static void spectrum_append_data(Spectrum *p, struct spectrum *sp)
{

	GdkRGBA c;

	gdouble alpha_frac;

	void *ref;

	GList *elem;
	GList *tmp;




	/* persistence == 0 -> infinite */
	if (p->cfg->n_per) {

		/* drop the oldest */
		if (g_list_length(p->cfg->per) == p->cfg->n_per) {
			elem = g_list_first(p->cfg->per);
			xyplot_drop_graph(p->cfg->plot, elem->data);
			p->cfg->per = g_list_delete_link(p->cfg->per, elem);
		}

		alpha_frac = p->cfg->c_per.alpha * 1.0 / ((gdouble) (p->cfg->n_per));


		for (elem = p->cfg->per; elem; elem = elem->next) {
			ref = elem->data;

			if (xyplot_get_graph_rgba(p->cfg->plot, ref, &c)) {
				/* graph is no more, drop */
				tmp = elem->next;
				p->cfg->per = g_list_delete_link(p->cfg->per, elem);

				if (tmp && tmp->prev) {
					elem = tmp->prev; /* take one step back */
					continue;
				}
			}

			c.alpha -= alpha_frac;

			/* could happen if the base alpha was changed by user,
			 * just set it to low
			 */
			if (c.alpha < 0.0)
				c.alpha = alpha_frac;

			xyplot_set_graph_rgba(p->cfg->plot, ref, c);
		}
	}


	ref = xyplot_add_graph(p->cfg->plot, sp->x, sp->y, NULL, sp->n,
			       g_strdup_printf("SPECTRUM"));

	xyplot_set_graph_style(p->cfg->plot, ref, p->cfg->s_per);
	xyplot_set_graph_rgba(p->cfg->plot, ref, p->cfg->c_per);
	xyplot_redraw(p->cfg->plot);

	p->cfg->per = g_list_append(p->cfg->per, ref);
}


/**
 * @brief drop and free list of running averages
 */

static void spectrum_drop_avg(Spectrum *p)
{
	GList *elem;

	for (elem = p->cfg->avg; elem; elem = elem->next)
		spectrum_free((struct spectrum *) elem->data);

	g_list_free(p->cfg->avg);
	p->cfg->avg  = NULL;
}


/**
 * @brief append new data set to list of running averages
 */

static void spectrum_append_avg(Spectrum *p, struct spectrum *sp)
{
	GList *elem;

	gsize i;
	gsize n;

	gdouble inv;

	gdouble *x;
	gdouble *y;

	struct spectrum *s;



	/* remove the old graph */
	xyplot_drop_graph(p->cfg->plot, p->cfg->r_avg);
	p->cfg->r_avg = NULL;

	/* averages are disabled, free spectrum and return */
	if (!p->cfg->n_avg) {
		spectrum_free(sp);
		return;
	}


	if (g_list_first(p->cfg->avg)) {
		elem = g_list_first(p->cfg->avg);
		s = (struct spectrum *) elem->data;

		if (s->n != sp->n)
			spectrum_drop_avg(p);
		else if (s->x[0] != sp->x[0])
			spectrum_drop_avg(p);
		else if (s->x[s->n - 1] != sp->x[sp->n - 1])
			spectrum_drop_avg(p);

		/* otherwise we'll just run with it */
	}


	n = g_list_length(p->cfg->avg);

	/* drop the oldest */
	if (n == p->cfg->n_avg) {
		elem = g_list_first(p->cfg->avg);
		spectrum_free((struct spectrum *) elem->data);
		p->cfg->avg = g_list_delete_link(p->cfg->avg, elem);
	} else {
		n++;
	}


	/* compute new average */
	x = g_memdup(sp->x, sp->n * sizeof(gdouble));
	y = g_memdup(sp->y, sp->n * sizeof(gdouble));

	for (elem = p->cfg->avg; elem; elem = elem->next) {
		s = (struct spectrum *) elem->data;
		for (i = 0; i < sp->n; i++)
			y[i] += s->y[i];


	}

	inv = 1.0 / (gdouble) n;

	for (i = 0; i < sp->n; i++)
		y[i] *= inv;

	/* now append new */
	p->cfg->avg = g_list_append(p->cfg->avg, sp);


	p->cfg->r_avg = xyplot_add_graph(p->cfg->plot, x, y, NULL, sp->n,
					 g_strdup_printf("AVERAGE"));
	xyplot_set_graph_style(p->cfg->plot, p->cfg->r_avg, p->cfg->s_avg);
	xyplot_set_graph_rgba(p->cfg->plot, p->cfg->r_avg, p->cfg->c_avg);
	xyplot_redraw(p->cfg->plot);
}


/**
 * @brief handle capabilities data
 */

static void spectrum_handle_pr_spec_data(gpointer instance,
					 const struct spec_data *s,
					 gpointer data)
{

	uint64_t i;
	uint64_t f;

	gdouble *frq;
	gdouble *amp;

	struct spectrum *sp;

	Spectrum *p;

	p = SPECTRUM(data);


	if (!s->n)
		return;

	frq = g_malloc(s->n * sizeof(gdouble));
	amp = g_malloc(s->n * sizeof(gdouble));

	for (i = 0, f = s->freq_min_hz; i < s->n; i++, f += s->freq_inc_hz) {
		frq[i] = (gdouble) f * 1e-6;		/* Hz to Mhz */
		amp[i] = (gdouble) s->spec[i] * 0.001;	/* mK to K */
	}


	/* everyone gets a copy of the data */
	sp    = (struct spectrum *) g_malloc(sizeof(struct spectrum));
	sp->x = g_memdup(frq, s->n * sizeof(gdouble));
	sp->y = g_memdup(amp, s->n * sizeof(gdouble));
	sp->n = s->n;
	spectrum_append_avg(p, sp);

	sp    = (struct spectrum *) g_malloc(sizeof(struct spectrum));
	sp->x = frq;
	sp->y = amp;
	sp->n = s->n;

	spectrum_append_data(p, sp);
}


/**
 * @brief button reset average callback
 */

static void spectrum_reset_avg_cb(GtkWidget *w, Spectrum *p)
{
	spectrum_drop_avg(p);

	xyplot_drop_graph(p->cfg->plot, p->cfg->r_avg);
	p->cfg->r_avg = NULL;
}


/**
 * @brief handle running average length change
 */

static gboolean spectrum_avg_value_changed_cb(GtkSpinButton *sb, Spectrum *p)
{
	gsize i;
	gsize n;

	GList *elem;

	struct spectrum *sp;



	p->cfg->n_avg = gtk_spin_button_get_value_as_int(sb);

	if (!p->cfg->n_avg) {
		spectrum_drop_avg(p);

		xyplot_drop_graph(p->cfg->plot, p->cfg->r_avg);
		p->cfg->r_avg = NULL;
		goto exit;
	}

	n = g_list_length(p->cfg->avg);
	if (!n)
		goto exit;/* empty */

	if (n < p->cfg->n_avg)
		goto exit; /* underfilled */

	/* drop old data sets to reach the newly configured value */
	for (i = 0; i < n - p->cfg->n_avg; i++) {
		elem = g_list_first(p->cfg->avg);
		spectrum_free((struct spectrum *) elem->data);
		p->cfg->avg = g_list_delete_link(p->cfg->avg, elem);
	}

	elem = g_list_last(p->cfg->avg);

	if (elem) {
		sp = (struct spectrum *) elem->data;
		p->cfg->avg = g_list_delete_link(p->cfg->avg, elem);
		spectrum_append_avg(p, sp); /* redraw with current set */
	}

exit:
	return TRUE;
}


/**
 * @brief button reset persistence callback
 */

static void spectrum_reset_per_cb(GtkWidget *w, Spectrum *p)
{
	spectrum_drop_data(p);
}


/**
 * @brief handle data persistence length change
 */

static gboolean spectrum_per_value_changed_cb(GtkSpinButton *sb, Spectrum *p)
{
	gsize i;
	gsize n;

	GList *elem;



	p->cfg->n_per = gtk_spin_button_get_value_as_int(sb);

	if (!p->cfg->n_per) {
		spectrum_drop_data(p);
		goto exit;
	}

	n = g_list_length(p->cfg->per);
	if (!n)
		goto exit;/* empty */

	if (n < p->cfg->n_per)
		goto exit; /* underfilled */

	/* drop old data sets to reach the newly configured value */
	for (i = 0; i < n - p->cfg->n_per; i++) {
		elem = g_list_first(p->cfg->per);
		xyplot_drop_graph(p->cfg->plot, elem->data);
		p->cfg->per = g_list_delete_link(p->cfg->per, elem);
	}

exit:
	return TRUE;
}


/**
 * @brief create vertical spectrum control bar
 */

static GtkWidget *spectrum_sidebar_new(Spectrum *p)
{
	GtkGrid *grid;

	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());


	w = gtk_label_new("ACQ");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 0, 0, 1, 1);


	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable acquisition");
	gtk_widget_set_halign(w, GTK_ALIGN_END);

	p->cfg->sw_acq = GTK_SWITCH(w);
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(spectrum_acq_toggle_cb), p);

	gtk_grid_attach(grid, w, 1, 0, 1, 1);


	w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach(grid, w, 0, 1, 2, 1);


	w = gtk_label_new("Data");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	w = gtk_spin_button_new_with_range(0, 100, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->cfg->n_per);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(grid, w, 0, 3, 2, 1);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(spectrum_per_value_changed_cb), p);

	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "HiSteps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Dashed Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Bézier");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Circles");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Squares");
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 4);	/* default circles */
	gtk_grid_attach(grid, w, 0, 4, 2, 1);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(spectrum_data_style_changed), p);


	w = gtk_color_button_new_with_rgba(&p->cfg->c_per);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(w), TRUE);
	gtk_grid_attach(grid, w, 0, 5, 1, 1);
	g_signal_connect(w, "color-set",
			 G_CALLBACK(spectrum_per_colour_set_cb), p);


	w = gtk_button_new_with_label("Clear");
	gtk_widget_set_halign(w, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, w, 1, 5, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(spectrum_reset_per_cb), p);

	w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach(grid, w, 0, 6, 2, 1);



	w = gtk_label_new("Average");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 0, 7, 1, 1);

	w = gtk_spin_button_new_with_range(0, 100, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->cfg->n_avg);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(grid, w, 0, 8, 2, 1);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(spectrum_avg_value_changed_cb), p);


	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "HiSteps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Dashed Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Bézier");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Circles");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Squares");
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);	/* default HiSteps */
	gtk_grid_attach(grid, w, 0, 9, 2, 1);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(spectrum_avg_style_changed), p);

	w = gtk_color_button_new_with_rgba(&p->cfg->c_avg);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(w), TRUE);
	gtk_grid_attach(grid, w, 0, 10, 1, 1);
	g_signal_connect(w, "color-set",
			 G_CALLBACK(spectrum_avg_colour_set_cb), p);


	w = gtk_button_new_with_label("Clear");
	gtk_widget_set_halign(w, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, w, 1, 10, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(spectrum_reset_avg_cb), p);

	w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach(grid, w, 0, 11, 2, 1);

	w = gtk_label_new("");
	gtk_grid_attach(grid, w, 0, 12, 2, 1);
	p->cfg->fit.fitpar = GTK_LABEL(w);


	return GTK_WIDGET(grid);
}



static void gui_create_spectrum_controls(Spectrum *p)
{
	GtkWidget *w;


	w = xyplot_new();
	gtk_box_pack_start(GTK_BOX(p), w, TRUE, TRUE, 0);
	p->cfg->plot = w;

	xyplot_set_xlabel(p->cfg->plot, "Frequency [MHz]");
	xyplot_set_ylabel(p->cfg->plot, "Amplitude [K]");
	g_signal_connect(p->cfg->plot, "xyplot-fit-selection",
			 G_CALLBACK(spectrum_plt_fitbox_selected),
			 &p->cfg->fit);

	g_signal_connect(p->cfg->plot, "xyplot-clicked-xy-coord",
			 G_CALLBACK(spectrum_plt_clicked_coord),
			 p);

	w = spectrum_sidebar_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

}


/**
 * @brief destroy signal handler
 */

static gboolean spectrum_destroy(GtkWidget *w, void *data)
{
	Spectrum *p;


	p = SPECTRUM(w);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_spd);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_acq);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_ena);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_dis);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cfg);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_con);

	return TRUE;
}


/**
 * @brief initialise the Spectrum class
 */

static void spectrum_class_init(SpectrumClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the Spectrum widget
 */

static void spectrum_init(Spectrum *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_SPECTRUM(p));

	p->cfg = spectrum_get_instance_private(p);

	p->cfg->avg   = NULL;
	p->cfg->n_avg = SPECTRUM_DEFAULT_AVG_LEN;
	p->cfg->r_avg = NULL;
	p->cfg->s_avg = STAIRS;
	p->cfg->c_avg = COLOR_WHITE;

	p->cfg->per   = NULL;
	p->cfg->n_per = SPECTRUM_DEFAULT_PER_LEN;
	p->cfg->r_per = NULL;
	p->cfg->s_per = CIRCLES;
	p->cfg->c_per = COLOR_YELLOW_PHOS;

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_HORIZONTAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);

	gui_create_spectrum_controls(p);

	p->cfg->id_spd = g_signal_connect(sig_get_instance(), "pr-spec-data",
			 G_CALLBACK(spectrum_handle_pr_spec_data),
			 (gpointer) p);

	p->cfg->id_acq = g_signal_connect(sig_get_instance(), "pr-status-acq",
			 G_CALLBACK(spectrum_handle_pr_status_acq),
			 (gpointer) p);

	p->cfg->id_ena = g_signal_connect(sig_get_instance(),
			 "pr-spec-acq-enable",
			 G_CALLBACK(spectrum_acq_cmd_spec_acq_enable),
			 (gpointer) p);

	p->cfg->id_dis = g_signal_connect(sig_get_instance(),
			 "pr-spec-acq-disable",
			 G_CALLBACK(spectrum_acq_cmd_spec_acq_disable),
			 (gpointer) p);

	p->cfg->id_cfg = g_signal_connect(sig_get_instance(), "pr-spec-acq-cfg",
			 G_CALLBACK(spectrum_handle_pr_spec_acq_cfg),
			 (gpointer) p);

	p->cfg->id_con = g_signal_connect(sig_get_instance(), "connected",
			 G_CALLBACK(spectrum_connected),
			 (gpointer) p);

	g_signal_connect(p, "destroy", G_CALLBACK(spectrum_destroy), NULL);

}


/**
 * @brief create a new Spectrum widget
 */

GtkWidget *spectrum_new(void)
{
	Spectrum *spectrum;


	spectrum = g_object_new(TYPE_SPECTRUM, NULL);

	return GTK_WIDGET(spectrum);
}
