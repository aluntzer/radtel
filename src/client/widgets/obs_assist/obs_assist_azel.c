/**
 * @file    widgets/obs_assist/obs_assist_azel.c
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
 * @brief a scan a rectangle in azimuth and elevation
 *
 */


#include <obs_assist.h>
#include <obs_assist_cfg.h>
#include <obs_assist_internal.h>

#include <coordinates.h>
#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <xyplot.h>
#include <cmd.h>
#include <math.h>



/**
 * @brief update the az progress bar
 */

static void azel_update_pbar_az(ObsAssist *p)
{
	gdouble d;
	gdouble frac = 1.0;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->azel.pbar_az);

	d = p->cfg->azel.az_hi  - p->cfg->azel.az_lo;

	if (d != 0.0)
		frac = (p->cfg->azel.az_cur - p->cfg->azel.az_lo) / d;

	str = g_strdup_printf("AZ: %5.2f° of [%5.2f°, %5.2f°]",
			      p->cfg->azel.az_cur,
			      p->cfg->azel.az_lo,
			      p->cfg->azel.az_hi);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}



/**
 * @brief update the el progress bar
 */

static void azel_update_pbar_el(ObsAssist *p)
{
	gdouble d;
	gdouble frac = 1.0;

	gchar *str;

	GtkProgressBar *pb;



	pb = GTK_PROGRESS_BAR(p->cfg->azel.pbar_el);

	d = p->cfg->azel.el_hi  - p->cfg->azel.el_lo;

	if (d != 0.0)
		frac = (p->cfg->azel.el_cur - p->cfg->azel.el_lo) / d;


	str = g_strdup_printf("EL: %5.2f° of [%5.2f°, %5.2f°]",
			      p->cfg->azel.el_cur,
			      p->cfg->azel.el_lo,
			      p->cfg->azel.el_hi);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief clear and draw the AZEL plot
 */

static void azel_draw_graph(ObsAssist *p)
{
	gdouble *x;
	gdouble *y;
	gdouble *c;


	/* update graph */
	xyplot_drop_all_graphs(p->cfg->azel.plt);
	x = g_memdup(p->cfg->azel.az->data,
		     p->cfg->azel.az->len * sizeof(gdouble));
	y = g_memdup(p->cfg->azel.el->data,
		     p->cfg->azel.el->len * sizeof(gdouble));
	c = g_memdup(p->cfg->azel.amp->data,
		     p->cfg->azel.amp->len * sizeof(gdouble));

	xyplot_add_graph(p->cfg->azel.plt, x, y, c,
			 p->cfg->azel.az->len,
			 g_strdup_printf("AZEL Scan"));

	xyplot_redraw(p->cfg->azel.plt);
}


/**
 * @brief verify position and issue move command if necessary
 *
 * @param az the actual target Azimuth
 * @param el the actual target Elevation
 *
 * @returns TRUE if in position
 * @note we use 2x the axis resolution for tolerance to avoid sampling issues
 */

static gboolean azel_in_position(ObsAssist *p, gdouble az, gdouble el)
{
	gdouble d_az;
	gdouble d_el;

	const gdouble az_tol = 2.0 * p->cfg->az_res;
	const gdouble el_tol = 2.0 * p->cfg->el_res;


	d_az = fabs(az - p->cfg->az);
	d_el = fabs(el - p->cfg->el);

	if ((d_az > az_tol) || (d_el > el_tol)) {

		obs_assist_clear_spec(p);

		/* disable acquisition first */
		if (p->cfg->acq_enabled)
			cmd_spec_acq_disable(PKT_TRANS_ID_UNDEF);

		/* update position if telescope is not moving already */
		if (!p->cfg->moving)
			cmd_moveto_azel(PKT_TRANS_ID_UNDEF, az, el);

		return FALSE;
	}


	return TRUE;
}


/**
 * @brief take a measurement
 *
 * @brief returns TRUE if measurement was taken
 */

static gboolean azel_measure(ObsAssist *p)
{
	gsize i;

	gdouble tmp = 0.0;

	static guint sample;
	static gdouble avg;


	/* enable acquisition at current position */
	if (!p->cfg->acq_enabled) {
		cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		return FALSE;
	}

	/* has new spectral data arrived? */
	if (!p->cfg->spec.n)
		return FALSE;

	/* compute continuum flux */
	for (i = 0; i < p->cfg->spec.n; i++)
		tmp += p->cfg->spec.y[i];

	tmp = tmp / (gdouble) p->cfg->spec.n;

	avg += tmp;


	if (sample < p->cfg->azel.n_avg) {
		sample++;
		return FALSE;
	}

	avg /= (gdouble) (sample + 1);

	g_array_append_val(p->cfg->azel.az, p->cfg->azel.az_cur);
	g_array_append_val(p->cfg->azel.el, p->cfg->azel.el_cur);
	g_array_append_val(p->cfg->azel.amp, avg);

	sample = 0;
	avg = 0.0;

	return TRUE;
}


/**
 * @brief move into position in azel
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean azel_obs_pos(ObsAssist *p)
{
	gdouble lim;

	const gdouble az_tol = 2.0 * p->cfg->az_res;

#if 1
	/* TODO: add option */
	gdouble cstep;

	cstep = p->cfg->azel.az_hi - p->cfg->azel.az_lo;
	cstep = cstep / trunc(cstep / (p->cfg->azel.az_stp / cos(RAD(p->cfg->azel.el_cur))));
#endif
	azel_update_pbar_az(p);
	azel_update_pbar_el(p);

	/* upper bound reached?  (el after az) */
	lim = p->cfg->azel.el_cur;
	if (p->cfg->azel.el_hi < lim) {
		p->cfg->azel.az_cur = p->cfg->azel.az_hi;
		p->cfg->azel.el_cur = p->cfg->azel.el_hi;
		/* indicate complete */
		azel_update_pbar_az(p);
		azel_update_pbar_el(p);
		return FALSE;
	}


	/* reset az if upper az bound reached and update el*/
	if (p->cfg->azel.az_hi < (p->cfg->azel.az_cur - az_tol)) {
		p->cfg->azel.az_cur = p->cfg->azel.az_hi;
		p->cfg->azel.el_cur += p->cfg->azel.el_stp;
		p->cfg->azel.az_stp *= -1.0;



	} else if (p->cfg->azel.az_lo > (p->cfg->azel.az_cur + az_tol)) {
		p->cfg->azel.az_cur = p->cfg->azel.az_lo;
		p->cfg->azel.el_cur += p->cfg->azel.el_stp;
		p->cfg->azel.az_stp *= -1.0;
	}

	/* actual pointing is done in horizon system */
	if (!azel_in_position(p, p->cfg->azel.az_cur, p->cfg->azel.el_cur))
		return TRUE;

	if (!azel_measure(p))
		return TRUE;

	obs_assist_clear_spec(p);

	azel_draw_graph(p);

	/* update azimuth */
#if 1
	p->cfg->azel.az_cur += cstep;
#else
	p->cfg->azel.az_cur += p->cfg->azel.az_stp;
#endif

	return TRUE;
}


/**
 * @brief perform azel scan
 */

static gboolean azel_obs(void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	if (p->cfg->abort) {
		/* we stay where we are */
		return G_SOURCE_REMOVE;
	}

	if (azel_obs_pos(p))
		return G_SOURCE_CONTINUE;

	/* on final, we stay at the current position */


	/* done */
	g_array_free(p->cfg->azel.az,  TRUE);
	g_array_free(p->cfg->azel.el,  TRUE);
	g_array_free(p->cfg->azel.amp, TRUE);

	return G_SOURCE_REMOVE;
}


/**
 * @brief start the azel observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	p->cfg->azel.az  = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->azel.el  = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->azel.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));


	sig_tracking(FALSE, 0.0, 0.0);

	obs_assist_hide_procedure_selectors(p);


	grid = GTK_GRID(new_default_grid());

	p->cfg->azel.plt = xyplot_new();
	xyplot_set_xlabel(p->cfg->azel.plt, "Azimuth [deg]");
	xyplot_set_ylabel(p->cfg->azel.plt, "Elevation [deg]");

	gtk_widget_set_hexpand(p->cfg->azel.plt, TRUE);
	gtk_widget_set_vexpand(p->cfg->azel.plt, TRUE);
	gtk_grid_attach(grid, p->cfg->azel.plt, 0, 0, 2, 1);
	gtk_widget_set_size_request(p->cfg->azel.plt, -1, 300);


	w = gtk_label_new("Azimuth");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);
	p->cfg->azel.pbar_az = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->azel.pbar_az, TRUE);
	gtk_grid_attach(grid, p->cfg->azel.pbar_az, 1, 1, 1, 1);

	w = gtk_label_new("Elevation");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);
	p->cfg->azel.pbar_el = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->azel.pbar_el, TRUE);
	gtk_grid_attach(grid, p->cfg->azel.pbar_el, 1, 2, 1, 1);


	w = gtk_button_new_with_label("Quit");
	gtk_widget_set_tooltip_text(w, "Quit observation");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_abort), p);


	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));


	/* the actual work is done asynchronously, .5 seconds calls
	 * per should be fine
	 */
	g_timeout_add(500, azel_obs, p);
}


/**
 * @brief set up the azel observation
 */

static void obs_assist_on_prepare_azel(GtkWidget *as, GtkWidget *pg,
					    ObsAssist *p)
{
	gint cp;

	gdouble tmp;

	GtkWidget *w;
	GtkWidget *box;

	GtkSpinButton *sb;

	GtkAssistantPageType type;

	gchar *lbl;



	type = gtk_assistant_get_page_type(GTK_ASSISTANT(as), pg);
	if (type != GTK_ASSISTANT_PAGE_CONFIRM)
		return;


	/* set configuration */
	sb = p->cfg->azel.sb_az_deg;
	p->cfg->azel.az_stp = gtk_spin_button_get_value(sb);

	sb = p->cfg->azel.sb_el_deg;
	p->cfg->azel.el_stp = gtk_spin_button_get_value(sb);

	sb = p->cfg->azel.sb_az_lo;
	p->cfg->azel.az_lo = gtk_spin_button_get_value(sb);

	sb = p->cfg->azel.sb_az_hi;
	p->cfg->azel.az_hi = gtk_spin_button_get_value(sb);

	sb = p->cfg->azel.sb_el_lo;
	p->cfg->azel.el_lo = gtk_spin_button_get_value(sb);

	sb = p->cfg->azel.sb_el_hi;
	p->cfg->azel.el_hi = gtk_spin_button_get_value(sb);

	sb = p->cfg->azel.sb_avg;
	p->cfg->azel.n_avg = gtk_spin_button_get_value_as_int(sb);


	/* swap around */
	if (p->cfg->azel.az_lo > p->cfg->azel.az_hi) {
		tmp = p->cfg->azel.az_hi;
		p->cfg->azel.az_hi = p->cfg->azel.az_lo;
		p->cfg->azel.az_lo = tmp;
	}

	if (p->cfg->azel.el_lo > p->cfg->azel.el_hi) {
		tmp = p->cfg->azel.el_hi;
		p->cfg->azel.el_hi = p->cfg->azel.el_lo;
		p->cfg->azel.el_lo = tmp;
	}


	/* adjust for integer steps */

	tmp = p->cfg->azel.az_hi - p->cfg->azel.az_lo;
	p->cfg->azel.az_stp = tmp / trunc(tmp / p->cfg->azel.az_stp);

	tmp = p->cfg->azel.el_hi - p->cfg->azel.el_lo;
	p->cfg->azel.el_stp = tmp / trunc(tmp / p->cfg->azel.el_stp);



	/* initial scan position is at lower bound */
	p->cfg->azel.az_cur = p->cfg->azel.az_lo;
	p->cfg->azel.el_cur = p->cfg->azel.el_lo;


	cp  = gtk_assistant_get_current_page(GTK_ASSISTANT(as));
      	box = gtk_assistant_get_nth_page(GTK_ASSISTANT(as), cp);

	gtk_container_foreach(GTK_CONTAINER(box),
			      (GtkCallback) gtk_widget_destroy, NULL);

	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

	lbl = g_strdup_printf(
	      "This is your configuration:\n\n"
	      "<tt>"
	      "Azimuth lower bound:       <b>%5.2f°</b>\n"
	      "Azimuth upper bound:       <b>%5.2f°</b>\n"
	      "Azimuth step size:         <b>%5.2f°</b>\n"
	      "Elevation lower bound:     <b>%5.2f°</b>\n"
	      "Elevation upper bound:     <b>%5.2f°</b>\n"
	      "Elevation step size:       <b>%5.2f°</b>\n"
	      "Samples per position:      <b>%d</b>\n\n"
	      "NOTE: step sizes may have been adjusted "
	      "to fit specified ranges."
	      "</tt>",
	      p->cfg->azel.az_lo, p->cfg->azel.az_hi,
	      p->cfg->azel.az_stp,
	      p->cfg->azel.el_lo, p->cfg->azel.el_hi,
	      p->cfg->azel.el_stp,
	      p->cfg->azel.n_avg);


        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_set_halign(w, GTK_ALIGN_START);


	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, TRUE);

	gtk_widget_show_all(box);
}


/**
 * @brief create info page
 */

static void obs_assist_azel_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);

	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
		"This observation mode will perform a grid scan between "
		"an spherical rectangle spanned by points in Azimuth and "
		"Elevation\n"
		"The resulting graph will show a Azimuth-Elevation diagram "
		"with the spectral signal amplitudes encoded in colour.\n\n");
#if 0
	/* XXX make optional */
		"<b>NOTE:</b> Azimuth angular distance steps will be corrected "
		"by the cosine of the elevation.");
#endif
        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);

	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_show_all(box);


	gtk_assistant_append_page(as, box);
	gtk_assistant_set_page_complete(as, box, TRUE);
	gtk_assistant_set_page_title(as, box, "Info");
	gtk_assistant_set_page_type(as, box, GTK_ASSISTANT_PAGE_INTRO);
}


/**
 * @brief create info page
 */

static void obs_assist_azel_create_page_2(GtkAssistant *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;
	GtkSpinButton *sb;


	grid = GTK_GRID(new_default_grid());


	/** Azimuth Step **/
	w = gui_create_desclabel("Azimuth Step Size",
				 "Specify the Azimuth step size in degrees.");
	gtk_grid_attach(grid, w, 0, 0, 1, 1);


	/* set an arbitrary limit of 10 degrees for*/
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(p->cfg->az_res,
							    10.,
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 2.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 0, 1, 1);
	p->cfg->azel.sb_az_deg = sb;


	/** Elevation Step **/
	w = gui_create_desclabel("Elevation Step Size",
				 "Specify the Elevation step size in degrees.");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set an arbitrary limit of 10 degrees for*/
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(p->cfg->el_res,
							    10.,
							    ceil(p->cfg->el_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 2.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->azel.sb_el_deg = sb;


	/** AZ lower bound **/
	w = gui_create_desclabel("Azimuth Start",
				 "Specify the lower bound of the "
				 "observation for the Azimuth axis.");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(ceil(p->cfg->az_min),
							    floor(p->cfg->az_max),
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 45.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 2, 1, 1);
	p->cfg->azel.sb_az_lo = sb;


	/** AZ upper bound **/
	w = gui_create_desclabel("Azimuth Stop",
				 "Specify the upper bound of the "
				 "observation for the Azimuth axis.");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(ceil(p->cfg->az_min),
							    floor(p->cfg->az_max),
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 12.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 3, 1, 1);
	p->cfg->azel.sb_az_hi = sb;



	/** EL lower bound **/
	w = gui_create_desclabel("Elevation Start",
				 "Specify the lower bound of the "
				 "observation for the Elevation axis.");
	gtk_grid_attach(grid, w, 0, 4, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(ceil(p->cfg->el_min),
							    floor(p->cfg->el_max),
							    ceil(p->cfg->el_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 30.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 4, 1, 1);
	p->cfg->azel.sb_el_lo = sb;


	/** EL upper bound **/
	w = gui_create_desclabel("Elevation Stop",
				 "Specify the upper bound of the "
				 "observation for the Elevation axis.");
	gtk_grid_attach(grid, w, 0, 5, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(ceil(p->cfg->el_min),
							    floor(p->cfg->el_max),
							    ceil(p->cfg->el_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 60.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 5, 1, 1);
	p->cfg->azel.sb_el_hi = sb;



	/** Averages **/
	w = gui_create_desclabel("Samples per position",
				 "Specify the number of measurements to be "
				 "averaged at each position.");
	gtk_grid_attach(grid, w, 0, 6, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 20, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 6, 1, 1);
	p->cfg->azel.sb_avg = GTK_SPIN_BUTTON(sb);



	gtk_widget_show_all(GTK_WIDGET(grid));

	/* attach page */
	gtk_assistant_append_page(as, GTK_WIDGET(grid));
	gtk_assistant_set_page_complete(as, GTK_WIDGET(grid), TRUE);
	gtk_assistant_set_page_title(as, GTK_WIDGET(grid), "Setup");
	gtk_assistant_set_page_type(as, GTK_WIDGET(grid),
				    GTK_ASSISTANT_PAGE_CONTENT);
}


/**
 * @brief create summary page
 */

static void obs_assist_azel_create_page_3(GtkAssistant *as)
{
	GtkWidget *box;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_widget_show_all(box);

	gtk_assistant_append_page(as, box);
	gtk_assistant_set_page_title(as, box, "Confirm");
	gtk_assistant_set_page_complete(as, box, TRUE);
	gtk_assistant_set_page_type(as, box, GTK_ASSISTANT_PAGE_CONFIRM);
}


/**
 * @brief populate the assistant
 */

static void obs_assist_azel_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);

	p->cfg->abort = FALSE;
	bzero(&p->cfg->azel, sizeof(p->cfg->azel));

	/* info page */
	obs_assist_azel_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_azel_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_azel_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_azel), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create azel scan selection
 */

GtkWidget *obs_assist_azel_scan_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Azimth/Elevation Scan",
				 "Perform a scan in Azimuth and Elevation "
				 "range.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start AZEL Scan.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_azel_setup_cb), p);


	return GTK_WIDGET(grid);
}
