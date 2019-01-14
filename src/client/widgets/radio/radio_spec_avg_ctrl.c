/**
 * @file    widgets/radio/radio_spec_avg_ctrl.c
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

#include <default_grid.h>
#include <desclabel.h>


#include <signals.h>

/**
 * @brief handle capabilities data and hide the average controls if the
 *        supported stack range is 0
 */

static void radio_spec_avg_handle_pr_capabilities(gpointer instance,
						  const struct capabilities *c,
						  gpointer data)
{
	GtkWidget *p;


	p = GTK_WIDGET(data);

	if (!c->n_stack_max)
		gtk_widget_hide(p);
	else
		gtk_widget_show_all(p);
}


/**
 * @brief create spectral averaging controls
 */

GtkWidget *radio_spec_avg_ctrl_new(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *tmp;


	grid = GTK_GRID(new_default_grid());



	w = gui_create_desclabel("Spectrum Averages",
				 "Configure the number of recorded spectrae to "
				 "stack and average on the receiver");


	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(grid, w, 0, 0, 1, 3);

	tmp = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	p->cfg->avg_cfg = GTK_LABEL(tmp);


	w = gtk_spin_button_new(NULL, 1, 1);
	gtk_widget_set_tooltip_text(w, "Set the number of acquired\n"
				       "spectrae to average");
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 1, 10);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), 0, 0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_entry_set_width_chars(GTK_ENTRY(w), 3);

	gtk_grid_attach(grid, w, 2, 1, 1, 1);

	p->cfg->sb_avg = GTK_SPIN_BUTTON(w);


	g_signal_connect(sig_get_instance(), "pr-capabilities",
			 G_CALLBACK(radio_spec_avg_handle_pr_capabilities),
			 (void *) grid);

	return GTK_WIDGET(grid);
}

