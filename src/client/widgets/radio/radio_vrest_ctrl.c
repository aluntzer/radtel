/**
 * @file    widgets/radio/radio_vrest_ctrl.c
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

#include <coordinates.h>
#include <signals.h>




static void radio_vrest_entry_changed_cb(GtkEditable *ed, gpointer data)
{
	gdouble vrest;

	Radio *p;


	p = RADIO(data);


	if (!gtk_entry_get_text_length(GTK_ENTRY(ed)))
		return;

	vrest = g_strtod(gtk_entry_get_text(GTK_ENTRY(ed)), NULL);

	p->cfg->freq_ref_mhz = vrest;

	radio_input_freq_val_refresh(p);
}



static void radio_vrest_entry_insert_text_cb(GtkEditable *ed, gchar *new_text,
					  gint new_text_len, gpointer pos,
					  gpointer data)
{
	gint i;


	/* allow digits and decimal separators only */
	for (i = 0; i < new_text_len; i++) {
		if (!g_ascii_isdigit(new_text[i])
		    && new_text[i] != ','
		    && new_text[i] != '.') {
			g_signal_stop_emission_by_name(ed, "insert-text");
			break;
		}
	}

}



static void radio_vrest_sel_changed(GtkComboBox *cb, gpointer data)
{
	GtkListStore *ls;

	GtkTreeIter iter;

	Radio *p;

	gdouble vrest;


	p = RADIO(data);

	if (!p)
		return;

	if (!gtk_combo_box_get_active_iter(cb, &iter))
		return;

	ls = GTK_LIST_STORE(gtk_combo_box_get_model(cb));

	gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 2, &vrest, -1);

	p->cfg->freq_ref_mhz = vrest;


	radio_input_freq_val_refresh(p);
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

GtkWidget *radio_vrest_ctrl_new(Radio *p)
{
	GtkWidget *grid;
	GtkWidget *w;
	GtkWidget *cb;

	GtkListStore *ls;
	GtkCellRenderer *col;


	grid = new_default_grid();



	w = gui_create_desclabel("Reference Rest Frequency",
				 "Used to calculate the radial (Doppler) "
				 "velocity to the Local Standard of Rest");

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


	ls = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE);

	/* printf to get locale decimal style */
	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(HI) J=1/2 F=1-0",
					  1, g_strdup_printf("%7.3f", 1420.406),
					  2, 1420.406, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=1-2",
					  1, g_strdup_printf("%7.3f", 1612.231),
					  2, 1612.231, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=1-1",
					  1, g_strdup_printf("%7.3f", 1665.402),
					  2, 1665.402, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=2-2",
					  1, g_strdup_printf("%7.3f", 1667.359),
					  2, 1667.359, -1);

	gtk_list_store_insert_with_values(ls, NULL, -1,
					  0, "(OH) J=3/2 F=2-1",
					  1, g_strdup_printf("%7.3f", 1720.530),
					  2, 1720.530, -1);

	cb = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(ls));

	g_object_unref(ls);

	col = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cb), col, TRUE);

	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cb), col, "text", 0, NULL );
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(cb), 1);

	/* the entry is a child of the box */
	w = gtk_bin_get_child(GTK_BIN(cb));
	gtk_entry_set_width_chars(GTK_ENTRY(w), 8);

	g_signal_connect(w, "insert-text",
			 G_CALLBACK(radio_vrest_entry_insert_text_cb), p);
	g_signal_connect(w, "changed",
			 G_CALLBACK(radio_vrest_entry_changed_cb), p);
	gtk_entry_set_input_purpose(GTK_ENTRY(w), GTK_INPUT_PURPOSE_DIGITS);

	gtk_combo_box_set_id_column(GTK_COMBO_BOX(cb), 1);

	g_signal_connect(G_OBJECT(cb), "changed",
			 G_CALLBACK(radio_vrest_sel_changed), (gpointer) p);

	gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Reference [MHz]"),
					1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), cb, 2, 2, 1, 1);


	return grid;
}


/**
 * @brief signal handler (doppler tracker) for the timeout callback
 *
 */

static gboolean radio_spec_doppler_track_timeout_cb(gpointer data)
{
	uint64_t f0, f1;

	gdouble vcent, vspan;
	gdouble fc, bw2;

	gdouble step;

	static gdouble fclast;
	static int bw_div_last;
	static int bin_div_last;

	struct coord_horizontal hor;
	struct coord_equatorial	equ;

	Radio *p;



	p = (Radio *) data;


	if (!p->cfg->tracking) {
		fclast = 0.0;
		gtk_switch_set_state(GTK_SWITCH(p->cfg->sw_dpl), FALSE);
		return p->cfg->tracking;
	}

	hor.az = p->cfg->az;
	hor.el = p->cfg->el;
	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);

	vcent = gtk_spin_button_get_value(p->cfg->sb_vel_ce);
	vspan = gtk_spin_button_get_value(p->cfg->sb_vel_bw);

	vcent = vcent + vlsr(equ, 0.0);

	fc  = doppler_freq(vcent, p->cfg->freq_ref_mhz);
	bw2 = fabs(doppler_freq_relative(vspan, p->cfg->freq_ref_mhz) * 0.5);


	gtk_spin_button_get_increments(p->cfg->sb_frq_ce, &step, NULL);


	if ((fabs(fclast - fc) < step) &&
	     (bin_div_last == p->cfg->bin_div) &&
	     (bw_div_last == p->cfg->bw_div))
		return p->cfg->tracking;

	/* otherwise adjust */
	fclast = fc;
	bin_div_last = p->cfg->bin_div;
	bw_div_last =  p->cfg->bw_div;

	f0 = (uint64_t) ((fc - bw2) * 1e6);
	f1 = (uint64_t) ((fc + bw2) * 1e6);

	cmd_spec_acq_cfg(PKT_TRANS_ID_UNDEF,
			 f0, f1, p->cfg->bw_div, p->cfg->bin_div,
			 gtk_spin_button_get_value_as_int(p->cfg->sb_avg),
			 0);

	return p->cfg->tracking;
}

/**
 * @brief signal handler for toggle switch
 */

static gboolean radio_spec_doppler_track_toggle_cb(GtkWidget *w, gboolean state,
						   gpointer data)
{
	Radio *p;

	const GSourceFunc sf = radio_spec_doppler_track_timeout_cb;


	p = (Radio *) data;

	if (gtk_switch_get_active(GTK_SWITCH(w))) {

		if (p->cfg->tracking)	/* already at it */
			return TRUE;

		p->cfg->tracking = G_SOURCE_CONTINUE;
		p->cfg->id_to = g_timeout_add_seconds(1, sf, p);
	} else {
		p->cfg->tracking = G_SOURCE_REMOVE;
	}


	return FALSE;
}


/**
 * @brief create spectrum acquisition controls
 */

GtkWidget *radio_spec_doppler_ctrl_new(Radio *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Doppler Tracking",
				 "Auto-adjust frequency given the reference "
				 "frequency to compensate for radial "
				 "velocity\n"
				 "Note: the reference is the VLSR");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 4);

	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable doppler tracking\n");
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_vexpand(w, FALSE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);

	gtk_grid_attach(grid, w, 2, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(radio_spec_doppler_track_toggle_cb), p);

	p->cfg->sw_dpl = GTK_SWITCH(w);


	return GTK_WIDGET(grid);
}

