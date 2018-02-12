/**
 * @file    widgets/radio/radio.c
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
 * @brief a widget to control the settings of the radio spectrometer
 *
 */


#include <radio.h>
#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>


G_DEFINE_TYPE(Radio, radio, GTK_TYPE_BOX)


struct _RadioConfig {
	gdouble prec;
};




/**
 * @brief signal handler for toggle switch
 */

static gboolean radio_spec_acq_toggle_cb(GtkWidget *w,
					 gboolean state, gpointer data)
{
	g_message("Set acquisition to %s in %s",
		  gtk_switch_get_active(GTK_SWITCH(w)) ? "ON" : "OFF",
		  __func__);

	return FALSE;
}


/**
 * @brief signal handler for acqisition on/off button status
 *
 * @note when using the internal signal server, widget pointers must be
 *	 transported via the userdata argument
 */

static gboolean radio_spec_acq_cmd_success_cb(GObject *o, gpointer data)
{
	GtkSwitch *s;


	s = GTK_SWITCH(data);


	/* block/unblock the state-set handler of the switch,
	 * so we can change the state without emitting a signal
	 */

	g_signal_handlers_block_by_func(s,
					G_CALLBACK(radio_spec_acq_toggle_cb),
					NULL);

	/* TODO: emit command, just toggle for now */
	gtk_switch_set_state(s, !gtk_switch_get_state(s));


	g_signal_handlers_unblock_by_func(s,
					  G_CALLBACK(radio_spec_acq_toggle_cb),
					  NULL);

	return FALSE;
}


/**
 * @brief signal handler for single shot button press event
 */

static void radio_spec_acq_single_shot_cb(GtkWidget *w, gpointer data)
{
	/* TODO: emit proper command */
	sig_cmd_success();
}



/**
 * @brief create spectrum acquisition controls
 */

static GtkWidget *gui_create_spec_acq_ctrl(void)
{
	GtkWidget *grid;
	GtkWidget *w;


	grid = new_default_grid();

	w = gui_create_desclabel("Spectral Acquisition",
				 "Enable persistent acquisition of spectral data by the server");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);


	w = gtk_switch_new();
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);
	g_signal_connect(sig_get_instance(), "cmd-success",
			 G_CALLBACK(radio_spec_acq_cmd_success_cb),
			 (gpointer) w);
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(radio_spec_acq_toggle_cb), NULL);


	w = gtk_button_new_with_label("Single Shot");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(radio_spec_acq_single_shot_cb), NULL);

	return grid;
}


/**
 * @brief create spectral frequency range controls
 */

static GtkWidget *gui_create_acq_freq_ctrl(void)
{
	GtkWidget *grid;
	GtkWidget *w;


	grid = new_default_grid();


	w = gui_create_desclabel("Acquisition Frequency Range",
				 "Configure the upper and lower frequency limits of the receiver\n"
				 "Note that the lower/upper frequency resolution depends on the receiver bandwidth settings");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);

	{
	gchar buf[256];
	GtkWidget *tmp = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	snprintf(buf, 256,
		 "<span foreground='#7AAA7E'"
		 "	size = 'small'>"
		 "Currently configured: %g - %g MHz"
		 "</span>",
		 1420.7, 1520.8);

	gtk_label_set_markup(GTK_LABEL(tmp), buf);

	/* TODO: add callback to label for remote freq range updates */
	}


	w = gtk_label_new("Low");
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);

	w = gtk_spin_button_new_with_range(1420.0, 1430.0, 0.1);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 1, 1, 1);
	/* TODO connect signals */


	w = gtk_label_new("High");
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);

	w = gtk_spin_button_new_with_range(1430.0, 1440.0, 0.1);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 2, 1, 1);
	/* TODO connect signals */

	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(GTK_GRID(grid), w, 3, 1, 1, 1);
	/* TODO connect signals */


	return grid;
}


/**
 * @brief create spectral resolution controls
 *
 * @note here we configure the spectral resolution. The remote device
 *	 would typically support either 2^n (SRT: n_max=2) or linear dividers
 *	 based on a given acquisition bandwidth (500 kHz for the SRT) and a
 *	 range of bins (SRT: 64) with equally either 2^n or linear
 *	 dividiers (SRT: none) for which we must generate the proper selections
 *	 below
 */

static GtkWidget *gui_create_acq_res_ctrl(void)
{
	GtkWidget *grid;
	GtkWidget *w;


	grid = new_default_grid();



	w = gui_create_desclabel("Bandwith Resolution",
				 "Configure the revceiver's acquisition mode\n"
				 "Note that this configures the acquisition size from which spectrae are are assembled");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);

	{
	gchar buf[256];
	GtkWidget *tmp = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	snprintf(buf, 256,
		 "<span foreground='#7AAA7E'"
		 "	size = 'small'>"
		 "Currently configured: bins: %d res %d khz"
		 "</span>",
		 64, 500);

	gtk_label_set_markup(GTK_LABEL(tmp), buf);

	/* TODO: add callback to label for remote range updates */
	}


	w = gtk_label_new("Bandwith");
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);

	w = gtk_spin_button_new_with_range(250., 500., 125.);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 1, 1, 1);
	/* TODO connect signals */


	w = gtk_label_new("Spectral Bins");
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);

	w = gtk_spin_button_new_with_range(32, 64, 16);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 2, 1, 1);
	/* TODO connect signals */

	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(GTK_GRID(grid), w, 3, 1, 1, 1);
	/* TODO connect signals */


	return grid;
}


/**
 * @brief create reference rest frequency controls 
 *
 * @note here we configure the spectral resolution. The remote device
 *	 would typically support either 2^n (SRT: n_max=2) or linear dividers
 *	 based on a given acquisition bandwidth (500 kHz for the SRT) and a
 *	 range of bins (SRT: 64) with equally either 2^n or linear
 *	 dividiers (SRT: none) for which we must generate the proper selections
 *	 below
 */

static GtkWidget *gui_create_ref_vrest_ctrl(void)
{
	GtkWidget *grid;
	GtkWidget *w;
	GtkWidget *cb;


	grid = new_default_grid();



	w = gui_create_desclabel("Reference Rest Frequency",
				 "Used to calculate the radial (Doppler) velocity to the Local Standard of Rest");

	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);



	/* note: for easier selection, always give J (total electronic angular
	 * momentum quantum number) and F (transitions between hyperfine
	 * levels)
	 *
	 * note on OH: the ground rotational state splits into a lambda-doublet
	 * sub-levels due to the interaction between the rotational and
	 * electronic angular momenta of the molecule. The sub-levels further
	 * split into two hyperfine levels as a result of the interaction
	 * between the electron and nuclear spins of the hydrogen atom.
	 * The transitions that connect sub-levels with the same F-values are
	 * called the main lines, whereas the transitions between sub-levels of
	 * different F-values are called the satellite lines.
	 * (See DICKE'S SUPERRADIANCE IN ASTROPHYSICS. II. THE OH 1612 MHz LINE,
	 * F. Rajabi and M. Houde,The Astrophysical Journal, Volume 828,
	 * Number 1.)
	 * The main lines are stronger than the satellite lines. In star
	 * forming regions, the 1665 MHz line exceeds the 1667 MHz line in
	 * intensity, while in equilibirium conditions, it is generally weaker.
	 * In late-type starts, the 1612 MHz line may sometimes be equal or
	 * even exceed the intensity of the main lines.
	 */

	cb = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1420.406", "Hydrogen (HI) J=1/2 F=1-0");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1612.231", "Hydroxyl Radical (OH) J=3/2 F=1-2");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1665.402", "Hydroxyl Radical (OH) J=3/2 F=1-1");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1667.359", "Hydroxyl Radical (OH) J=3/2 F=2-2");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1720.530", "Hydroxyl Radical (OH) J=3/2 F=2-1");

	gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);

	w = gtk_entry_new();
	g_object_bind_property(cb, "active-id", w, "text",
			       G_BINDING_BIDIRECTIONAL);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Presets"), 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), cb, 2, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 1, 1, 1);


	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(GTK_GRID(grid), w, 4, 1, 1, 1);


	return grid;
}


static void gui_create_radio_controls(GtkBox *box)
{
	GtkWidget *w;


	w = gui_create_spec_acq_ctrl();
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);


	w = gui_create_acq_freq_ctrl();
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);

	w = gui_create_acq_res_ctrl();
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);

	w =gui_create_ref_vrest_ctrl();
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);

}



/**
 * @brief initialise the Radio class
 */

static void radio_class_init(RadioClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;

	g_type_class_add_private(klass, sizeof(RadioConfig));

	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the Radio widget
 */

static void radio_init(Radio *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_RADIO(p));

	p->cfg = G_TYPE_INSTANCE_GET_PRIVATE(p, TYPE_RADIO, RadioConfig);

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_radio_controls(GTK_BOX(p));

#if 0
	/* connect the relevant signals of the DrawingArea */
	g_signal_connect(G_OBJECT(&p->parent), "draw",
			  G_CALLBACK(xyplot_draw_cb), NULL);
#endif
}


/**
 * @brief create a new Radio widget
 */

GtkWidget *radio_new(void)
{
	Radio *radio;


	radio = g_object_new(TYPE_RADIO, NULL);

	return GTK_WIDGET(radio);
}
