/**
 * @file    widgets/radio/radio_spec_acq_ctrl.c
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
 * @brief signal handler for toggle switch
 */

static gboolean radio_spec_acq_toggle_cb(GtkWidget *w,
					 gboolean state, gpointer data)
{
	if (gtk_switch_get_active(GTK_SWITCH(w)))
		cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
	else
		cmd_spec_acq_disable(PKT_TRANS_ID_UNDEF);

	return TRUE;
}



/**
 * @brief helper function to change to state of our acquistion toggle button
 *	  without it emitting the "toggle" signal
 */

static void radio_spec_acq_toggle_button(GtkSwitch *s, gboolean state)
{
	const GCallback cb = G_CALLBACK(radio_spec_acq_toggle_cb);


	/* block/unblock the state-set handler of the switch for all copies of
	 * the widget, so we can change the state without emitting a signal
	 */

	g_signal_handlers_block_matched(s, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	gtk_switch_set_state(s, state);

	g_signal_handlers_unblock_matched(s, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
}


/**
 * @brief signal handler for acquisition button "on" status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

gboolean radio_spec_acq_cmd_spec_acq_enable(gpointer instance, gpointer data)
{
	Radio *p;


	p = RADIO(data);

	radio_spec_acq_toggle_button(p->cfg->sw_acq, TRUE);

	return FALSE;
}


/**
 * @brief signal handler for acquisition button "off" status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

gboolean radio_spec_acq_cmd_spec_acq_disable(gpointer instance, gpointer data)
{
	Radio *p;


	p = RADIO(data);

	radio_spec_acq_toggle_button(p->cfg->sw_acq, FALSE);

	return FALSE;
}

#if 0

/**
 * @brief signal handler for single shot button press event
 */

static void radio_spec_acq_single_shot_cb(GtkWidget *w, Radio *p)
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
			 gtk_spin_button_get_value_as_int(p->cfg->sb_avg), 1);

	cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
}

#endif



/**
 * @brief create spectrum acquisition controls
 */

GtkWidget *radio_spec_acq_ctrl_new(Radio *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Spectral Acquisition",
				 "Enable or disable acquisition of spectral "
				 "data by the server.");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 2);

#if 0
	w = gtk_button_new_with_label("Single");
	gtk_widget_set_tooltip_text(w, "Acquire a single spectrum\n"
				       "given the current settings");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(radio_spec_acq_single_shot_cb), p);
#endif


	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable acquisition\n");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);

	gtk_grid_attach(grid, w, 2, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(radio_spec_acq_toggle_cb), p);

	p->cfg->sw_acq = GTK_SWITCH(w);


	return GTK_WIDGET(grid);
}

