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
#include <cmd.h>
#include <math.h>


struct _RadioConfig {
	gdouble prec;
	struct capabilities c;

	GtkSpinButton *sb_frq_lo;
	GtkSpinButton *sb_frq_hi;

	GtkSpinButton *sb_bw_div;
	GtkSpinButton *sb_bin_div;

	GtkSpinButton *sb_avg;

	GtkLabel *freq_cfg;
	GtkLabel *bw_cfg;
	GtkLabel *avg_cfg;


	int bw_div;		/* bandwidth divider */
	int bin_div;		/* per-bandwidth bin divider */
};

G_DEFINE_TYPE_WITH_PRIVATE(Radio, radio, GTK_TYPE_BOX)


/**
 * @brief display the currently configured frequency range on the remote
 *
 * XXX need actual config, use local config as standin for now
 */

static void radio_update_conf_freq_lbl(Radio *p)
{
	gchar *lbl;

	gdouble f0, f1; /** XXX **/

	f0 = gtk_spin_button_get_value(p->cfg->sb_frq_lo);
	f1 = gtk_spin_button_get_value(p->cfg->sb_frq_hi);

	lbl = g_strdup_printf(
		 "<span foreground='#7AAA7E'"
		 "	size = 'small'>"
		 "Currently configured: %.4f - %.4f MHz"
		 "</span>",
		 f0, f1);

	gtk_label_set_markup(p->cfg->freq_cfg, lbl);

	g_free(lbl);
}


/**
 * @brief display the currently configured bandwidth setting on the remote
 *
 * XXX need actual config, use local config as standin for now
 */

static void radio_update_conf_bw_lbl(Radio *p)
{
	gchar *lbl;

	gdouble bw, bd; /** XXX **/


	/** begin dummy **/
	if (p->cfg->c.bw_max_div_lin)
		bw = p->cfg->c.bw_max_hz / (gdouble) (p->cfg->bw_div + 1);
	else
		bw = p->cfg->c.bw_max_hz / (gdouble) (1 << p->cfg->bw_div);

	if (p->cfg->c.bw_max_bin_div_lin)
		bd = p->cfg->c.bw_max_bins / (gdouble) (p->cfg->bin_div + 1);
	else
		bd = p->cfg->c.bw_max_bins / (gdouble) (1 << p->cfg->bin_div);

	/** end dummy **/


	lbl = g_strdup_printf(
		 "<span foreground='#7AAA7E'"
		 "	size = 'small'>"
		 "Currently configured: Spectral Bins: %g Bandwidth: %g kHz "
		 "(%g kHz per Bin)"
		 "</span>",
		 bd, bw * 1e-3, bw * 1e-3 / bd); /* to kHz */

	gtk_label_set_markup(p->cfg->bw_cfg, lbl);

	g_free(lbl);
}


/**
 * @brief display the currently configured averaging on the remote
 *
 * XXX need actual config, use local config as standin for now
 */

static void radio_update_avg_lbl(Radio *p)
{
	gchar *lbl;

	int avg;


	avg = (int) gtk_spin_button_get_value(p->cfg->sb_avg);

	lbl = g_strdup_printf(
		 "<span foreground='#7AAA7E'"
		 "	size = 'small'>"
		 "Currently configured averages: %dx"
		 "</span>",
		 avg);

	gtk_label_set_markup(p->cfg->avg_cfg, lbl);

	g_free(lbl);
}



static void radio_update_freq_range_spin_button(Radio *p, GtkSpinButton *b)
{
	gdouble fmin;
	gdouble fmax;
	gdouble finc;

	gdouble val;


	/* return as long as freq_max is not set */
	if (!p->cfg->c.freq_max_hz)
		return;

	fmin = (gdouble) p->cfg->c.freq_min_hz * 1e-6; /* to MHz */
	fmax = (gdouble) p->cfg->c.freq_max_hz * 1e-6; /* to MHz */

	finc = (gdouble) p->cfg->c.bw_max_hz / (gdouble) p->cfg->c.bw_max_bins;

	/* we use linear bandwidth dividers if available */
	if (p->cfg->c.bw_max_div_lin)
	       	finc = finc / (gdouble) (p->cfg->bw_div + 1);
	else
	       	finc = finc / (gdouble) (1 << p->cfg->bw_div);


	/* we use linear per-bandwidth bin dividers if available */
	if (p->cfg->c.bw_max_bin_div_lin)
		finc = finc / (gdouble) (p->cfg->bin_div + 1);
	else
		finc = finc / (gdouble) (1 << p->cfg->bin_div);

	finc *= 1e-6;	/* to Mhz */


	val = gtk_spin_button_get_value(b);

	gtk_spin_button_set_range(b, fmin, fmax);
	gtk_spin_button_set_increments(b, finc, finc * 10.0);

	/* force update, snaps value to ticks if finc changed */
	gtk_spin_button_update(b);

	/* properly clamp value to range, Gtk just sets this to range_min */
	if (val < fmin)
		val = fmin;
	else if (val > fmax)
		val = fmax;
	else
		return;

	gtk_spin_button_set_value(b, val);
}


static void radio_update_freq_range(Radio *p)
{
	radio_update_freq_range_spin_button(p, p->cfg->sb_frq_lo);
	radio_update_freq_range_spin_button(p, p->cfg->sb_frq_hi);
}


static void radio_update_bw_divider(Radio *p)
{
	gdouble dmax;

	GtkSpinButton *b;


	b = p->cfg->sb_bw_div;

	/* we prefer linear dividers if available */
	if (p->cfg->c.bw_max_div_lin) {
		dmax = (gdouble) p->cfg->c.bw_max_div_lin;
		p->cfg->bw_div = 1;
	} else {
		dmax = (gdouble) p->cfg->c.bw_max_div_rad2;
		p->cfg->bw_div = 0;
	}

	gtk_spin_button_set_range(b, (gdouble) p->cfg->bw_div, dmax);
	gtk_spin_button_set_value(b, dmax);
}


static void radio_update_bin_divider(Radio *p)
{
	gdouble dmax;

	GtkSpinButton *b;


	b = p->cfg->sb_bin_div;

	/* we prefer linear dividers if available */
	if (p->cfg->c.bw_max_bin_div_lin) {
		dmax = (gdouble) p->cfg->c.bw_max_bin_div_lin;
		p->cfg->bin_div = 1;
	} else {
		dmax = (gdouble) p->cfg->c.bw_max_bin_div_rad2;
		p->cfg->bin_div = 0;
	}

	gtk_spin_button_set_range(b, (gdouble) p->cfg->bin_div, dmax);
	gtk_spin_button_set_value(b, dmax);
}


static void radio_handle_cmd_capabilities(gpointer instance,
					  const struct capabilities *c,
					  gpointer data)
{
	Radio *p;

	p = RADIO(data);

	p->cfg->c = (*c);

	g_message("Event \"cmd-capabilities\" signalled:");

	g_message("\tfreq_min_hz %lu",		p->cfg->c.freq_min_hz);
	g_message("\tfreq_max_hz %lu",		p->cfg->c.freq_max_hz);
	g_message("\tfreq_inc_hz %d",		p->cfg->c.freq_inc_hz);
	g_message("\tbw_max_hz %d",		p->cfg->c.bw_max_hz);
	g_message("\tbw_max_div_lin %d",	p->cfg->c.bw_max_div_lin);
	g_message("\tbw_max_div_rad2 %d",	p->cfg->c.bw_max_div_rad2);
	g_message("\tbw_max_bins %d",		p->cfg->c.bw_max_bins);
	g_message("\tbw_max_bin_div_lin %d",	p->cfg->c.bw_max_bin_div_lin);
	g_message("\tbw_max_bin_div_rad2 %d",	p->cfg->c.bw_max_bin_div_rad2);


	radio_update_bw_divider(p);
	radio_update_bin_divider(p);
	radio_update_freq_range(p);
}


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
 * @see if one of the frequency range spin buttons is above/below the other
 *      and spin up/down a single increment (we always want at least one
 *      frequency bin, duh!)
 *
 * @note interestingly, an increment paramter of 1 to gtk_spin_button_spin()
 *	 forces a full-integer digit forward/backward, while 0 does an actual
 *	 fine-grain decimal place incerement as if you had clicked the button.
 *	 Bug or lack of documentation?
 */

static gint radio_freq_value_changed(GtkSpinButton *sb, Radio *p)
{
	gdouble f0, f1;


	f0 = gtk_spin_button_get_value(p->cfg->sb_frq_lo);
	f1 = gtk_spin_button_get_value(p->cfg->sb_frq_hi);


	if (f0 < f1)
		return TRUE;


	if (sb == p->cfg->sb_frq_lo) {
		gtk_spin_button_set_value(p->cfg->sb_frq_hi, f0);
		gtk_spin_button_spin(p->cfg->sb_frq_hi,
				     GTK_SPIN_STEP_FORWARD, 0);
	}

	if (sb == p->cfg->sb_frq_hi) {
		gtk_spin_button_set_value(p->cfg->sb_frq_lo, f1);
		gtk_spin_button_spin(p->cfg->sb_frq_lo,
				     GTK_SPIN_STEP_BACKWARD, 0);
	}

	return TRUE;
}



/**
 * @brief signal handler for single shot button press event
 * TODO: stacking
 */

static void radio_spec_acq_single_shot_cb(GtkWidget *w, Radio *p)
{
	uint64_t f0;
	uint64_t f1;


	f0 = (uint64_t) (gtk_spin_button_get_value(p->cfg->sb_frq_lo) * 1e6);
	f1 = (uint64_t) (gtk_spin_button_get_value(p->cfg->sb_frq_hi) * 1e6);

	g_message("F0: %.10f F1 %.10f f0: %ld f1: %ld div: %d",
		  gtk_spin_button_get_value(p->cfg->sb_frq_lo),
		  gtk_spin_button_get_value(p->cfg->sb_frq_hi),
		  f0, f1, p->cfg->bw_div);

	radio_update_avg_lbl(p);
	radio_update_conf_freq_lbl(p);
	radio_update_conf_bw_lbl(p);

	cmd_spec_acq_start(f0, f1, p->cfg->bw_div, p->cfg->bin_div,
			   gtk_spin_button_get_value(p->cfg->sb_avg), 1);
}


/**
 * @brief this handles the input signal to the bandwidth spin button
 *
 * @note every time the the contents of the spin button entry is modified,
 *	 (when the output signal is connected to a callback)
 *	 gtk wants us to update the actual value of the adjustment.
 *	 Since internally we have to track the bandwidth divider value, we
 *	 update the GtkAdjustment to hold the current (inverse) divider
 *	 (updated by radio_sb_bw_div_output_cb).
 */

static gint radio_sb_bw_div_input_cb(GtkSpinButton *sb, gdouble *new_val,
				     Radio *p)
{
	int div;
	gdouble disp;


	if (p->cfg->c.bw_max_div_lin)
		(*new_val) = p->cfg->c.bw_max_div_lin - p->cfg->bw_div;
	else
		(*new_val) = p->cfg->c.bw_max_div_rad2 - p->cfg->bw_div;


	return TRUE;
}

/**
 * @brief bandwidth spin button output formatter
 *
 * @note requires the input handler above
 *	 Since GtkSpinButtons have no "inverse" option, we do it ourselves
 *	 so that plus/minus actually increments/decrements the bandwidth
 *	 instead of the reverse
 */

static gboolean radio_sb_bw_div_output_cb(GtkSpinButton *sb, Radio *p)
{
	gdouble bw;
	unsigned int val;

	gchar *lbl;

	GtkAdjustment *adj;


	adj = gtk_spin_button_get_adjustment(sb);
	val = (unsigned int) gtk_adjustment_get_value(adj);

	/* prefer linear divider if available */
	if (p->cfg->c.bw_max_div_lin) {
		p->cfg->bw_div = p->cfg->c.bw_max_div_lin - val;
		bw = p->cfg->c.bw_max_hz / (gdouble) (p->cfg->bw_div + 1);
	} else {
		p->cfg->bw_div = p->cfg->c.bw_max_div_rad2 - val;
		bw = p->cfg->c.bw_max_hz / (gdouble) (1 << p->cfg->bw_div);
	}

	lbl = g_strdup_printf("%g", bw);

	gtk_entry_set_text(GTK_ENTRY(sb), lbl);

	g_free(lbl);


	/* inform range selector about new divider */
	radio_update_freq_range(p);


	return TRUE;
}


/**
 * @brief this handles the input signal to the bin size spin button
 *
 * @note every time the the contents of the spin button entry is modified,
 *	 (when the output signal is connected to a callback)
 *	 gtk wants us to update the actual value of the adjustment.
 *	 Since internally we have to track the bin divider value, we update
 *	 the GtkAdjustment to hold the current (inverse) divider (updated by
 *	 radio_sb_bin_div_output_cb).
 */

static gint radio_sb_bin_div_input_cb(GtkSpinButton *sb, gdouble *new_val,
					 Radio *p)
{
	int div;
	gdouble disp;


	if (p->cfg->c.bw_max_bin_div_lin)
		(*new_val) = p->cfg->c.bw_max_bin_div_lin - p->cfg->bin_div;
	else
		(*new_val) = p->cfg->c.bw_max_div_rad2 - p->cfg->bin_div;


	return TRUE;
}


/**
 * @brief bandwidth spin button output formatter
 *
 * @note requires the input handler above
 *	 Since GtkSpinButtons have no "inverse" option, we do it ourselves
 *	 so that plus/minus actually increments/decrements the bandwidth
 *	 instead of the reverse
 */

static gboolean radio_sb_bin_div_output_cb(GtkSpinButton *sb, Radio *p)
{
	gdouble bd;
	unsigned int val;

	gchar *lbl;

	GtkAdjustment *adj;


	adj = gtk_spin_button_get_adjustment(sb);
	val = (unsigned int) gtk_adjustment_get_value(adj);

	/* prefer linear divider if available */
	if (p->cfg->c.bw_max_bin_div_lin) {
		p->cfg->bin_div = p->cfg->c.bw_max_bin_div_lin - val;
		bd = p->cfg->c.bw_max_bins / (gdouble) (p->cfg->bin_div + 1);
	} else {
		p->cfg->bin_div = p->cfg->c.bw_max_bin_div_rad2 - val;
		bd = p->cfg->c.bw_max_bins / (gdouble) (1 << p->cfg->bin_div);
	}

	lbl = g_strdup_printf("%g", bd);

	gtk_entry_set_text(GTK_ENTRY(sb), lbl);

	g_free(lbl);


	/* inform range selector about new divider */
	radio_update_freq_range(p);


	return TRUE;
}




/**
 * @brief create spectrum acquisition controls
 */

static GtkWidget *gui_create_spec_acq_ctrl(Radio *p)
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
			 G_CALLBACK(radio_spec_acq_single_shot_cb), p);

	return grid;
}


/**
 * @brief create spectral frequency range controls
 */

static GtkWidget *gui_create_acq_freq_ctrl(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *tmp;


	grid = GTK_GRID(new_default_grid());


	w = gui_create_desclabel("Acquisition Frequency Range",
				 "Configure the upper and lower frequency limits of the receiver\n"
				 "Note that the lower/upper frequency resolution depends on the receiver bandwidth settings");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);

	tmp = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	p->cfg->freq_cfg = GTK_LABEL(tmp);
	/* TODO: add callback to label for remote freq range updates */



	/* XXX: add AUTOSET button to center freq, on/off switch for center or
	 * lo/hi freq*/
	/* NOTE: we set the high value to MAX_DOUBLE and the initial low value
	 * to MIN_VALUE. Every time we get an update of the capabilities, we
	 * shrink the initial value if the current value does not fit,
	 * otherwise we leave it be
	 */

	w = gtk_label_new("Low");
	gtk_grid_attach(grid, w, 1, 1, 1, 1);

	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 2, 1, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(radio_freq_value_changed), p);

	p->cfg->sb_frq_lo = GTK_SPIN_BUTTON(w);

	/* TODO connect signals */


	w = gtk_label_new("High");
	gtk_grid_attach(grid, w, 1, 2, 1, 6);

	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 2, 2, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(radio_freq_value_changed), p);

	p->cfg->sb_frq_hi = GTK_SPIN_BUTTON(w);

	/* TODO connect signals */

	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	/* TODO connect signals */


	return GTK_WIDGET(grid);
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

static GtkWidget *gui_create_acq_res_ctrl(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *tmp;


	grid = GTK_GRID(new_default_grid());



	w = gui_create_desclabel("Bandwith Resolution",
				 "Configure the receiver's acquisition mode\n"
				 "Note that this configures the acquisition "
				 "size from which spectrae are assembled");

	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(grid, w, 0, 0, 1, 3);

	tmp = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	p->cfg->bw_cfg = GTK_LABEL(tmp);
	/* TODO: add callback to label for remote range updates */


	w = gtk_label_new("Bandwith");
	gtk_grid_attach(grid, w, 1, 1, 1, 1);

	w = gtk_spin_button_new(NULL, 1, 1);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 1, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w), 6);

	gtk_grid_attach(grid, w, 2, 1, 1, 1);
	g_signal_connect(G_OBJECT(w), "input",
			 G_CALLBACK(radio_sb_bw_div_input_cb), p);
	g_signal_connect(G_OBJECT(w), "output",
			 G_CALLBACK(radio_sb_bw_div_output_cb), p);

	p->cfg->sb_bw_div = GTK_SPIN_BUTTON(w);




	w = gtk_label_new("Spectral Bins");
	gtk_grid_attach(grid, w, 1, 2, 1, 1);

	w = gtk_spin_button_new(NULL, 1, 1);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 1, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w), 3);

	gtk_grid_attach(grid, w, 2, 2, 1, 1);
	g_signal_connect(G_OBJECT(w), "input",
			 G_CALLBACK(radio_sb_bin_div_input_cb), p);
	g_signal_connect(G_OBJECT(w), "output",
			 G_CALLBACK(radio_sb_bin_div_output_cb), p);

	p->cfg->sb_bin_div = GTK_SPIN_BUTTON(w);






	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	/* TODO connect signals */


	return GTK_WIDGET(grid);
}


/**
 * @brief create spectral averaging controls
 *
 * @note we limit the number of remote averages to 32 to force a sane range
 *       on the user
 */

static GtkWidget *gui_create_spec_avg_ctrl(Radio *p)
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
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 1, 10);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), 1, 32);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(w), 0);
	gtk_entry_set_width_chars(GTK_ENTRY(w), 3);

	gtk_grid_attach(grid, w, 2, 1, 1, 1);

	p->cfg->sb_avg = GTK_SPIN_BUTTON(w);

	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(grid, w, 3, 1, 1, 1);


	return GTK_WIDGET(grid);
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

	cb = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1420.406", "Hydrogen (HI) J=1/2 F=1-0");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1612.231", "Hydroxyl Radical (OH) J=3/2 F=1-2");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1665.402", "Hydroxyl Radical (OH) J=3/2 F=1-1");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1667.359", "Hydroxyl Radical (OH) J=3/2 F=2-2");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(cb), "1720.530", "Hydroxyl Radical (OH) J=3/2 F=2-1");


	w = gtk_entry_new();
	g_object_bind_property(cb, "active-id", w, "text",
			       G_BINDING_BIDIRECTIONAL);


	gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Presets"), 1, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), cb, 2, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 1, 1, 1);


	w = gtk_button_new_with_label("Set");
	gtk_grid_attach(GTK_GRID(grid), w, 4, 1, 1, 1);

	gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);

	return grid;
}


static void gui_create_radio_controls(Radio *p)
{
	GtkWidget *w;


	w = gui_create_spec_acq_ctrl(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);


	w = gui_create_acq_freq_ctrl(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = gui_create_acq_res_ctrl(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = gui_create_spec_avg_ctrl(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w =gui_create_ref_vrest_ctrl();
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

}



/**
 * @brief initialise the Radio class
 */

static void radio_class_init(RadioClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


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

	p->cfg = radio_get_instance_private(p);


	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_radio_controls(p);

	g_signal_connect(sig_get_instance(), "cmd-capabilities",
			 G_CALLBACK(radio_handle_cmd_capabilities),
			 (void *) p);
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
