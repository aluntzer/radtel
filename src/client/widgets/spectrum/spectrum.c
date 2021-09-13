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

#include <glib/gstdio.h>
#include <locale.h>

G_DEFINE_TYPE_WITH_PRIVATE(Spectrum, spectrum, GTK_TYPE_BOX)

#define SPECTRUM_DEFAULT_AVG_LEN 10
#define SPECTRUM_DEFAULT_PER_LEN 10

#define SPECTRUM_REFRESH_HZ_CAP  30.

#define SPECTRUM_REFRESH_AVG_LEN 10.

#define SPECTRUM_REFRESH_DUTY_CYCLE 0.8


/**
 * @brief redraw the plot if the configured time has expired
 */

static void spectrum_plot_try_refresh(GtkWidget *w, Spectrum *p)
{
	gdouble elapsed;

	const double n  = 1.0 / SPECTRUM_REFRESH_AVG_LEN;
	const double n1 = SPECTRUM_REFRESH_AVG_LEN - 1.0;


	g_timer_stop(p->cfg->timer);

	elapsed = g_timer_elapsed(p->cfg->timer, NULL);

	if (elapsed > p->cfg->refresh) {

		/* reuse the timer to measure drawing time */
		g_timer_start(p->cfg->timer);
		xyplot_redraw(w);
		g_timer_stop(p->cfg->timer);

		elapsed = g_timer_elapsed(p->cfg->timer, NULL);

		elapsed /= SPECTRUM_REFRESH_DUTY_CYCLE;

		/* adapt refresh rate */
		p->cfg->refresh = (p->cfg->refresh * n1 + elapsed) * n;

		if (p->cfg->refresh < (1.0 / SPECTRUM_REFRESH_HZ_CAP))
			p->cfg->refresh = (1.0 / SPECTRUM_REFRESH_HZ_CAP);



		g_timer_start(p->cfg->timer);
	} else {
		g_timer_continue(p->cfg->timer);
	}

}



static gdouble spectrum_convert_x2(gdouble x, gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);


	return -(vlsr(p->cfg->pos_equ, 0.0) + doppler_vel(x, p->cfg->freq_ref_mhz));
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
 * @brief handle connected
 */

static void spectrum_connected(gpointer instance, gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);

	/* set toggle default OFF */
	spectrum_acq_toggle_button(p->cfg->sw_acq, FALSE);
	/* fetch the config */
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
	cmd_getpos_azel(PKT_TRANS_ID_UNDEF);
	cmd_spec_acq_cfg_get(PKT_TRANS_ID_UNDEF);
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
 * @brief handle capabilities data
 */

static void spectrum_handle_pr_capabilities(gpointer instance,
					    const struct capabilities *c,
					    gpointer data)
{
	Spectrum *p;


	p = SPECTRUM(data);


	p->cfg->lat = (gdouble) c->lat_arcsec / 3600.0;
	p->cfg->lon = (gdouble) c->lon_arcsec / 3600.0;
}


/**
 * @brief handle position data
 */

static gboolean spectrum_handle_getpos_azel_cb(gpointer instance,
					       struct getpos *pos,
					       gpointer data)
{
	struct coord_horizontal hor;

	Spectrum *p;


	p = SPECTRUM(data);


	hor.az = (double) pos->az_arcsec / 3600.0;
	hor.el = (double) pos->el_arcsec / 3600.0;


	p->cfg->pos_hor = hor;
	p->cfg->pos_equ = horizontal_to_equatorial(hor,
						   p->cfg->lat, p->cfg->lon,
						   0.0);
	p->cfg->pos_gal = horizontal_to_galactic(hor, p->cfg->lat, p->cfg->lon);


	return TRUE;
}


static void spectrum_record_add(Spectrum *p, struct spectrum *sp)
{
	size_t i;

	time_t now;
	struct tm *t;


	if (!p->cfg->rec)
		return;

	if (!sp)
		return;


	setlocale(LC_ALL, "C");


	time(&now);
	t = localtime(&now);

	fprintf(p->cfg->rec, "%.4f %.4f %d %d %d %.4f %.4f %.4f %.4f "
			     "%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %d ",
			     p->cfg->lat, p->cfg->lon,
			     t->tm_year + 1900, t->tm_mon  + 1,
			     t->tm_mday, local_sidereal_time(p->cfg->lon),
			     p->cfg->pos_hor.az, p->cfg->pos_hor.el,
			     p->cfg->pos_equ.ra, p->cfg->pos_equ.dec,
			     p->cfg->pos_gal.lat, p->cfg->pos_gal.lon,
			     sp->x[0], sp->x[sp->n - 1],
			     spectrum_convert_x2(sp->x[0], p),
			     spectrum_convert_x2(sp->x[sp->n - 1], p),
			     p->cfg->freq_ref_mhz, sp->n);


	for (i = 0; i <  sp->n; i++)
		fprintf(p->cfg->rec, "%.4f ", sp->y[i]);

	fprintf(p->cfg->rec, "\n");

	setlocale(LC_ALL, "");
}


static void spectrum_record_start(Spectrum *p)
{
	GtkWidget *dia;
	GtkFileChooser *chooser;
	gint res;

	GtkWidget *win;

	gchar *fname;


	win = gtk_widget_get_toplevel(GTK_WIDGET(p));

	if (!GTK_IS_WINDOW(win)) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return;
	}

	fname = g_strdup_printf("record.dat");

	dia = gtk_file_chooser_dialog_new("Select Record File",
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

	gtk_file_chooser_set_current_folder(chooser,g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS));

	res = gtk_dialog_run(GTK_DIALOG(dia));

	g_free(fname);

	if (res == GTK_RESPONSE_ACCEPT) {

		fname = gtk_file_chooser_get_filename(chooser);


		p->cfg->rec = g_fopen(fname, "w");

		if (!p->cfg->rec) {
			GtkWidget *dia;
			GtkWindow * win;

			win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(p)));
			dia = gtk_message_dialog_new(win,
						     GTK_DIALOG_MODAL,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     "Could not open file %s",
						     fname);

			gtk_dialog_run(GTK_DIALOG(dia));
			gtk_widget_destroy(dia);
		}

		fprintf(p->cfg->rec,
			"# Format: LAT LON YEAR MONTH DAY LST AZ EL RA DE "
			"GLAT GLON FIRST_FREQ[MHz] LAST_FREQ[MHz] REF_FREQ[MHz]"
			"VRAD0[km/s] VRAD1[km/s] BINS "
			"Amplitude[K](1...N)\n");



		g_free(fname);
	}

	gtk_widget_destroy(dia);
}


static void spectrum_rec_button_toggle(GtkToggleButton *btn,
				       gpointer         data)
{
	Spectrum *p;

	GtkStyleContext *ctx;


	p = SPECTRUM(data);

	ctx = gtk_widget_get_style_context(GTK_WIDGET (btn));

	if (gtk_toggle_button_get_active(btn)) {
		spectrum_record_start(p);
		gtk_style_context_add_class(ctx, "destructive-action");
	} else {
		if (p->cfg->rec) {
			fclose(p->cfg->rec);
			p->cfg->rec = NULL;
		}
	}

	/* if failed or disabled, set inactive */
	if (!p->cfg->rec) {
		gtk_style_context_remove_class(ctx, "destructive-action");
		gtk_toggle_button_set_active(btn, FALSE);
	}


}


/**
 * @brief plot a gaussian
 */

static void spectrum_plot_gaussian(GtkWidget *w, gdouble par[4], size_t n,
				   struct fitdata *fit, Spectrum *p)
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

	spectrum_plot_try_refresh(w, p);
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

	Spectrum *p;


	p = SPECTRUM(data);


	if (!p)
		return TRUE;

	fit = &p->cfg->fit;

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
			      "<b>%8.2f [MHz]</b>\n"
			      "<b>%8.2f [km/s]</b>\n\n"
			      "Height:\n"
			      "<b>%8.2f [K]</b>\n\n"
			      "FWHM:\n"
			      "<b>%8.2f [MHz]</b>\n"
			      "<b>%8.2f [km/s]</b>\n\n"
			      "</tt>",
			      gaussian_peak(par),
			      spectrum_convert_x2(gaussian_peak(par), p),
			      gaussian_height(par),
			      gaussian_fwhm(par),
			      fabs(doppler_vel_relative(gaussian_fwhm(par),
							p->cfg->freq_ref_mhz)));

	gtk_label_set_markup(fit->fitpar, lbl);
	g_free(lbl);

	/* XXX plot a fixed 200 points for now */
	spectrum_plot_gaussian(w, par, 200, fit, p);

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
	case 6:
		(*s) = MARIO;
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

	spectrum_plot_try_refresh(p->cfg->plot, p);
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


	/* disable for persistence == 0 -> infinite */
#if 1
	if (!p->cfg->n_per)
		return;
#endif

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

	spectrum_plot_try_refresh(p->cfg->plot, p);

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

	spectrum_plot_try_refresh(p->cfg->plot, p);
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
	x = g_memdup2(sp->x, sp->n * sizeof(gdouble));
	y = g_memdup2(sp->y, sp->n * sizeof(gdouble));

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

	spectrum_plot_try_refresh(p->cfg->plot, p);
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

	struct spectrum *sp = NULL;

	Spectrum *p;

	p = SPECTRUM(data);


	if (!s->n)
		return;



	/* update positions */
	p->cfg->pos_equ = horizontal_to_equatorial(p->cfg->pos_hor,
						   p->cfg->lat, p->cfg->lon,
						   0.0);
	p->cfg->pos_gal = horizontal_to_galactic(p->cfg->pos_hor,
						 p->cfg->lat, p->cfg->lon);


	frq = g_malloc(s->n * sizeof(gdouble));
	amp = g_malloc(s->n * sizeof(gdouble));

	for (i = 0, f = s->freq_min_hz; i < s->n; i++, f += s->freq_inc_hz) {
		frq[i] = (gdouble) f * 1e-6;		/* Hz to Mhz */
		amp[i] = (gdouble) s->spec[i] * 0.001;	/* mK to K */
	}

	/* everyone gets a copy of the data */
	if (p->cfg->n_per) {
		sp    = (struct spectrum *) g_malloc(sizeof(struct spectrum));
		sp->x = g_memdup2(frq, s->n * sizeof(gdouble));
		sp->y = g_memdup2(amp, s->n * sizeof(gdouble));
		sp->n = s->n;
		spectrum_append_data(p, sp);
	}

	sp    = (struct spectrum *) g_malloc(sizeof(struct spectrum));
	sp->x = frq;
	sp->y = amp;
	sp->n = s->n;


	/* write to file if enabled, this one does not need a copy */
	spectrum_record_add(p, sp);

	/* this one does */
	spectrum_append_avg(p, sp);
}


/**
 * @brief button reset average callback
 */

static void spectrum_reset_avg_cb(GtkWidget *w, Spectrum *p)
{
	spectrum_drop_avg(p);

	xyplot_drop_graph(p->cfg->plot, p->cfg->r_avg);
	p->cfg->r_avg = NULL;

	xyplot_redraw(p->cfg->plot);
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
		xyplot_drop_graph(p->cfg->plot, p->cfg->r_avg);
		spectrum_drop_avg(p);
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
	xyplot_redraw(p->cfg->plot);
	return TRUE;
}


/**
 * @brief button reset persistence callback
 */

static void spectrum_reset_per_cb(GtkWidget *w, Spectrum *p)
{
	spectrum_drop_data(p);
	xyplot_redraw(p->cfg->plot);
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
	xyplot_redraw(p->cfg->plot);

	return TRUE;
}




static void spectrum_vrest_entry_changed_cb(GtkEditable *ed, gpointer data)
{
	gdouble vrest;

	Spectrum *p;


	p = SPECTRUM(data);


	if (!gtk_entry_get_text_length(GTK_ENTRY(ed)))
		return;

	vrest = g_strtod(gtk_entry_get_text(GTK_ENTRY(ed)), NULL);

	p->cfg->freq_ref_mhz = vrest;

	xyplot_redraw(p->cfg->plot);
}


static void spectrum_vrest_entry_insert_text_cb(GtkEditable *ed, gchar *new_text,
					  gint new_text_len, gpointer pos,
					  gpointer data)
{
	gint i;


	/* allow digits and decimal separators only */
	for (i = 0; i < new_text_len; i++) {
		if (!g_ascii_isdigit(new_text[i])
		    && new_text[i] != ','
		    && new_text[i] != '.') {
			g_signal_stop_emission_by_name(ed, "insert-text");
			break;
		}
	}

}


static void spectrum_vrest_sel_changed(GtkComboBox *cb, gpointer data)
{
	GtkListStore *ls;

	GtkTreeIter iter;

	Spectrum *p;

	gdouble vrest;


	p = SPECTRUM(data);

	if (!p)
		return;

	if (!gtk_combo_box_get_active_iter(cb, &iter))
		return;

	ls = GTK_LIST_STORE(gtk_combo_box_get_model(cb));

	gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 2, &vrest, -1);

	p->cfg->freq_ref_mhz = vrest;


	xyplot_redraw(p->cfg->plot);
}


/**
 * @brief create reference rest frequency controls
 *
 * @note here we configure the spectral resolution. The remote device
 *	 would typically support either 2^n (SRT: n_max=2) or linear dividers
 *	 based on a given acquisition bandwidth (500 kHz for the SRT) and a
 *	 range of bins (SRT: 64) with equally either 2^n or linear
 *	 dividiers (SRT: none) for which we must generate the proper selections
 *	 below
 */

GtkWidget *spectrum_vrest_ctrl_new(Spectrum *p)
{
	GtkWidget *w;
	GtkWidget *cb;

	GtkListStore *ls;
	GtkCellRenderer *col;


	/* note: for easier selection, always give J (total electronic angular
	 * momentum quantum number) and F (transitions between hyperfine
	 * levels)
	 *
	 * note on OH: the ground rotational state splits into a lambda-doublet
	 * sub-levels due to the interaction between the rotational and
	 * electronic angular momenta of the molecule. The sub-levels further
	 * split into two hyperfine levels as a result of the interaction
	 * between the electron and nuclear spins of the hydrogen atom.
	 * The transitions that connect sub-levels with the same F-values are
	 * called the main lines, whereas the transitions between sub-levels of
	 * different F-values are called the satellite lines.
	 * (See DICKE'S SUPERRADIANCE IN ASTROPHYSICS. II. THE OH 1612 MHz LINE,
	 * F. Rajabi and M. Houde,The Astrophysical Journal, Volume 828,
	 * Number 1.)
	 * The main lines are stronger than the satellite lines. In star
	 * forming regions, the 1665 MHz line exceeds the 1667 MHz line in
	 * intensity, while in equilibirium conditions, it is generally weaker.
	 * In late-type starts, the 1612 MHz line may sometimes be equal or
	 * even exceed the intensity of the main lines.
	 */


	ls = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE);

	/* printf to get locale decimal style */
	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(HI) J=1/2 F=1-0",
					  1, g_strdup_printf("%7.3f", 1420.406),
					  2, 1420.406, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=1-2",
					  1, g_strdup_printf("%7.3f", 1612.231),
					  2, 1612.231, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=1-1",
					  1, g_strdup_printf("%7.3f", 1665.402),
					  2, 1665.402, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=2-2",
					  1, g_strdup_printf("%7.3f", 1667.359),
					  2, 1667.359, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=2-1",
					  1, g_strdup_printf("%7.3f", 1720.530),
					  2, 1720.530, -1);

	cb = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(ls));

	g_object_unref(ls);

	col = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cb), col, TRUE);

	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cb), col, "text", 0, NULL );
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(cb), 1);

	/* the entry is a child of the box */
	w = gtk_bin_get_child(GTK_BIN(cb));
	gtk_entry_set_width_chars(GTK_ENTRY(w), 8);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);

	g_signal_connect(w, "insert-text",
			 G_CALLBACK(spectrum_vrest_entry_insert_text_cb), p);
	g_signal_connect(w, "changed",
			 G_CALLBACK(spectrum_vrest_entry_changed_cb), p);
	gtk_entry_set_input_purpose(GTK_ENTRY(w), GTK_INPUT_PURPOSE_DIGITS);

	gtk_combo_box_set_id_column(GTK_COMBO_BOX(cb), 1);

	g_signal_connect(G_OBJECT(cb), "changed",
			 G_CALLBACK(spectrum_vrest_sel_changed), (gpointer) p);

	gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);



	return cb;
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

	w = gtk_spin_button_new_with_range(0, 1000, 1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), p->cfg->n_per);
	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(w, FALSE);
	gtk_grid_attach(grid, w, 0, 3, 2, 1);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(spectrum_per_value_changed_cb), p);

	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "HiStep");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Dashed Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Bézier");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Circle");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Square");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Mario");
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

	w = gtk_spin_button_new_with_range(0, 1000, 1);
	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
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

	w = gtk_label_new("Ref. Frequency [MHz]");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 0, 12, 2, 1);

	w = spectrum_vrest_ctrl_new(p);
	gtk_grid_attach(grid, w, 0, 13, 2, 1);;

	w = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_grid_attach(grid, w, 0, 14, 2, 1);


	w = gtk_toggle_button_new_with_label("Record");
	gtk_button_set_always_show_image(GTK_BUTTON(w), TRUE);
	gtk_button_set_image(GTK_BUTTON(w),
			     gtk_image_new_from_icon_name("media-record-symbolic",
							  GTK_ICON_SIZE_BUTTON));
	g_signal_connect(G_OBJECT(w), "toggled",
			 G_CALLBACK(spectrum_rec_button_toggle), p);


	gtk_grid_attach(grid, w, 0, 15, 2, 1);


	w = gtk_label_new("");
	gtk_grid_attach(grid, w, 0, 16, 2, 1);
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

	xyplot_set_x2_conversion(p->cfg->plot, spectrum_convert_x2, (void *) p);
	xyplot_set_x2label(p->cfg->plot, "VLSR [km/s]");


	g_signal_connect(p->cfg->plot, "xyplot-fit-selection",
			 G_CALLBACK(spectrum_plt_fitbox_selected),
			 p);

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
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cap);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_pos);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_con);

	g_timer_destroy(p->cfg->timer);

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

	p->cfg->timer = g_timer_new();
	p->cfg->rec   = NULL;
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

	p->cfg->freq_ref_mhz = 1420.406;

	p->cfg->refresh = 1.0 / SPECTRUM_REFRESH_HZ_CAP;

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

	p->cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
			 G_CALLBACK(spectrum_handle_pr_capabilities),
			 (gpointer) p);

	p->cfg->id_pos = g_signal_connect(sig_get_instance(), "pr-getpos-azel",
			 G_CALLBACK(spectrum_handle_getpos_azel_cb),
			 (gpointer) p);

	p->cfg->id_con = g_signal_connect(sig_get_instance(), "net-connected",
			 G_CALLBACK(spectrum_connected),
			 (gpointer) p);

	g_signal_connect(p, "destroy", G_CALLBACK(spectrum_destroy), NULL);

	g_timer_start(p->cfg->timer);
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
