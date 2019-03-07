/**
 * @file    widgets/obs_assist/obs_assist_cross.c
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
#include <obs_assist_internal.h>

#include <coordinates.h>
#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <xyplot.h>
#include <cmd.h>
#include <math.h>




static gboolean cross_obs(void *data)
{
	ObsAssist *p;
	GtkProgressBar *pb;

	gdouble frac;
	gsize i;

	gdouble avg = 0.0;

	void *ref;


	p = OBS_ASSIST(data);

		pb = GTK_PROGRESS_BAR(p->cfg->cross.pbar_az);

	if (p->cfg->cross.az_cur < p->cfg->cross.az_max + p->cfg->cross.az_stp) {

		frac = (p->cfg->cross.az_cur - p->cfg->cross.az_min)/(p->cfg->cross.az_max - p ->cfg->cross.az_min);
		gtk_progress_bar_set_fraction(pb, frac);
		gtk_progress_bar_set_show_text(pb, TRUE);
		gtk_progress_bar_set_text(pb, g_strdup_printf("Offset: %5.2f°", (p->cfg->cross.az_cur - p->cfg->cross.az_cent) / p->cfg->cross.az_cor));


		g_message("%f < %f, scanning in Azimuth", p->cfg->cross.az_cur, p->cfg->cross.az_max);

		if ((fabs(p->cfg->cross.az_cur - p->cfg->az) > 2.0 * p->cfg->az_res)
		    || fabs(p->cfg->cross.el_cent - p->cfg->el) > 2.0 * p->cfg->el_res) {
			g_message("want AZEL %f, %f, am at %f %f", p->cfg->cross.az_cur, p->cfg->cross.el_cent, p->cfg->az, p->cfg->el);

			/* disable acquisition before moving */
			if (p->cfg->acq_enabled) {
				g_message("Acquisition is still enabled, disabling first\n");
				cmd_spec_acq_disable(PKT_TRANS_ID_UNDEF);
				obs_assist_clear_spec(p);
				return G_SOURCE_CONTINUE;
			}

			g_message("Sending move to AZEL\n");

			cmd_moveto_azel(PKT_TRANS_ID_UNDEF, p->cfg->cross.az_cur, p->cfg->el);

			return G_SOURCE_CONTINUE;
		}



		g_message("At position.\n");

		/* enable acquisition at current position */
		if (!p->cfg->acq_enabled) {
			g_message("Acquisition is still disabled, enabling first\n");

			cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
			return G_SOURCE_CONTINUE;
		}


		if (!p->cfg->spec.n) {
			g_message("Still waiting for spectral data...\n");
			return G_SOURCE_CONTINUE;
		}




		for (i = 0; i < p->cfg->spec.n; i++)
			avg += p->cfg->spec.y[i];

		avg = avg / (gdouble) p->cfg->spec.n;
		g_array_append_val(p->cfg->cross.az.off, p->cfg->cross.az_cur);
		g_array_append_val(p->cfg->cross.az.amp, avg);

		g_message("------append: %g %g\n", p->cfg->cross.az_cur, avg );

		obs_assist_clear_spec(p);

		g_message("took single shot of data at offset %f\n", (p->cfg->cross.az_cur - p->cfg->cross.az_cent) / p->cfg->cross.az_cor);

	//	xyplot_drop_all_graphs(p->cfg->cross.plt);

		ref = xyplot_add_graph(p->cfg->cross.plt,
				       (gdouble *) g_memdup(p->cfg->cross.az.off->data, p->cfg->cross.az.off->len * sizeof(gdouble)),
				       (gdouble *) g_memdup(p->cfg->cross.az.amp->data, p->cfg->cross.az.off->len * sizeof(gdouble)),
				       NULL,
				       p->cfg->cross.az.off->len,
				       "Azimuth Scan");

		g_message("<<<<<<<< len at %d\n", p->cfg->cross.az.off->len);


		xyplot_set_graph_style(p->cfg->cross.plt, ref, CURVES);


		p->cfg->cross.az_cur += p->cfg->cross.az_stp;
		g_message("updated AZ to %f", p->cfg->cross.az_cur);



		return G_SOURCE_CONTINUE;

	}

	g_message("CROSS DONE!");

	/* done */

	g_array_free(p->cfg->cross.az.off, FALSE);
	g_array_free(p->cfg->cross.az.amp, FALSE);
	g_array_free(p->cfg->cross.el.off, FALSE);
	g_array_free(p->cfg->cross.el.amp, FALSE);
#if 0
	gtk_widget_destroy(gtk_widget_get_parent(GTK_WIDGET(pb)));
	gtk_widget_show_all(GTK_WIDGET(p));
#endif
	return G_SOURCE_REMOVE;
}


/**
 * @brief start the cross observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	p->cfg->cross.az.off = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->cross.az.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->cross.el.off = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->cross.el.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));

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

	/* the actual work is done asynchronously */
	g_timeout_add(5000, cross_obs, p);
}


/**
 * @brief set up the cross observation
 */

static void obs_assist_on_prepare_cross(GtkWidget *as, GtkWidget *pg,
					ObsAssist *p)
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
		w = obs_assist_limits_exceeded_warning("upper", "elevation",
							  p->cfg->el_max);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (el_min < p->cfg->el_min) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("lower", "elevation",
							  p->cfg->el_min);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (az_max > p->cfg->az_max) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("right", "azimuth",
							  p->cfg->az_max);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (az_min < p->cfg->az_min) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("left", "azimuth",
							  p->cfg->az_min);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (!page_complete) {
		w = gtk_check_button_new_with_label("I choose to ignore any "
						    "warnings.");
		g_signal_connect(G_OBJECT(w), "toggled",
				 G_CALLBACK(obs_assist_on_ignore_warning), as);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}



	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, page_complete);

	gtk_widget_show_all(box);
}


/**
 * @brief create info page
 */

static void obs_assist_cross_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


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


	gtk_assistant_append_page(as, box);
	gtk_assistant_set_page_complete(as, box, TRUE);
	gtk_assistant_set_page_title(as, box, "Info");
	gtk_assistant_set_page_type(as, box, GTK_ASSISTANT_PAGE_INTRO);
}


/**
 * @brief create info page
 */

static void obs_assist_cross_create_page_2(GtkAssistant *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;
	GtkSpinButton *sb;

	gdouble res;


	grid = GTK_GRID(new_default_grid());


	/** STEP **/
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
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(res, 20., 0.1));
	gtk_spin_button_set_value(sb, 2.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 0, 1, 1);
	p->cfg->cross.sb_deg = sb;


	/** AZ **/
	w = gui_create_desclabel("Steps in Azimuth",
				 "Specify the number of steps in Azimuth. An "
				 "even number of steps will take samples only "
				 "left and right of the center");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set some arbitrary limits */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(2, 101, 1));
	gtk_spin_button_set_value(sb, 11);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->cross.sb_az = sb;


	/** EL **/
	w = gui_create_desclabel("Steps in Elevation",
				 "Specify the number of steps in Elevation. An "
				 "even number of steps will take samples only "
				 "above and below of the center");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(2, 101, 1));
	gtk_spin_button_set_value(sb, 11);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 2, 1, 1);
	p->cfg->cross.sb_el = GTK_SPIN_BUTTON(sb);


	/** XXX: TODO: track option checkbox */


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

static void obs_assist_cross_create_page_3(GtkAssistant *as)
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

static void obs_assist_cross_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);


	/* info page */
	obs_assist_cross_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_cross_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_cross_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_cross), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create cross scan selection
 */

GtkWidget *obs_assist_cross_scan_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


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
