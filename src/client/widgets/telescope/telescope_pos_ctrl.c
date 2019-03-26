/**
 * @file    widgets/telescope/telescope_pos_ctrl.c
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
 */


#include <telescope.h>
#include <telescope_cfg.h>
#include <telescope_internal.h>

#include <default_grid.h>
#include <desclabel.h>
#include <signals.h>
#include <coordinates.h>


static gboolean telescope_getpos_azel_cb(gpointer instance, struct getpos *pos,
					 Telescope *p)
{
	gint prev;

	gdouble az, el;

	/* block updater */
	g_signal_handlers_block_matched(sig_get_instance(),
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					telescope_getpos_azel_cb, p);

	az = (gdouble) pos->az_arcsec / 3600.0;
	el = (gdouble) pos->el_arcsec / 3600.0;

	prev = gtk_combo_box_get_active(p->cfg->coord_ref_cb);

	/* change mode to horizontal for proper conversion */
	gtk_combo_box_set_active(p->cfg->coord_ref_cb, 0);
	gtk_spin_button_set_value(p->cfg->sb_az, az);
	gtk_spin_button_set_value(p->cfg->sb_el, el);

	/* and back to whatever */
	gtk_combo_box_set_active(p->cfg->coord_ref_cb, 0);
	gtk_combo_box_set_active(p->cfg->coord_ref_cb, prev);


	return TRUE;
}


/**
 * @brief signal handler for get position button press event
 */

static void telescope_get_pos_cb(GtkWidget *w, Telescope *p)
{
	/* unblock updater */
	g_signal_handlers_unblock_matched(sig_get_instance(),
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					telescope_getpos_azel_cb, p);

	cmd_getpos_azel(PKT_TRANS_ID_UNDEF);
}


/**
 * @brief create telescope get position button
 */

GtkWidget *telescope_get_pos_new(Telescope *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Retrieve Telescope Pointing",
				 "Fetch the current telescope coordinates from "
				 "the server and update the input box.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Get Coordinates");
	gtk_widget_set_tooltip_text(w, "Get position\nfrom server");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(telescope_get_pos_cb), p);

	/* connect the pr-getpos-azel and block the handler */
	g_signal_connect(sig_get_instance(), "pr-getpos-azel",
			 (GCallback) telescope_getpos_azel_cb,
			 (gpointer) p);

	g_signal_handlers_block_matched(sig_get_instance(),
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					telescope_getpos_azel_cb, p);
	return GTK_WIDGET(grid);
}


/**
 * @brief signal handler for set position button press event
 */

static void telescope_set_pos_cb(GtkWidget *w, Telescope *p)
{
	struct coord_horizontal hor;
	struct coord_equatorial	equ;


	telescope_update_azel_internal(p);

	hor.az = gtk_spin_button_get_value(p->cfg->sb_az);
	hor.el = gtk_spin_button_get_value(p->cfg->sb_el);


	/* update tracked position */
	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);
	p->cfg->track_ra = equ.ra;
	p->cfg->track_de = equ.dec;

	/* if tracking is on, let the tracker do the move */
	if (p->cfg->tracking)
		return;

	/* otherwise go to azel */
	cmd_moveto_azel(PKT_TRANS_ID_UNDEF, hor.az, hor.el);
}


/**
 * @brief create telescope set position button
 */

GtkWidget *telescope_pos_ctrl_new(Telescope *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Move Telescope",
				 "Slew to the specified coordinates");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Go to Coordinates");
	gtk_widget_set_tooltip_text(w, "Send position\nto server");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(telescope_set_pos_cb), p);


	return GTK_WIDGET(grid);
}



/**
 * @brief signal handler for PARK button press event
 */

static void telescope_park_cb(GtkWidget *w, Telescope *p)
{
	cmd_park_telescope(PKT_TRANS_ID_UNDEF);
}


/**
 * @brief create telescope PARK button
 */

GtkWidget *telescope_park_ctrl_new(Telescope *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Park Telescope",
				 "Move telescope to park position");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Stow Telescope");
	gtk_widget_set_tooltip_text(w, "Drive to park position");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(telescope_park_cb), p);


	return GTK_WIDGET(grid);
}


/**
 * @brief signal handler for recalibrate pointing button press event
 */

static void telescope_recal_pointing_cb(GtkWidget *w, Telescope *p)
{
	cmd_recalibrate_pointing(PKT_TRANS_ID_UNDEF);
}


/**
 * @brief create telescope PARK button
 */

GtkWidget *telescope_recal_pointing_new(Telescope *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Recalibrate Telescope Pointing",
				 "Execute a calibration procedure if the "
				 "telescope's drive is suspected to be in "
				 "misalignment.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Recalibrate Drive");
	gtk_widget_set_tooltip_text(w, "Recalibrate drive position");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(telescope_recal_pointing_cb), p);


	return GTK_WIDGET(grid);
}


/**
 * @brief signal handler (tracker) for the timeout callback
 *
 */

static gboolean telescope_track_timeout_cb(void *data)
{
	static int once;

	struct coord_horizontal hor;
	struct coord_equatorial	equ;

	Telescope *p;



	p = (Telescope *) data;


	/* Attempt one position update on the remote, but otherwise do not
	 * send updates while the telescope is moving. This way we do not spam
	 * the server for no reason, but can still insert a position update
	 * if the current move different from our target location
	 */
	if (p->cfg->moving) {
		if (!once)
			return p->cfg->tracking;
		once++;
	}
	once = 0;

	equ.ra  = p->cfg->track_ra;
	equ.dec = p->cfg->track_de;

	hor = equatorial_to_horizontal(equ, p->cfg->lat, p->cfg->lon, 0.0);

	/* stop tracking if out of bounds */
	if (hor.az > p->cfg->az_max)
		p->cfg->tracking = G_SOURCE_REMOVE;

	if (hor.az < p->cfg->az_min)
		p->cfg->tracking = G_SOURCE_REMOVE;

	if (hor.el > p->cfg->el_max)
		p->cfg->tracking = G_SOURCE_REMOVE;

	if (hor.el < p->cfg->el_min)
		p->cfg->tracking = G_SOURCE_REMOVE;


	if ((fabs(hor.az - p->cfg->tgt_az) < p->cfg->az_res) &&
	     fabs(hor.el - p->cfg->tgt_el) < p->cfg->el_res)
		return p->cfg->tracking;

	if (p->cfg->tracking)
		cmd_moveto_azel(PKT_TRANS_ID_UNDEF, hor.az, hor.el);
	else
		gtk_switch_set_state(GTK_SWITCH(p->cfg->sw_trk), FALSE);

	return p->cfg->tracking;
}


/**
 * @brief signal handler for toggle switch
 */

static gboolean telescope_track_sky_toggle_cb(GtkWidget *w,
					      gboolean state, Telescope *p)
{
	const GSourceFunc sf = telescope_track_timeout_cb;


	if (p->cfg->track_ra == DBL_MIN) {
		gtk_switch_set_state(GTK_SWITCH(w), FALSE);

		return TRUE;
	}

	if (gtk_switch_get_active(GTK_SWITCH(w))) {

		if (p->cfg->tracking)	/* already at it */
			return TRUE;

		p->cfg->tracking = G_SOURCE_CONTINUE;
		p->cfg->id_to = g_timeout_add_seconds(1, sf, p);
	} else {
		g_source_remove(p->cfg->id_to);
		p->cfg->tracking = G_SOURCE_REMOVE;

		g_signal_emit_by_name(sig_get_instance(), "tracking", FALSE,
				      0.0, 0.0);

	}

	return FALSE;
}


/**
 * @brief callback to update current equatorial position for tracking
 */

void telescope_tracker_getpos_azel_cb(gpointer instance, struct getpos *pos,
					  Telescope *p)
{
	struct coord_horizontal hor;
	struct coord_equatorial	equ;


	p->cfg->az = (gdouble) pos->az_arcsec / 3600.0;
	p->cfg->el = (gdouble) pos->el_arcsec / 3600.0;


	/* do not update reference from external source while tracking */
	if (p->cfg->tracking)
		return;

	/* update only if it has not been set yet */
	if (p->cfg->track_ra != DBL_MIN)
		return;

	hor.az = p->cfg->az;
	hor.el = p->cfg->el;

	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);

	p->cfg->track_ra = equ.ra;
	p->cfg->track_de = equ.dec;

	return;
}

/**
 * @brief callback to update target equatorial position for tracking
 */

void telescope_tracker_moveto_azel_cb(gpointer instance,
					  gdouble az, gdouble el,
					  Telescope *p)
{
	struct coord_horizontal hor;
	struct coord_equatorial	equ;



	p->cfg->tgt_az = (gdouble) az;
	p->cfg->tgt_el = (gdouble) el;


	/* do not update reference from external source while tracking */
	if (p->cfg->tracking)
		return;

	hor.az = p->cfg->tgt_az;
	hor.el = p->cfg->tgt_el;

	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);

	p->cfg->track_ra = equ.ra;
	p->cfg->track_de = equ.dec;

	return;
}



void telescope_tracker_ctrl(gpointer instance, gboolean state,
			    gdouble az, gdouble el,
			    Telescope *p)
{
	struct coord_horizontal hor;
	struct coord_equatorial	equ;


	if (state == p->cfg->tracking)
		return;

	if (state) {
		hor.az = az;
		hor.el = el;

		equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);

		p->cfg->track_ra = equ.ra;
		p->cfg->track_de = equ.dec;
	}


	if (state)
		gtk_switch_set_state(p->cfg->sw_trk, TRUE);
	else
		gtk_switch_set_state(GTK_SWITCH(p->cfg->sw_trk), FALSE);

}


/**
 * @brief create telescope PARK button
 */

GtkWidget *telescope_track_sky_new(Telescope *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Track Sky",
				 "Keep the telescope pointed towards the "
				 "currently configured on-sky coordinates.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 2);

	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable Tracking\n");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);


	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(telescope_track_sky_toggle_cb), p);
	p->cfg->sw_trk = GTK_SWITCH(w);

	return GTK_WIDGET(grid);
}
