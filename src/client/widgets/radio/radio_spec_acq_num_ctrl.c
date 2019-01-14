/**
 * @file    widgets/radio/radio_spec_acq_num_ctrl.c
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


#include <radio.h>
#include <radio_cfg.h>
#include <radio_internal.h>

#include <default_grid.h>
#include <desclabel.h>


#include <signals.h>


/**
 * @brief create spectrum acquisition controls
 */

GtkWidget *radio_spec_acq_num_ctrl_new(Radio *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Acquisition Limit",
				 "Configure the maximum number of spectrae to "
				 "acquire before stopping the observation.\n"
				 "Note that a limit of 0 will enable infinite "
				 "acquisition.");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 2);


	w = gtk_label_new("Acquisitions");
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);

	w = gtk_spin_button_new(NULL, 1, 10);
	gtk_widget_set_tooltip_text(w, "Set the number of spectrae to acquire\n"
				       "0 = No Limit");
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 1, 10);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), 0, 1000);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);

	gtk_grid_attach(grid, w, 2, 0, 1, 1);

	p->cfg->sb_limit = GTK_SPIN_BUTTON(w);


	return GTK_WIDGET(grid);
}

