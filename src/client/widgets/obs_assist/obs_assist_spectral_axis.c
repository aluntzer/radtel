/**
 * @file    widgets/obs_assist/obs_assist_spectral_axis.c
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
 * @brief a create a spectral scan along one telescope axis
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
 * @brief select telescope axis
 */

void obs_assist_on_spectral_axis_select(GtkWidget *w, ObsAssist *p)
{
	GtkToggleButton *b;


	b = GTK_TOGGLE_BUTTON(w);

	p->cfg->spectral_axis.ax = gtk_toggle_button_get_active(b);
/* XXX */
	p->cfg->spectral_axis.ax = TRUE;
}


/**
 * @brief update the ax progress bar
 */

static void spectral_axis_update_pbar_ax(ObsAssist *p)
{
	gdouble frac;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->spectral_axis.pbar_ax);

	frac = (p->cfg->spectral_axis.ax_cur - p->cfg->spectral_axis.ax_lo) /
	       (p->cfg->spectral_axis.ax_hi - p->cfg->spectral_axis.ax_lo);

	str = g_strdup_printf("ax: %5.2f° of [%5.2f°, %5.2f°]",
			      p->cfg->spectral_axis.ax_cur,
			      p->cfg->spectral_axis.ax_lo,
			      p->cfg->spectral_axis.ax_hi);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief update the frequency-degree graph
 */

static void spectral_axis_draw_graph(ObsAssist *p, gdouble ax, struct spectrum *s)
{
	gsize i;

	gdouble *axdeg;


	axdeg = (gdouble *) g_malloc(s->n * sizeof(gdouble));


	for (i = 0; i < s->n; i++)
		axdeg[i] = ax;


	xyplot_add_graph(p->cfg->spectral_axis.plt, axdeg, s->x, s->y, s->n,
			 g_strdup_printf("ax %g", ax));

	xyplot_redraw(p->cfg->spectral_axis.plt);
}


/**
 * @brief verify position and issue move command if necessary
 *
 * @param az the actual target Azimuth
 * @param el the actual target Elevation
 *
 * @returns TRUE if in position
 */

static gboolean spectral_axis_in_position(ObsAssist *p, gdouble az, gdouble el)
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
 */

static gboolean spectral_axis_measure(ObsAssist *p)
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
	if (samples < p->cfg->spectral_axis.n_avg) {
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

	spectral_axis_draw_graph(p, p->cfg->spectral_axis.ax_cur, sp);

	g_free(sp);
	sp = NULL; /* plot takes care of data deallocation */
	samples = 0;

	obs_assist_clear_spec(p);

	return TRUE;
}


/**
 * @brief move into position on axis
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean spectral_axis_obs_pos(ObsAssist *p)
{
	gdouble ax_lim;


	spectral_axis_update_pbar_ax(p);

	/* upper bound reached?  */
	ax_lim = p->cfg->spectral_axis.ax_cur;
	if ((p->cfg->spectral_axis.ax_hi < ax_lim) ||
	    (p->cfg->spectral_axis.ax_lo > ax_lim))
		return FALSE;


	/* XXX currently Azimuth only! */
	if (!spectral_axis_in_position(p, p->cfg->spectral_axis.ax_cur, p->cfg->el))
		return TRUE;

	if (!spectral_axis_measure(p))
		return TRUE;

	obs_assist_clear_spec(p);

	/* update axis */
	p->cfg->spectral_axis.ax_cur += p->cfg->spectral_axis.ax_stp;

	return TRUE;
}


/**
 * @brief scan along plane
 */

static gboolean spectral_axis_obs(void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	if (p->cfg->abort) {
		/* we stay where we are */
		return G_SOURCE_REMOVE;
	}

	if (spectral_axis_obs_pos(p))
		return G_SOURCE_CONTINUE;


	/* on final, we stay at the current position */

	/* done */

	return G_SOURCE_REMOVE;
}


/**
 * @brief start the spectral_axis observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	sig_tracking(FALSE, 0.0, 0.0);

	obs_assist_hide_procedure_selectors(p);


	grid = GTK_GRID(new_default_grid());

	p->cfg->spectral_axis.plt = xyplot_new();
	/* XXX fixed to AZ*/
	xyplot_set_xlabel(p->cfg->spectral_axis.plt, "Azimuth [deg]");
	xyplot_set_ylabel(p->cfg->spectral_axis.plt, "Frequency [MHz]");

	gtk_widget_set_hexpand(p->cfg->spectral_axis.plt, TRUE);
	gtk_widget_set_vexpand(p->cfg->spectral_axis.plt, TRUE);
	gtk_grid_attach(grid, p->cfg->spectral_axis.plt, 0, 0, 2, 1);
	gtk_widget_set_size_request(p->cfg->spectral_axis.plt, -1, 300);


	w = gtk_label_new("Scan");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);
	p->cfg->spectral_axis.pbar_ax = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->spectral_axis.pbar_ax, TRUE);
	gtk_grid_attach(grid, p->cfg->spectral_axis.pbar_ax, 1, 1, 1, 1);


	w = gtk_button_new_with_label("Quit");
	gtk_widget_set_tooltip_text(w, "Quit observation");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_abort), p);


	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));

	/* the actual work is done asynchronously, .5 seconds calls
	 * per should be fine
	 */
	g_timeout_add(500, spectral_axis_obs, p);
}


/**
 * @brief set up the galactic plane observation
 */

static void obs_assist_on_prepare_spectral_axis(GtkWidget *as, GtkWidget *pg,
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

	/* XXX */
	p->cfg->spectral_axis.ax = TRUE;

	/* set configuration */
	sb = p->cfg->spectral_axis.sb_deg;
	p->cfg->spectral_axis.ax_stp = gtk_spin_button_get_value(sb);

	sb = p->cfg->spectral_axis.sb_lo;
	p->cfg->spectral_axis.ax_lo = gtk_spin_button_get_value(sb);

	sb = p->cfg->spectral_axis.sb_hi;
	p->cfg->spectral_axis.ax_hi = gtk_spin_button_get_value(sb);

	sb = p->cfg->spectral_axis.sb_avg;
	p->cfg->spectral_axis.n_avg = gtk_spin_button_get_value_as_int(sb);


	/* swap around */
	if (p->cfg->spectral_axis.ax_lo > p->cfg->spectral_axis.ax_hi) {
		tmp = p->cfg->spectral_axis.ax_hi;
		p->cfg->spectral_axis.ax_hi = p->cfg->spectral_axis.ax_lo;
		p->cfg->spectral_axis.ax_lo = tmp;
	}


	/* initial scan position is at lower bound */
	p->cfg->spectral_axis.ax_cur = p->cfg->spectral_axis.ax_lo;


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
	      "Axis lower bound:          <b>%5.2f°</b>\n"
	      "Axis upper bound:          <b>%5.2f°</b>\n"
	      "Samples per position:      <b>%d</b>\n"
	      "Scan Axis:                 <b>%s</b>\n"
	      "</tt>",
	      p->cfg->spectral_axis.ax_stp,
	      p->cfg->spectral_axis.ax_lo, p->cfg->spectral_axis.ax_hi,
	      p->cfg->spectral_axis.n_avg,
	      p->cfg->spectral_axis.ax ? "AZIMUTH" : "ELEVATION");

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

static void obs_assist_spectral_axis_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);

	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
		"This observation mode will perform a scan along a telescope "
		"axis. \n"
		"The resulting graph will show a frequency-angle diagram "
		"with the spectral signal amplitudes encoded in colour.\n\n"
	       	"<b>Note:</b> PRELIMINARY! SCANS IN AZIMUTH ONLY!\n\n");

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

static void obs_assist_spectral_axis_create_page_2(GtkAssistant *as, ObsAssist *p)
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
	p->cfg->spectral_axis.sb_deg = sb;


	/** ax lower bound **/
	w = gui_create_desclabel("Axis Start",
				 "Specify the lower bound of the observation.");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set some arbitrary limits */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0., 360., 0.1));
	gtk_spin_button_set_value(sb, 180.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->spectral_axis.sb_lo = sb;


	/** ax upper bound **/
	w = gui_create_desclabel("Axis Stop",
				 "Specify the upper bound of the observation.");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0., 360., 0.1));
	gtk_spin_button_set_value(sb, 190.);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 2, 1, 1);
	p->cfg->spectral_axis.sb_hi = GTK_SPIN_BUTTON(sb);

	/** Averages **/
	w = gui_create_desclabel("Samples per position",
				 "Specify the number of measurements to be "
				 "averaged at each position.");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 20, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 3, 1, 1);
	p->cfg->spectral_axis.sb_avg = GTK_SPIN_BUTTON(sb);


	w = gui_create_desclabel("<b> NOT IMPLEMENTED </b>",
				 "Select Axis");
	gtk_grid_attach(grid, w, 0, 5, 1, 1);

	w = gtk_check_button_new_with_label("Azimuth");
	g_signal_connect(G_OBJECT(w), "toggled",
				 G_CALLBACK(obs_assist_on_spectral_axis_select), p);
	gtk_grid_attach(grid, w, 1, 5, 1, 1);





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

static void obs_assist_spectral_axis_create_page_3(GtkAssistant *as)
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

static void obs_assist_spectral_axis_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);

	p->cfg->abort = FALSE;
	memset(&p->cfg->spectral_axis, 0, sizeof(p->cfg->spectral_axis));

	/* info page */
	obs_assist_spectral_axis_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_spectral_axis_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_spectral_axis_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_spectral_axis), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create spectral_axis scan selection
 */

GtkWidget *obs_assist_spectral_axis_scan_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Axis Scan",
				 "Perform a spectral scan along "
				 "a telescope axis.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Axis Scan.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_spectral_axis_setup_cb), p);


	return GTK_WIDGET(grid);
}
