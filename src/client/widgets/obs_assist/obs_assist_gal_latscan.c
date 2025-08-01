/**
 * @file    widgets/obs_assist/obs_assist_gal_latscan.c
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
 * @brief a scan along a galactic latitude
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


static int once;

/* a mechanism to allow recording of at least one
 * spectrum when a position has been reached;
 * this prevents apparent stalls in the zenith region
 * when tracking objects in coordinate systems other
 * than horizon; note: this is due to the zenith being
 * a pole where the changes in angular position between
 * the coordinate systems may occur faster than
 * the recording speed of a single spectrum
 */

static void gal_latscan_set_once(int arg)
{
	if (arg)
		once = 1;
	else
		once = 0;
}

static int gal_latscan_get_once(void)
{
	return once;
}



/**
 * @brief update the glon progress bar
 */

static void gal_latscan_update_pbar_glon(ObsAssist *p)
{
	gdouble frac;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->gal_latscan.pbar_glon);

	frac = (p->cfg->gal_latscan.glon_cur - p->cfg->gal_latscan.glon_lo) /
	       (p->cfg->gal_latscan.glon_hi - p->cfg->gal_latscan.glon_lo);

	str = g_strdup_printf("GLON: %5.2f° of [%5.2f°, %5.2f°]",
			      p->cfg->gal_latscan.glon_cur,
			      p->cfg->gal_latscan.glon_lo,
			      p->cfg->gal_latscan.glon_hi);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief update the repeat progress bar
 */

static void gal_latscan_update_pbar_rpt(ObsAssist *p)
{
	gdouble frac;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->gal_latscan.pbar_rpt);

	frac = (gdouble) p->cfg->gal_latscan.rpt_cur /
	       (gdouble) p->cfg->gal_latscan.n_rpt;

	str = g_strdup_printf("Run: %d of %d",
			      p->cfg->gal_latscan.rpt_cur,
			      p->cfg->gal_latscan.n_rpt);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief update the velocity-longitude grap
 */

static void gal_latscan_draw_graph(ObsAssist *p, gdouble glon, struct spectrum *s)
{
	gsize i;

	gdouble *lon;

	gdouble min = DBL_MAX;

	struct coord_galactic gal;


	gal.lat = p->cfg->gal_latscan.glat;
	gal.lon = glon;

	lon = (gdouble *) g_malloc(s->n * sizeof(gdouble));

	for (i = 0; i < s->n; i++) {
		if (s->y[i] < min)
			min = s->y[i];
	}

	for (i = 0; i < s->n; i++)  {
		s->y[i] -= min;
		/* XXX velocity reference from wizard !*/
		s->x[i] = - (vlsr(galactic_to_equatorial(gal), 0.0)
			     + doppler_vel(s->x[i], 1420.406));
		lon[i] = glon;
	}

	xyplot_add_graph(p->cfg->gal_latscan.plt, lon, s->x, s->y, s->n,
			 g_strdup_printf("GLON %g", glon));

	xyplot_redraw(p->cfg->gal_latscan.plt);
}


/**
 * @brief verify position and issue move command if necessary
 *
 * @param az the actual target Azimuth
 * @param el the actual target Elevation
 *
 * @returns TRUE if in position
 * @note we use 1.5x the axis resolution for tolerance to avoid sampling issues
 */

static gboolean gal_latscan_in_position(ObsAssist *p, gdouble az, gdouble el)
{
	gdouble d_az;
	gdouble d_el;

	const gdouble az_tol = 1.5 * p->cfg->az_res;
	const gdouble el_tol = 1.5 * p->cfg->el_res;


	if (az < 0.0)
		az = 360.0 + az;

	d_az = fmod(fabs(az - p->cfg->az), 360.0);
	d_el = fmod(fabs(el - p->cfg->el), 90.0);

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

static gboolean gal_latscan_measure(ObsAssist *p)
{
	gsize i;

	static gint samples;

	static struct spectrum *sp;


	/* enable acquisition at current position */
	if (!p->cfg->acq_enabled) {
		cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		return FALSE;
	}

	/* has new spectral data arrived? */
	if (!p->cfg->spec.n)
		return FALSE;

	if (!sp) {
		sp    = (struct spectrum *) g_malloc(sizeof(struct spectrum));
		sp->x = g_memdup2(p->cfg->spec.x, p->cfg->spec.n * sizeof(gdouble));
		sp->y = g_memdup2(p->cfg->spec.y, p->cfg->spec.n * sizeof(gdouble));
		sp->n = p->cfg->spec.n;
	}

	/* spec data has arrived, we may track again */
	gal_latscan_set_once(FALSE);

	/* If length of spectral data or the first frequency bin has changed,
	 * we can be pretty sure that the spectrometer configuration changed
	 * while we were accumulating. This leaves some edge cases, but who
	 * cares. Worst cast, the data is bad.
	 */
	if ((sp->n != p->cfg->spec.n) || sp->x[0] != p->cfg->spec.x[0]) {
		g_free(sp->x);
		g_free(sp->y);
		g_free(sp);
		sp = NULL;
		samples = 0;

		obs_assist_clear_spec(p);
		return FALSE;
	}


	samples++;

	/* stack */
	if (samples < p->cfg->gal_latscan.n_avg) {
		for (i = 0; i < sp->n; i++)
			sp->y[i] += p->cfg->spec.y[i];

		obs_assist_clear_spec(p);
		return FALSE;
	}


	/* done stacking, divide if necessary */

	if (samples > 1) {
		for (i = 0; i < sp->n; i++)
			sp->y[i] /= samples;
	}

	gal_latscan_draw_graph(p, p->cfg->gal_latscan.glon_cur, sp);

	g_free(sp);
	sp = NULL; /* plot takes care of data deallocation */
	samples = 0;

	obs_assist_clear_spec(p);

	return TRUE;
}


/**
 * @brief move into position
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean gal_latscan_obs_pos(ObsAssist *p)
{
	gdouble glon_lim;

	struct coord_galactic gal;
	struct coord_horizontal hor;


	gal_latscan_update_pbar_glon(p);

	/* upper bound reached?  */
	glon_lim = p->cfg->gal_latscan.glon_cur;
	if ((p->cfg->gal_latscan.glon_hi < glon_lim) ||
	    (p->cfg->gal_latscan.glon_lo > glon_lim))
		return FALSE;

	/* actual pointing is done in horizon system */
	gal.lat = p->cfg->gal_latscan.glat;
	gal.lon = p->cfg->gal_latscan.glon_cur;
	hor = galactic_to_horizontal(gal, p->cfg->lat, p->cfg->lon, 0.0);

	if (!gal_latscan_get_once())
		if (!gal_latscan_in_position(p, hor.az, hor.el))
		return TRUE;

	/* we reached the position, allow at least one spectrum;
	 * this will be cleared in gal_latscan_measure()
	 */
	gal_latscan_set_once(TRUE);

	if (!gal_latscan_measure(p))
		return TRUE;

	obs_assist_clear_spec(p);

	/* update longitude */
	p->cfg->gal_latscan.glon_cur += p->cfg->gal_latscan.glon_stp;

	return TRUE;
}


/**
 * @brief scan along latitude
 */

static gboolean gal_latscan_obs(void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	if (p->cfg->abort) {
		/* we stay where we are */
		return G_SOURCE_REMOVE;
	}

	if (gal_latscan_obs_pos(p))
		return G_SOURCE_CONTINUE;


	/* on repeat, set glon back to lower bound */
	if (p->cfg->gal_latscan.rpt_cur < p->cfg->gal_latscan.n_rpt) {
		p->cfg->gal_latscan.rpt_cur++;
		p->cfg->gal_latscan.glon_stp *= -1.0;
		p->cfg->gal_latscan.glon_cur += p->cfg->gal_latscan.glon_stp;;
		gal_latscan_update_pbar_rpt(p);
		return G_SOURCE_CONTINUE;
	}

	/* on final, we stay at the current position */

	/* done */

	return G_SOURCE_REMOVE;
}


/**
 * @brief start the gal_latscan observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	sig_tracking(FALSE, 0.0, 0.0);

	obs_assist_hide_procedure_selectors(p);


	grid = GTK_GRID(new_default_grid());

	p->cfg->gal_latscan.plt = xyplot_new();
	xyplot_set_xlabel(p->cfg->gal_latscan.plt, "Galactic Longitude [deg]");
	xyplot_set_ylabel(p->cfg->gal_latscan.plt, "VLSR [km/s]");

	gtk_widget_set_hexpand(p->cfg->gal_latscan.plt, TRUE);
	gtk_widget_set_vexpand(p->cfg->gal_latscan.plt, TRUE);
	gtk_grid_attach(grid, p->cfg->gal_latscan.plt, 0, 0, 2, 1);
	gtk_widget_set_size_request(p->cfg->gal_latscan.plt, -1, 300);


	w = gtk_label_new("Scan");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);
	p->cfg->gal_latscan.pbar_glon = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->gal_latscan.pbar_glon, TRUE);
	gtk_grid_attach(grid, p->cfg->gal_latscan.pbar_glon, 1, 1, 1, 1);

	w = gtk_label_new("Repeat");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);
	p->cfg->gal_latscan.pbar_rpt = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->gal_latscan.pbar_rpt, TRUE);
	gtk_grid_attach(grid, p->cfg->gal_latscan.pbar_rpt, 1, 2, 1, 1);


	w = gtk_button_new_with_label("Quit");
	gtk_widget_set_tooltip_text(w, "Quit observation");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_abort), p);


	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));

	/* set initial */
	gal_latscan_update_pbar_rpt(p);

	/* the actual work is done asynchronously, .1 seconds calls
	 * per should be fine
	 */
	g_timeout_add(100, gal_latscan_obs, p);
}


/**
 * @brief set up the galactic latitude observation
 */

static void obs_assist_on_prepare_gal_latscan(GtkWidget *as, GtkWidget *pg,
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
	sb = p->cfg->gal_latscan.sb_deg;
	p->cfg->gal_latscan.glon_stp = gtk_spin_button_get_value(sb);

	sb = p->cfg->gal_latscan.sb_lo;
	p->cfg->gal_latscan.glon_lo = gtk_spin_button_get_value(sb);

	sb = p->cfg->gal_latscan.sb_hi;
	p->cfg->gal_latscan.glon_hi = gtk_spin_button_get_value(sb);

	sb = p->cfg->gal_latscan.sb_glat;
	p->cfg->gal_latscan.glat = gtk_spin_button_get_value(sb);

	sb = p->cfg->gal_latscan.sb_avg;
	p->cfg->gal_latscan.n_avg = gtk_spin_button_get_value_as_int(sb);

	sb = p->cfg->gal_latscan.sb_rpt;
	p->cfg->gal_latscan.n_rpt = gtk_spin_button_get_value_as_int(sb);


	/* determine initial direction*/
	tmp = p->cfg->gal_latscan.glon_hi - p->cfg->gal_latscan.glon_lo;
	if (tmp < 0.0)
		p->cfg->gal_latscan.glon_stp *= -1.0;

	/* initial scan position is at lower bound */
	p->cfg->gal_latscan.glon_cur = p->cfg->gal_latscan.glon_lo;

	/* swap around */
	if (p->cfg->gal_latscan.glon_lo > p->cfg->gal_latscan.glon_hi) {
		tmp = p->cfg->gal_latscan.glon_hi;
		p->cfg->gal_latscan.glon_hi = p->cfg->gal_latscan.glon_lo;
		p->cfg->gal_latscan.glon_lo = tmp;
	}


	p->cfg->gal_latscan.rpt_cur  = 1;


	cp  = gtk_assistant_get_current_page(GTK_ASSISTANT(as));
      	box = gtk_assistant_get_nth_page(GTK_ASSISTANT(as), cp);

	gtk_container_foreach(GTK_CONTAINER(box),
			      (GtkCallback) gtk_widget_destroy, NULL);

	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

	lbl = g_strdup_printf(
	      "This is your configuration:\n\n"
	      "<tt>"
	      "Nominal step size:         <b>%5.2f°</b>\n"
	      "GLON lower bound:          <b>%5.2f°</b>\n"
	      "GLON upper bound:          <b>%5.2f°</b>\n"
	      "GLAT:                      <b>%5.2f°</b>\n"
	      "Samples per position:      <b>%d</b>\n"
	      "Scan repeat:               <b>%d</b>\n"
	      "</tt>",
	      p->cfg->gal_latscan.glon_stp,
	      p->cfg->gal_latscan.glon_lo, p->cfg->gal_latscan.glon_hi,
	      p->cfg->gal_latscan.glat,
	      p->cfg->gal_latscan.n_avg, p->cfg->gal_latscan.n_rpt);

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

static void obs_assist_gal_latscan_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);

	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
		"This observation mode will perform a scan along the configured "
		"galactic latitude between the specified galactic "
		"longitudes.\n"
		"The resulting graph will show a velocity-longitude diagram "
		"with the spectral signal amplitudes encoded in colour.\n\n"
	       	"<b>Note:</b> The doppler velocity will be calculated from the "
		"reference rest frequency configured in the spectrometer "
		"settings. All velocities will be corrected for the Velocity "
		"of the Local Standard of Rest (VLSR) according to the line of "
		"sight.\n\n"
		"<b>Note:</b> While it is allowed to modify the spectrometer "
		"settings during the observation, changing the reference rest "
	        "frequency is not advised.\n\n"
		"<b>Note:</b> Unless configured otherwise, the observation "
		"procedure will skip any points along the scan line that are "
		"below the local horizon.");
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

static void obs_assist_gal_latscan_create_page_2(GtkAssistant *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;
	GtkSpinButton *sb;

	gdouble res;


	grid = GTK_GRID(new_default_grid());


	/** STEP **/
	w = gui_create_desclabel("Step Size",
				 "Specify the step size in degrees.");
	gtk_grid_attach(grid, w, 0, 0, 1, 1);

	/* determine some minimum step size */
	if (p->cfg->el_res < p->cfg->az_res)
		res = p->cfg->az_res;
	else
		res = p->cfg->el_res;

	/* set an arbitrary limit of 100 * res degrees for step size */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(res, 10., 0.1));
	gtk_spin_button_set_value(sb, 2.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 0, 1, 1);
	p->cfg->gal_latscan.sb_deg = sb;


	/** GLON lower bound **/
	w = gui_create_desclabel("Galactic Longitude Start",
				 "Specify the lower bound of the observation.");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set some arbitrary limits */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0., 360., 0.1));
	gtk_spin_button_set_value(sb, 50.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->gal_latscan.sb_lo = sb;


	/** GLON upper bound **/
	w = gui_create_desclabel("Galactic Longitude Stop",
				 "Specify the upper bound of the observation.");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0., 360., 0.1));
	gtk_spin_button_set_value(sb, 250.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 2, 1, 1);
	p->cfg->gal_latscan.sb_hi = GTK_SPIN_BUTTON(sb);

	/** Galactic Latitude Offset **/
	w = gui_create_desclabel("Galactic Latitude",
	                         "Specify the galactic latitude in degrees.");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-90.0, 90.0, 0.1));
	gtk_spin_button_set_value(sb, 0.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 3, 1, 1);
	p->cfg->gal_latscan.sb_glat = GTK_SPIN_BUTTON(sb);

	/** Averages **/
	w = gui_create_desclabel("Samples per position",
				 "Specify the number of measurements to be "
				 "averaged at each position.");
	gtk_grid_attach(grid, w, 0, 4, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 20, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 4, 1, 1);
	p->cfg->gal_latscan.sb_avg = GTK_SPIN_BUTTON(sb);


	/** Repeat **/
	w = gui_create_desclabel("Scan Repeats",
				 "Specify the number of times to repeat the "
				 "observation run.");
	gtk_grid_attach(grid, w, 0, 5, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 20, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 5, 1, 1);
	p->cfg->gal_latscan.sb_rpt = GTK_SPIN_BUTTON(sb);


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

static void obs_assist_gal_latscan_create_page_3(GtkAssistant *as)
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

static void obs_assist_gal_latscan_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);

	p->cfg->abort = FALSE;
	memset(&p->cfg->gal_latscan, 0, sizeof(p->cfg->gal_latscan));

	/* info page */
	obs_assist_gal_latscan_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_gal_latscan_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_gal_latscan_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_gal_latscan), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create gal_latscan scan selection
 */

GtkWidget *obs_assist_gal_latscan_scan_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Galactic Latitude Scan",
				 "Perform a scan along a "
				 "galactic latitude.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Galactic Latitude Scan.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_gal_latscan_setup_cb), p);


	return GTK_WIDGET(grid);
}
