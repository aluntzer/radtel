/**
 * @file    widgets/radio/radio_spec_cfg_ctrl.c
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
 * @brief signal handler for GET config button press event
 */

static void radio_spec_cfg_get_cb(GtkWidget *w, Radio *p)
{
	cmd_spec_acq_cfg_get(PKT_TRANS_ID_UNDEF);
}


/**
 * @brief signal handler for SET config button press event
 */

static void radio_spec_cfg_set_cb(GtkWidget *w, Radio *p)
{
	uint64_t f0;
	uint64_t f1;


	f0 = (uint64_t) (gtk_spin_button_get_value(p->cfg->sb_frq_lo) * 1e6);
	f1 = (uint64_t) (gtk_spin_button_get_value(p->cfg->sb_frq_hi) * 1e6);

	radio_update_avg_lbl(p);
	radio_update_conf_freq_lbl(p);
	radio_update_conf_bw_lbl(p);

	cmd_spec_acq_cfg(PKT_TRANS_ID_UNDEF,
			 f0, f1, p->cfg->bw_div, p->cfg->bin_div,
			 gtk_spin_button_get_value_as_int(p->cfg->sb_avg),
			 0);
}



/**
 * @brief create spectrum acquisition control "GET"
 */

GtkWidget *radio_spec_cfg_ctrl_get_new(Radio *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Retrieve Spectrometer Configuration",
				 "Fetch the current remote configuration from "
				 "the server");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);


	w = gtk_button_new_with_label("Get Configuration");
	gtk_widget_set_tooltip_text(w, "Fetch spectrometer configuration\n"
				       "from server");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(radio_spec_cfg_get_cb), p);

	return GTK_WIDGET(grid);
}


/**
 * @brief create spectrum acquisition control "SET"
 */

GtkWidget *radio_spec_cfg_ctrl_set_new(Radio *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Program Spectrometer Configuration",
				 "Send the current local configuration to the "
				 "server.");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Set Configuration");
	gtk_widget_set_tooltip_text(w, "Send spectrometer configuration\n"
				       "to server");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(radio_spec_cfg_set_cb), p);


	return GTK_WIDGET(grid);
}

