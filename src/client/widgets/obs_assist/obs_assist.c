/**
 * @file    widgets/obs_assist/obs_assist.c
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
 * @brief a widget to show local and remote systems status info
 *
 */


#include <obs_assist.h>
#include <obs_assist_cfg.h>

#include <coordinates.h>
#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <xyplot.h>
#include <cmd.h>
#include <math.h>


G_DEFINE_TYPE_WITH_PRIVATE(ObsAssist, obs_assist, GTK_TYPE_BOX)


/**
 * @brief handle position data
 */

static gboolean obs_assist_getpos_azel_cb(gpointer instance, struct getpos *pos,
					  ObsAssist *p)
{
	p->cfg->az = (gdouble) pos->az_arcsec / 3600.0;
	p->cfg->el = (gdouble) pos->el_arcsec / 3600.0;


	return TRUE;
}


/**
 * @brief handle capabilities data
 */

static void obs_assist_handle_pr_capabilities(gpointer instance,
					      const struct capabilities *c,
					      gpointer data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	p->cfg->lat = (gdouble) c->lat_arcsec / 3600.0;
	p->cfg->lon = (gdouble) c->lon_arcsec / 3600.0;

	p->cfg->az_min = (gdouble) c->az_min_arcsec / 3600.0;
	p->cfg->az_max = (gdouble) c->az_max_arcsec / 3600.0;
	p->cfg->az_res = (gdouble) c->az_res_arcsec / 3600.0;

	p->cfg->el_min = (gdouble) c->el_min_arcsec / 3600.0;
	p->cfg->el_max = (gdouble) c->el_max_arcsec / 3600.0;
	p->cfg->el_res = (gdouble) c->el_res_arcsec / 3600.0;
}





static GtkWidget *obs_assistant_limits_exceeded_warning(const gchar *direction,
							const gchar *axis,
							const double limit_deg)
{
	GtkWidget *w;

	gchar *lbl;


	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
	lbl = g_strdup_printf(
	      "<span foreground=\"red\" size=\"large\">WARNING:</span> "
	      "Your configuration exceeds the %s limit of the %s "
	      "drive: <tt>%5.2f°</tt>", direction, axis, limit_deg);
	gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);

	gtk_widget_set_halign(w, GTK_ALIGN_START);

	return w;
}


static void on_ignore_warning(GtkWidget *w, gpointer data)
{
	GtkWidget *cp;
	GtkAssistant *as;

	gint pn;


       	as = GTK_ASSISTANT(data);

	pn = gtk_assistant_get_current_page(as);
	cp = gtk_assistant_get_nth_page(as, pn);

	gtk_assistant_set_page_complete(as, cp, TRUE);
}



static void on_assistant_close_cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(data));
}


static gboolean cross_obs(void *data)
{
	ObsAssist *p;
	GtkProgressBar *pb;

	gdouble frac;


	p = OBS_ASSIST(data);

		pb = GTK_PROGRESS_BAR(p->cfg->cross.pbar_az);

	if (p->cfg->cross.az_cur < p->cfg->cross.az_max + p->cfg->cross.az_stp) {

		frac = (p->cfg->cross.az_cur - p->cfg->cross.az_min)/(p->cfg->cross.az_max - p ->cfg->cross.az_min);
		gtk_progress_bar_set_fraction(pb, frac);
		gtk_progress_bar_set_show_text(pb, TRUE);
		gtk_progress_bar_set_text(pb, g_strdup_printf("Offset: %5.2f°", (p->cfg->cross.az_cur - p->cfg->cross.az_cent) / p->cfg->cross.az_cor));


		g_message("%f < %f, scanning in Azimuth", p->cfg->cross.az_cur, p->cfg->cross.az_max);

		if ((fabs(p->cfg->cross.az_cur - p->cfg->az) > p->cfg->az_res)
		    || fabs(p->cfg->cross.el_cent - p->cfg->el) > p->cfg->el_res) {
			g_message("want AZEL %f, %f, am at %f %f, sending move command", p->cfg->cross.az_cur, p->cfg->cross.el_cent, p->cfg->az, p->cfg->el);
#if 0
			cmd_moveto_azel(PKT_TRANS_ID_UNDEF, az, p->cfg->el);
#else
			p->cfg->az = p->cfg->cross.az_cur;	/* fake update */
#endif
			return G_SOURCE_CONTINUE;
		}

		/** XXX fake a single shot */
		g_message("taking single shot of data at offset %f\n", (p->cfg->cross.az_cur - p->cfg->cross.az_cent) / p->cfg->cross.az_cor);
		/** XXX this must be done in a callback */

		p->cfg->cross.az_cur += p->cfg->cross.az_stp;

		g_message("updated AZ to %f", p->cfg->cross.az_cur);

		return G_SOURCE_CONTINUE;

	}

	g_message("DONE!");
	/* done */
	gtk_widget_destroy(gtk_widget_get_parent(GTK_WIDGET(pb)));
	gtk_widget_show_all(GTK_WIDGET(p));
	return G_SOURCE_REMOVE;
}


static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkWidget *parent;

	GtkGrid *grid;


	gtk_container_foreach(GTK_CONTAINER(p),
			      (GtkCallback) gtk_widget_hide, NULL);

	grid = GTK_GRID(new_default_grid());

	p->cfg->cross.plt = xyplot_new();
	xyplot_set_xlabel(p->cfg->cross.plt, "Offset");
	xyplot_set_ylabel(p->cfg->cross.plt, "Amplitude");

	gtk_widget_set_hexpand(p->cfg->cross.plt, TRUE);
	gtk_widget_set_vexpand(p->cfg->cross.plt, TRUE);
	gtk_grid_attach(grid, p->cfg->cross.plt, 0, 0, 3, 1);

	w = gtk_label_new("Azimuth Scan");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);
	p->cfg->cross.pbar_az = gtk_progress_bar_new();
	gtk_grid_attach(grid, p->cfg->cross.pbar_az, 1, 1, 2, 1);

	w = gtk_label_new("Elevation Scan");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);
	p->cfg->cross.pbar_el = gtk_progress_bar_new();
	gtk_grid_attach(grid, p->cfg->cross.pbar_el, 1, 2, 2, 1);


	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));

	g_timeout_add(1000, cross_obs, p);
}


static void on_assistant_prepare(GtkWidget *as, GtkWidget *pg, ObsAssist *p)
{
	gint cp;

	GtkWidget *w;
	GtkWidget *box;

	GtkAssistantPageType type;

	gchar *lbl;

	gdouble az_min;
	gdouble az_max;
	gdouble el_min;
	gdouble el_max;
	gdouble az_cor;
	gdouble az_off;
	gdouble az_stp;
	gdouble el_off;

	gdouble page_complete = TRUE;


	type = gtk_assistant_get_page_type(GTK_ASSISTANT(as), pg);
	if (type != GTK_ASSISTANT_PAGE_CONFIRM)
		return;


	p->cfg->cross.az_pt = gtk_spin_button_get_value(p->cfg->cross.sb_az);
	p->cfg->cross.el_pt = gtk_spin_button_get_value(p->cfg->cross.sb_az);
	p->cfg->cross.deg   = gtk_spin_button_get_value(p->cfg->cross.sb_deg);


	az_cor = 1.0 / cos(RAD(p->cfg->el));
	az_off = p->cfg->cross.deg * 0.5 * (p->cfg->cross.az_pt - 1.0);
	az_min = p->cfg->az - az_off * az_cor;
	az_max = p->cfg->az + az_off * az_cor;
	az_stp = (az_max - az_min)  / (p->cfg->cross.az_pt - 1.0);
	g_warning("off: %f step: %f", az_off, az_stp);

	el_off = p->cfg->cross.deg * 0.5 * (p->cfg->cross.el_pt - 1.0);
	el_min = p->cfg->el - el_off;
	el_max = p->cfg->el + az_off;


	p->cfg->cross.az_min = az_min;
	p->cfg->cross.az_max = az_max;
	p->cfg->cross.el_min = el_min;
	p->cfg->cross.el_max = el_max;
	p->cfg->cross.az_cor = az_cor;
	p->cfg->cross.az_off = az_off;
	p->cfg->cross.az_stp = az_stp;
	p->cfg->cross.el_off = el_off;

	p->cfg->cross.az_cur = az_min;
	p->cfg->cross.el_cur = el_min;

	p->cfg->cross.az_cent = p->cfg->az;
	p->cfg->cross.el_cent = p->cfg->el;



	cp  = gtk_assistant_get_current_page(GTK_ASSISTANT(as));
      	box = gtk_assistant_get_nth_page(GTK_ASSISTANT(as), cp);

	gtk_container_foreach(GTK_CONTAINER(box),
			      (GtkCallback) gtk_widget_destroy, NULL);

	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
	      "This is your configuration:\n\n"
	      "<tt>"
	      "Scan points in Azimuth:    <b>%5.0f</b>\n"
	      "Scan points in Elevation:  <b>%5.0f</b>\n"
	      "Nominal step size:         <b>%5.2f°</b>\n"
	      "Corrected Azimuth step:    <b>%5.2f°</b>\n\n"
	      "Center Azimuth:          <b>%5.2f°</b>\n"
	      "Center Elevation:        <b>%5.2f°</b>\n"
	      "Scan range in Azimuth:   <b>%5.2f°</b> to <b>%5.2f°</b>\n"
	      "Scan range in Elevation: <b>%5.2f°</b> to <b>%5.2f°</b>\n"
	      "</tt>",
	      p->cfg->cross.az_pt, p->cfg->cross.el_pt,
	      p->cfg->cross.deg, az_stp,
	      p->cfg->cross.az_cent, p->cfg->cross.el_cent,
	      az_min, az_max, el_min, el_max);
        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_set_halign(w, GTK_ALIGN_START);


	if (el_max > p->cfg->el_max) {
		page_complete = FALSE;
		w = obs_assistant_limits_exceeded_warning("upper", "elevation",
							  p->cfg->el_max);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (el_min < p->cfg->el_min) {
		page_complete = FALSE;
		w = obs_assistant_limits_exceeded_warning("lower", "elevation",
							  p->cfg->el_min);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (az_max > p->cfg->az_max) {
		page_complete = FALSE;
		w = obs_assistant_limits_exceeded_warning("right", "azimuth",
							  p->cfg->az_max);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (az_min < p->cfg->az_min) {
		page_complete = FALSE;
		w = obs_assistant_limits_exceeded_warning("left", "azimuth",
							  p->cfg->az_min);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (!page_complete) {
		w = gtk_check_button_new_with_label("I choose to ignore any "
						    "warnings.");
		g_signal_connect(G_OBJECT(w), "toggled",
				 G_CALLBACK(on_ignore_warning), as);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}



	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, page_complete);

	gtk_widget_show_all(box);
}


static void obs_assist_cross_setup_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *box;
	GtkWidget *sb;

	GtkWidget *w;
	GtkWidget *as;
	GtkGrid *grid;

	gchar *title;

	gint width, height;
	gchar *lbl;

	gdouble res;


	as = g_object_new(GTK_TYPE_ASSISTANT, "use-header-bar", TRUE, NULL);

	gtk_window_get_size(GTK_WINDOW(gtk_widget_get_toplevel(widget)), &width, &height);
	/* XXX assert toplevel == type_window */
	gtk_window_set_transient_for(GTK_WINDOW(as),
				     GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gtk_window_set_modal(GTK_WINDOW(as), TRUE);
	gtk_window_set_position(GTK_WINDOW(as), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_window_set_attached_to(GTK_WINDOW(as),
				   gtk_widget_get_toplevel(widget));

	gtk_window_set_default_size(GTK_WINDOW(as),
				    (2 * width) / 3, (2 * height) / 3);

	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(on_assistant_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(on_assistant_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(on_assistant_prepare), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);



	/* info page */
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

        lbl = g_strdup_printf(
		"This observation mode will perform a scan in the shape of a "
		"cross around the current on-sky position of the telescope.\n\n"
		"<b>Note:</b> The central position will be tracked at the "
		"sidereal rate. The resulting graphs will be in Azimuth and "
		"Elevation offsets from the centeral position."
		"Azimuth distance will be corrected for the cosine of the "
		"Elevation for actual angular distance.");
        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);

	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_show_all(box);

	gtk_assistant_append_page(GTK_ASSISTANT(as), box);
	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, TRUE);
	gtk_assistant_set_page_title(GTK_ASSISTANT(as), box, "Info");
	gtk_assistant_set_page_type(GTK_ASSISTANT(as), box,
				    GTK_ASSISTANT_PAGE_INTRO);



	/* settings page */

	/** STEP **/
	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Step Size",
				 "Specify the step size in degrees. This "
				 "setting will apply to both scan directions.");
	gtk_grid_attach(grid, w, 0, 0, 1, 1);

	/* determine some minimum step size */
	if (p->cfg->el_res < p->cfg->az_res)
		res = p->cfg->az_res;
	else
		res = p->cfg->el_res;

	/* set an arbitrary limit of 20 degrees for step size */
	sb = gtk_spin_button_new_with_range(res, 20., 0.1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb), 2.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_widget_set_valign(sb, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, sb, 1, 0, 1, 1);
	p->cfg->cross.sb_deg = GTK_SPIN_BUTTON(sb);



	/** AZ **/
	w = gui_create_desclabel("Steps in Azimuth",
				 "Specify the number of steps in Azimuth. An "
				 "even number of steps will take samples only "
				 "left and right of the center");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set some arbitrary limits */
	sb = gtk_spin_button_new_with_range(2, 101, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb), 11);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_widget_set_valign(sb, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, sb, 1, 1, 1, 1);
	p->cfg->cross.sb_az = GTK_SPIN_BUTTON(sb);


	/** EL **/
	w = gui_create_desclabel("Steps in Elevation",
				 "Specify the number of steps in Elevation. An "
				 "even number of steps will take samples only "
				 "above and below of the center");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = gtk_spin_button_new_with_range(2, 101, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb), 11);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_widget_set_valign(sb, GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, sb, 1, 2, 1, 1);


	gtk_widget_show_all(GTK_WIDGET(grid));
	gtk_assistant_append_page(GTK_ASSISTANT(as), GTK_WIDGET(grid));
	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), GTK_WIDGET(grid), TRUE);
	gtk_assistant_set_page_title(GTK_ASSISTANT(as), GTK_WIDGET(grid), "Setup");
	gtk_assistant_set_page_type(GTK_ASSISTANT(as), GTK_WIDGET(grid),
				    GTK_ASSISTANT_PAGE_CONTENT);
	p->cfg->cross.sb_el = GTK_SPIN_BUTTON(sb);




	/** summary page **/
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_widget_show_all(box);

	gtk_assistant_append_page(GTK_ASSISTANT(as), box);
	gtk_assistant_set_page_title(GTK_ASSISTANT(as), box, "Confirm");
	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, TRUE);
	gtk_assistant_set_page_type(GTK_ASSISTANT(as), box,
				    GTK_ASSISTANT_PAGE_CONFIRM);




	gtk_widget_show(as);
}


/**
 * @brief create cross scan selection
 */

GtkWidget *ob_assist_cross_scan_new(ObsAssist *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Cross Scan",
				 "Perform a scan around a source in azimuth "
				 "and elevation.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Cross Scan.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);


	return GTK_WIDGET(grid);
}


/**
 * @brief create galactic plane scan selection
 */

GtkWidget *ob_assist_gal_line_scan_new(ObsAssist *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Galactic Line Scan",
				 "Scan along galactic longitude.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Galactic Line Scan");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);


	return GTK_WIDGET(grid);
}


/**
 * @brief create beam switching selection
 */

GtkWidget *ob_assist_beam_switching_new(ObsAssist *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Beam Switching",
				 "Perform a beam switching observation");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start beam switching");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);


	return GTK_WIDGET(grid);
}


static void gui_create_obs_assist_controls(ObsAssist *p)
{
	GtkWidget *w;

	w = ob_assist_cross_scan_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
	w = ob_assist_gal_line_scan_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
	w = ob_assist_beam_switching_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
}


/**
 * @brief destroy signal handler
 */

static gboolean obs_assist_destroy(GtkWidget *w, void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(w);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_pos);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cap);

	return TRUE;
}


/**
 * @brief initialise the ObsAssist class
 */

static void obs_assist_class_init(ObsAssistClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the ObsAssist widget
 */

static void obs_assist_init(ObsAssist *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_OBS_ASSIST(p));

	p->cfg = obs_assist_get_instance_private(p);

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_obs_assist_controls(p);


	p->cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
					  G_CALLBACK(obs_assist_handle_pr_capabilities),
					  (void *) p);

	p->cfg->id_pos = g_signal_connect(sig_get_instance(), "pr-getpos-azel",
					  (GCallback) obs_assist_getpos_azel_cb,
					  (gpointer) p);

	g_signal_connect(p, "destroy", G_CALLBACK(obs_assist_destroy), NULL);
}


/**
 * @brief create a new ObsAssist widget
 */

GtkWidget *obs_assist_new(void)
{
	ObsAssist *obs_assist;


	obs_assist = g_object_new(TYPE_OBS_ASSIST, NULL);

	return GTK_WIDGET(obs_assist);
}
