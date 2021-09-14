/**
 * @file    widgets/obs_assist/obs_assist_npoint.c
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
 * @brief a scan a rectangle in galactic latitude and longitude
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

static void npoint_update_pbar_glon(ObsAssist *p)
{
	gdouble d;
	gdouble frac = 1.0;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->npoint.pbar_glon);

	d = p->cfg->npoint.glon_hi  - p->cfg->npoint.glon_lo;

	if (d != 0.0)
		frac = (p->cfg->npoint.glon_cur - p->cfg->npoint.glon_lo) / d;

	str = g_strdup_printf("GLON: %5.2f° of [%5.2f°, %5.2f°]",
			      p->cfg->npoint.glon_cur,
			      p->cfg->npoint.glon_lo,
			      p->cfg->npoint.glon_hi);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}



/**
 * @brief update the el progress bar
 */

static void npoint_update_pbar_glat(ObsAssist *p)
{
	gdouble d;
	gdouble frac = 1.0;

	gchar *str;

	GtkProgressBar *pb;



	pb = GTK_PROGRESS_BAR(p->cfg->npoint.pbar_glat);

	d = p->cfg->npoint.glat_hi  - p->cfg->npoint.glat_lo;

	if (d != 0.0)
		frac = (p->cfg->npoint.glat_cur - p->cfg->npoint.glat_lo) / d;


	str = g_strdup_printf("GLAT: %5.2f° of [%5.2f°, %5.2f°]",
			      p->cfg->npoint.glat_cur,
			      p->cfg->npoint.glat_lo,
			      p->cfg->npoint.glat_hi);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief clear and draw the NPoint plot
 *
 * @todo one graph per scanline (see also npoint_measure)
 */

static void npoint_draw_graph(ObsAssist *p)
{
	gdouble *x;
	gdouble *y;
	gdouble *c;


	/* update graph */
	xyplot_drop_all_graphs(p->cfg->npoint.plt);
	x = g_memdup2(p->cfg->npoint.glon->data,
		     p->cfg->npoint.glon->len * sizeof(gdouble));
	y = g_memdup2(p->cfg->npoint.glat->data,
		     p->cfg->npoint.glat->len * sizeof(gdouble));
	c = g_memdup2(p->cfg->npoint.amp->data,
		     p->cfg->npoint.amp->len * sizeof(gdouble));

	xyplot_add_graph(p->cfg->npoint.plt, x, y, c,
			 p->cfg->npoint.glon->len,
			 g_strdup_printf("NPoint Scan"));

	xyplot_redraw(p->cfg->npoint.plt);
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

static gboolean npoint_in_position(ObsAssist *p, gdouble az, gdouble el)
{
	gdouble d_az;
	gdouble d_el;

	const gdouble az_tol = 1.0 * p->cfg->az_res;
	const gdouble el_tol = 1.0 * p->cfg->el_res;


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
 *
 * @todo this should take actual glat/glon back-converted from the
 *	 horizontal position to avoid position aliasing, but needs
 *	 support in npoint_draw_graph for one-graph-per-scanline as well
 */

static gboolean npoint_measure(ObsAssist *p)
{
	gsize i;

	gdouble tmp = 0.0;

	static guint sample;
	static gdouble avg;
#if 0
	struct coord_horizontal hor;
	struct coord_galactic gal;
#endif

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


	if (sample < p->cfg->npoint.n_avg) {
		sample++;
		return FALSE;
	}

	avg /= (gdouble) (sample + 1);

#if 0
	hor.az = p->cfg->az;
	hor.el = p->cfg->el;

	gal = horizontal_to_galactic(hor, p->cfg->lat, p->cfg->lon);

	g_print("at %g %g: %g %g vs %g %g\n", hor.az, hor.el, gal.lat, gal.lon,
		p->cfg->npoint.glon_cur, p->cfg->npoint.glat_cur);

	g_array_append_val(p->cfg->npoint.glon, gal.lon);
	g_array_append_val(p->cfg->npoint.glat, gal.lat);
#else
	g_array_append_val(p->cfg->npoint.glon, p->cfg->npoint.glon_cur);
	g_array_append_val(p->cfg->npoint.glat, p->cfg->npoint.glat_cur);
#endif
	g_array_append_val(p->cfg->npoint.amp, avg);


	sample = 0;
	avg = 0.0;

	return TRUE;
}


/**
 * @brief move into position in glat/glon
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean npoint_obs_pos(ObsAssist *p)
{
	gdouble lim;

	struct coord_horizontal hor;
	struct coord_galactic gal;


	npoint_update_pbar_glon(p);
	npoint_update_pbar_glat(p);

	/* upper bound reached?  (glon after glat) */
	lim = p->cfg->npoint.glon_cur;
	if (p->cfg->npoint.glon_hi < lim) {
		p->cfg->npoint.glat_cur = p->cfg->npoint.glat_hi;
		p->cfg->npoint.glon_cur = p->cfg->npoint.glon_hi;
		/* indicate complete */
		npoint_update_pbar_glat(p);
		npoint_update_pbar_glon(p);
		return FALSE;
	}


	/* reset glat if upper glat bound reached and update glon */
	if (p->cfg->npoint.glat_hi < p->cfg->npoint.glat_cur) {
		p->cfg->npoint.glat_cur = p->cfg->npoint.glat_hi;
		p->cfg->npoint.glon_cur += p->cfg->npoint.glon_stp;
		p->cfg->npoint.glat_stp *= -1.0;

	} else if (p->cfg->npoint.glat_lo > p->cfg->npoint.glat_cur) {
		p->cfg->npoint.glat_cur = p->cfg->npoint.glat_lo;
		p->cfg->npoint.glon_cur += p->cfg->npoint.glon_stp;
		p->cfg->npoint.glat_stp *= -1.0;
	}

	gal.lat = p->cfg->npoint.glat_cur;
	gal.lon = p->cfg->npoint.glon_cur;

	hor = galactic_to_horizontal(gal, p->cfg->lat, p->cfg->lon, 0.0);


	/* actual pointing is done in horizon system */
	if (!npoint_in_position(p, hor.az, hor.el))
		return TRUE;

	if (!npoint_measure(p))
		return TRUE;

	obs_assist_clear_spec(p);

	npoint_draw_graph(p);

	/* update glat */
	p->cfg->npoint.glat_cur += p->cfg->npoint.glat_stp;

	/* set telescope position to be far off next measurement point
	 * this mitigates aliasing if the galactic and horizontal grids
	 * overlap in a way where the converted coordinates fall within
	 * the move tolerance (i.e. aixs step size)
	 */

	p->cfg->az = -p->cfg->az;
	p->cfg->el = -p->cfg->el;

	return TRUE;
}


/**
 * @brief perform azel scan
 */

static gboolean npoint_obs(void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	if (p->cfg->abort) {
		/* we stay where we are */
		return G_SOURCE_REMOVE;
	}

	if (npoint_obs_pos(p))
		return G_SOURCE_CONTINUE;

	/* on final, we stay at the current position */


	/* done */
	g_array_free(p->cfg->npoint.glon,  TRUE);
	g_array_free(p->cfg->npoint.glat,  TRUE);
	g_array_free(p->cfg->npoint.amp, TRUE);

	return G_SOURCE_REMOVE;
}


/**
 * @brief start the azel observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	p->cfg->npoint.glon = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->npoint.glat = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->npoint.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));


	sig_tracking(FALSE, 0.0, 0.0);

	obs_assist_hide_procedure_selectors(p);


	grid = GTK_GRID(new_default_grid());

	p->cfg->npoint.plt = xyplot_new();
	xyplot_set_xlabel(p->cfg->npoint.plt, "Galactic Longitude [deg]");
	xyplot_set_ylabel(p->cfg->npoint.plt, "Galactic Latitude [deg]");

	gtk_widget_set_hexpand(p->cfg->npoint.plt, TRUE);
	gtk_widget_set_vexpand(p->cfg->npoint.plt, TRUE);
	gtk_grid_attach(grid, p->cfg->npoint.plt, 0, 0, 2, 1);
	gtk_widget_set_size_request(p->cfg->npoint.plt, -1, 300);


	w = gtk_label_new("Gal. Lon.");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);
	p->cfg->npoint.pbar_glon = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->npoint.pbar_glon, TRUE);
	gtk_grid_attach(grid, p->cfg->npoint.pbar_glon, 1, 1, 1, 1);

	w = gtk_label_new("Gal. Lat");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);
	p->cfg->npoint.pbar_glat = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->npoint.pbar_glat, TRUE);
	gtk_grid_attach(grid, p->cfg->npoint.pbar_glat, 1, 2, 1, 1);


	w = gtk_button_new_with_label("Quit");
	gtk_widget_set_tooltip_text(w, "Quit observation");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_abort), p);


	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));


	/* the actual work is done asynchronously, .05 seconds calls
	 * per should be fine
	 */
	g_timeout_add(50, npoint_obs, p);
}


/**
 * @brief set up the npoint observation
 */

static void obs_assist_on_prepare_npoint(GtkWidget *as, GtkWidget *pg,
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
	sb = p->cfg->npoint.sb_glon_deg;
	p->cfg->npoint.glon_stp = gtk_spin_button_get_value(sb);

	sb = p->cfg->npoint.sb_glat_deg;
	p->cfg->npoint.glat_stp = gtk_spin_button_get_value(sb);

	sb = p->cfg->npoint.sb_glon_lo;
	p->cfg->npoint.glon_lo = gtk_spin_button_get_value(sb);

	sb = p->cfg->npoint.sb_glon_hi;
	p->cfg->npoint.glon_hi = gtk_spin_button_get_value(sb);

	sb = p->cfg->npoint.sb_glat_lo;
	p->cfg->npoint.glat_lo = gtk_spin_button_get_value(sb);

	sb = p->cfg->npoint.sb_glat_hi;
	p->cfg->npoint.glat_hi = gtk_spin_button_get_value(sb);

	sb = p->cfg->npoint.sb_avg;
	p->cfg->npoint.n_avg = gtk_spin_button_get_value_as_int(sb);


	/* swap around */
	if (p->cfg->npoint.glon_lo > p->cfg->npoint.glon_hi) {
		tmp = p->cfg->npoint.glon_hi;
		p->cfg->npoint.glon_hi = p->cfg->npoint.glon_lo;
		p->cfg->npoint.glon_lo = tmp;
	}

	if (p->cfg->npoint.glat_lo > p->cfg->npoint.glat_hi) {
		tmp = p->cfg->npoint.glat_hi;
		p->cfg->npoint.glat_hi = p->cfg->npoint.glat_lo;
		p->cfg->npoint.glat_lo = tmp;
	}


	/* adjust for integer steps */

	tmp = p->cfg->npoint.glon_hi - p->cfg->npoint.glon_lo;
	p->cfg->npoint.glon_stp = tmp / trunc(tmp / p->cfg->npoint.glon_stp);

	tmp = p->cfg->npoint.glat_hi - p->cfg->npoint.glat_lo;
	p->cfg->npoint.glat_stp = tmp / trunc(tmp / p->cfg->npoint.glat_stp);



	/* initial scan position is at lower bound */
	p->cfg->npoint.glon_cur = p->cfg->npoint.glon_lo;
	p->cfg->npoint.glat_cur = p->cfg->npoint.glat_lo;


	cp  = gtk_assistant_get_current_page(GTK_ASSISTANT(as));
      	box = gtk_assistant_get_nth_page(GTK_ASSISTANT(as), cp);

	gtk_container_foreach(GTK_CONTAINER(box),
			      (GtkCallback) gtk_widget_destroy, NULL);

	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

	lbl = g_strdup_printf(
	      "This is your configuration:\n\n"
	      "<tt>"
	      "Gal. Lon. lower bound:     <b>%5.2f°</b>\n"
	      "Gal. Lon. upper bound:     <b>%5.2f°</b>\n"
	      "Gal. Lon. step size:       <b>%5.2f°</b>\n"
	      "Gal. Lat. lower bound:     <b>%5.2f°</b>\n"
	      "Gal. Lat. upper bound:     <b>%5.2f°</b>\n"
	      "Gal. Lat. step size:       <b>%5.2f°</b>\n"
	      "Samples per position:      <b>%d</b>\n\n"
	      "NOTE: step sizes may have been adjusted "
	      "to fit specified ranges."
	      "</tt>",
	      p->cfg->npoint.glon_lo, p->cfg->npoint.glon_hi,
	      p->cfg->npoint.glon_stp,
	      p->cfg->npoint.glat_lo, p->cfg->npoint.glat_hi,
	      p->cfg->npoint.glat_stp,
	      p->cfg->npoint.n_avg);


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

static void obs_assist_npoint_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);

	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
		"This observation mode will perform a grid scan in "
		"a rectangle spanned by points in Galactic Coordinates\n"
		"The resulting graph will show a Latitude-Longitude diagram "
		"with the spectral signal amplitudes encoded in colour.\n\n");

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

static void obs_assist_npoint_create_page_2(GtkAssistant *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;
	GtkSpinButton *sb;


	grid = GTK_GRID(new_default_grid());

	/* XXX should limit step size to worst-res axis res at least */

	/** GLON Step **/
	w = gui_create_desclabel("Galactic Longitude Step Size",
				 "Specify the Galactic Longitude step size in degrees.");
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
	p->cfg->npoint.sb_glon_deg = sb;


	/** GLAT Step **/
	w = gui_create_desclabel("Galactic Latitude Step Size",
				 "Specify the Galactic Latitude step size in degrees.");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set an arbitrary limit of 10 degrees for*/
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(p->cfg->az_res,
							    10.,
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 2.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->npoint.sb_glat_deg = sb;


	/** GLON lower bound **/
	w = gui_create_desclabel("Galactic Longitude Start",
				 "Specify the lower bound of the "
				 "observation for the Galactic Longitude.");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0.0, 360.,
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 120.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 2, 1, 1);
	p->cfg->npoint.sb_glon_lo = sb;


	/** GLON upper bound **/
	w = gui_create_desclabel("Galactic Longitude Stop",
				 "Specify the upper bound of the "
				 "observation for Galactic Longitude.");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0., 360.,
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 240.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 3, 1, 1);
	p->cfg->npoint.sb_glon_hi = sb;



	/** GLAT lower bound **/
	w = gui_create_desclabel("Galactic Latitude Start",
				 "Specify the lower bound of the "
				 "observation for Galactic Latitude.");
	gtk_grid_attach(grid, w, 0, 4, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-90., 90.,
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, -30.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 4, 1, 1);
	p->cfg->npoint.sb_glat_lo = sb;


	/** GLAT upper bound **/
	w = gui_create_desclabel("Galactic Latitude Stop",
				 "Specify the upper bound of the "
				 "observation for Galactic Latitude.");
	gtk_grid_attach(grid, w, 0, 5, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-90., 90.,
							    ceil(p->cfg->az_res * 10.) * 0.1));
	gtk_spin_button_set_value(sb, 30.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 5, 1, 1);
	p->cfg->npoint.sb_glat_hi = sb;



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
	p->cfg->npoint.sb_avg = GTK_SPIN_BUTTON(sb);



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

static void obs_assist_npoint_create_page_3(GtkAssistant *as)
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

static void obs_assist_npoint_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);

	p->cfg->abort = FALSE;
	memset(&p->cfg->azel, 0, sizeof(p->cfg->azel));

	/* info page */
	obs_assist_npoint_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_npoint_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_npoint_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_npoint), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create azel scan selection
 */

GtkWidget *obs_assist_npoint_scan_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("N-Point Map Scan",
				 "Perform a map scan in Galactic Coordinates.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Map Scan.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_npoint_setup_cb), p);


	return GTK_WIDGET(grid);
}
