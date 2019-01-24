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

#include <default_grid.h>
#include <desclabel.h>



/**
 * @brief signal handler for set position button press event
 */

static void telescope_set_pos_cb(GtkWidget *w, Telescope *p)
{
	double az;
	double el;


	az = gtk_spin_button_get_value(p->cfg->sb_az);
	el = gtk_spin_button_get_value(p->cfg->sb_el);

	cmd_moveto_azel(PKT_TRANS_ID_UNDEF, az, el);
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
