/**
 * @file    widgets/radio/radio_hot_load_ctrl.c
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

static gboolean radio_hot_load_toggle_cb(GtkWidget *w,
					 gboolean state, gpointer data)
{
	if (gtk_switch_get_active(GTK_SWITCH(w)))
		cmd_hot_load_enable(PKT_TRANS_ID_UNDEF);
	else
		cmd_hot_load_disable(PKT_TRANS_ID_UNDEF);

	return TRUE;
}



/**
 * @brief helper function to change to state of our hot load toggle button
 *	  without it emitting the "toggle" signal
 */

static void radio_hot_load_toggle_button(GtkSwitch *s, gboolean state)
{
	const GCallback cb = G_CALLBACK(radio_hot_load_toggle_cb);


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
 * @brief signal handler for hot load button "on" status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

gboolean radio_hot_load_cmd_hot_load_enable(gpointer instance, gpointer data)
{
	Radio *p;


	p = RADIO(data);

	radio_hot_load_toggle_button(p->cfg->sw_hot, TRUE);

	return FALSE;
}


/**
 * @brief signal handler for hot load button "off" status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

gboolean radio_hot_load_cmd_hot_load_disable(gpointer instance, gpointer data)
{
	Radio *p;


	p = RADIO(data);

	radio_hot_load_toggle_button(p->cfg->sw_hot, FALSE);

	return FALSE;
}


/**
 * @brief create hot load controls
 */

GtkWidget *radio_hot_load_ctrl_new(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *tmp;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Hot Load",
				 "Enable or disable a hot load on the "
				 "telescope.");

	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(grid, w, 0, 0, 1, 4);


	tmp = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	p->cfg->hot_cfg = GTK_LABEL(tmp);

	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable hot load\n");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);

	gtk_grid_attach(grid, w, 2, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(radio_hot_load_toggle_cb), p);

	p->cfg->sw_hot = GTK_SWITCH(w);


	return GTK_WIDGET(grid);
}

