/**
 * @file    widgets/radio/radio_acq_freq_range_ctrl.c
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
 *
 * XXX this needs some refactoring
 */


#include <radio.h>
#include <radio_cfg.h>

#include <default_grid.h>
#include <desclabel.h>

#include <coordinates.h>


static gint radio_freq_value_changed(GtkSpinButton *sb, Radio *p);
static gint radio_center_freq_value_changed(GtkSpinButton *sb, Radio *p);
static gint radio_vel_value_changed(GtkSpinButton *sb, Radio *p);
static gint radio_center_vel_value_changed(GtkSpinButton *sb, Radio *p);


void radio_input_block_signals(Radio *p)
{
	g_signal_handler_block(p->cfg->sb_frq_lo, p->cfg->id_fl);
	g_signal_handler_block(p->cfg->sb_frq_hi, p->cfg->id_fh);
	g_signal_handler_block(p->cfg->sb_frq_ce, p->cfg->id_fc);
	g_signal_handler_block(p->cfg->sb_frq_bw, p->cfg->id_fs);

	g_signal_handler_block(p->cfg->sb_vel_lo, p->cfg->id_vl);
	g_signal_handler_block(p->cfg->sb_vel_hi, p->cfg->id_vh);
	g_signal_handler_block(p->cfg->sb_vel_ce, p->cfg->id_vc);
	g_signal_handler_block(p->cfg->sb_vel_bw, p->cfg->id_vs);
}



void radio_input_unblock_signals(Radio *p)
{
	g_signal_handler_unblock(p->cfg->sb_frq_lo, p->cfg->id_fl);
	g_signal_handler_unblock(p->cfg->sb_frq_hi, p->cfg->id_fh);
	g_signal_handler_unblock(p->cfg->sb_frq_ce, p->cfg->id_fc);
	g_signal_handler_unblock(p->cfg->sb_frq_bw, p->cfg->id_fs);

	g_signal_handler_unblock(p->cfg->sb_vel_lo, p->cfg->id_vl);
	g_signal_handler_unblock(p->cfg->sb_vel_hi, p->cfg->id_vh);
	g_signal_handler_unblock(p->cfg->sb_vel_ce, p->cfg->id_vc);
	g_signal_handler_unblock(p->cfg->sb_vel_bw, p->cfg->id_vs);
}


/**
 * @brief show input field configuration configuration
 */

static void radio_show_input_fields(Radio *p)
{
	gchar *lbl;

	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_lo));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_hi));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_ce));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_bw));

	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_vel_lo));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_vel_hi));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_vel_ce));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_vel_bw));


	if (p->cfg->mode_freq) {

		if (p->cfg->mode_lohi) {

			lbl = "Low [MHz]";
			gtk_label_set_text(p->cfg->sb_frq_lo_center_lbl, lbl);

			lbl = "High [MHz]";
			gtk_label_set_text(p->cfg->sb_frq_hi_bw_lbl, lbl);

			gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_lo));
			gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_hi));

			return;
		}

		lbl = "Center [MHz]";
		gtk_label_set_text(p->cfg->sb_frq_lo_center_lbl, lbl);

		lbl = "Span [MHz]";
		gtk_label_set_text(p->cfg->sb_frq_hi_bw_lbl, lbl);

		gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_ce));
		gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_bw));

		return;
	}

	if (p->cfg->mode_lohi) {

		lbl = "Low [km/s]";
		gtk_label_set_text(p->cfg->sb_frq_lo_center_lbl, lbl);

		lbl = "High [km/s]";
		gtk_label_set_text(p->cfg->sb_frq_hi_bw_lbl, lbl);

		gtk_widget_show(GTK_WIDGET(p->cfg->sb_vel_lo));
		gtk_widget_show(GTK_WIDGET(p->cfg->sb_vel_hi));

		return;

	}

	lbl = "Center [km/s]";
	gtk_label_set_text(p->cfg->sb_frq_lo_center_lbl, lbl);

	lbl = "Span [km/s]";
	gtk_label_set_text(p->cfg->sb_frq_hi_bw_lbl, lbl);

	gtk_widget_show(GTK_WIDGET(p->cfg->sb_vel_ce));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_vel_bw));
}


/**
 * @brief signal handler for the velocity input mode radio button
 */

static void radio_button_toggle(GtkWidget *button, Radio *p)
{
	if (gtk_toggle_button_get_active(p->cfg->rb_frq))
		p->cfg->mode_freq = TRUE;
	else if (gtk_toggle_button_get_active(p->cfg->rb_vel))
		p->cfg->mode_freq = FALSE;

	if (gtk_toggle_button_get_active(p->cfg->rb_lohi))
		p->cfg->mode_lohi = TRUE;
	else if (gtk_toggle_button_get_active(p->cfg->rb_cbw))
		p->cfg->mode_lohi = FALSE;


	radio_show_input_fields(p);
}



static gint radio_vel_value_changed(GtkSpinButton *sb, Radio *p)
{
	gdouble f0, f1, fcent, fspan;
	gdouble v0, v1, vcent, vspan;


	p->cfg->tracking = G_SOURCE_REMOVE;

	v0 = gtk_spin_button_get_value(p->cfg->sb_vel_lo);
	v1 = gtk_spin_button_get_value(p->cfg->sb_vel_hi);

	f0 = doppler_freq(v0, p->cfg->freq_ref_mhz);
	f1 = doppler_freq(v1, p->cfg->freq_ref_mhz);

	fcent = (f1 + f0) * 0.5;
	fspan = fabs(f1 - f0);

	vcent = (v1 + v0) * 0.5;
	vspan = fabs(v1 - v0);


	/* must block handler or we'll enter a update loop */
	radio_input_block_signals(p);


	/* always update hidden spin buttons */
	gtk_spin_button_set_value(p->cfg->sb_frq_ce, fcent);
	gtk_spin_button_set_value(p->cfg->sb_frq_bw, fspan);

	gtk_spin_button_set_value(p->cfg->sb_vel_ce, vcent);
	gtk_spin_button_set_value(p->cfg->sb_vel_bw, vspan);

	gtk_spin_button_set_value(p->cfg->sb_frq_lo, f0);
	gtk_spin_button_set_value(p->cfg->sb_frq_hi, f1);

	radio_input_unblock_signals(p);

	if (v0 < v1)
		return TRUE;


	/* underflow, push down */
	if (sb == p->cfg->sb_vel_lo) {
		gtk_spin_button_set_value(p->cfg->sb_vel_hi, v0);
		gtk_spin_button_spin(p->cfg->sb_vel_hi,
				     GTK_SPIN_STEP_FORWARD, 0);
	}

	if (sb == p->cfg->sb_vel_hi) {
		gtk_spin_button_set_value(p->cfg->sb_vel_lo, v1);
		gtk_spin_button_spin(p->cfg->sb_vel_lo,
				     GTK_SPIN_STEP_BACKWARD, 0);
	}

	return TRUE;

}

static gint radio_center_vel_value_changed(GtkSpinButton *sb, Radio *p)
{
	gdouble fc, bw2;

	gdouble fmin;
	gdouble fmax;

	gdouble v0, v1, vcent, vspan;


	fmin = (gdouble) p->cfg->c.freq_min_hz * 1e-6; /* to MHz */
	fmax = (gdouble) p->cfg->c.freq_max_hz * 1e-6; /* to MHz */


	vcent = gtk_spin_button_get_value(p->cfg->sb_vel_ce);
	vspan = gtk_spin_button_get_value(p->cfg->sb_vel_bw);

	v0 = vcent - vspan * 0.5;
	v1 = vcent + vspan * 0.5;

	fc  = doppler_freq(vcent, p->cfg->freq_ref_mhz);
	bw2 = doppler_freq_relative(vspan, p->cfg->freq_ref_mhz) * 0.5;

	if (fc - bw2 < fmin)
		bw2 = fc - fmin;
	if (fc + bw2 > fmax)
		bw2 = fmax - fc;

	vspan = doppler_vel_relative(bw2 * 2.0, p->cfg->freq_ref_mhz);

	radio_input_block_signals(p);

	gtk_spin_button_set_value(p->cfg->sb_frq_ce, fc);
	gtk_spin_button_set_value(p->cfg->sb_frq_bw, bw2 * 2.0);

	gtk_spin_button_set_value(p->cfg->sb_frq_lo, fc - bw2);
	gtk_spin_button_set_value(p->cfg->sb_frq_hi, fc + bw2);

	gtk_spin_button_set_value(p->cfg->sb_vel_lo, v0);
	gtk_spin_button_set_value(p->cfg->sb_vel_hi, v1);

	gtk_spin_button_set_value(p->cfg->sb_vel_bw, vspan);

	radio_input_unblock_signals(p);

	return TRUE;
}


/**
 * @brief see if one of the frequency range spin buttons is above/below the
 *	  other and spin up/down a single increment (we always want at least one
 *	  frequency bin, duh!)
 *
 * @note interestingly, an increment paramter of 1 to gtk_spin_button_spin()
 *	 forces a full-integer digit forward/backward, while 0 does an actual
 *	 fine-grain decimal place incerement as if you had clicked the button.
 *	 Bug or lack of documentation?
 */

static gint radio_freq_value_changed(GtkSpinButton *sb, Radio *p)
{
	gdouble f0, f1, fcent, fspan;
	gdouble v0, v1, vcent, vspan;


	p->cfg->tracking = G_SOURCE_REMOVE;

	f0 = gtk_spin_button_get_value(p->cfg->sb_frq_lo);
	f1 = gtk_spin_button_get_value(p->cfg->sb_frq_hi);

	v0 = doppler_vel(f0, p->cfg->freq_ref_mhz);
	v1 = doppler_vel(f1, p->cfg->freq_ref_mhz);

	fcent = (f1 + f0) * 0.5;
	fspan = fabs(f1 - f0);

	vcent = doppler_vel(fcent, p->cfg->freq_ref_mhz);
	vspan = doppler_vel_relative(fspan, p->cfg->freq_ref_mhz);

	/* must block handler or we'll enter a update loop */
	radio_input_block_signals(p);

	/* always update hidden spin buttons */
	gtk_spin_button_set_value(p->cfg->sb_frq_ce, fcent);
	gtk_spin_button_set_value(p->cfg->sb_frq_bw, fspan);

	gtk_spin_button_set_value(p->cfg->sb_vel_ce, vcent);
	gtk_spin_button_set_value(p->cfg->sb_vel_bw, vspan);

	gtk_spin_button_set_value(p->cfg->sb_vel_lo, v0);
	gtk_spin_button_set_value(p->cfg->sb_vel_hi, v1);


	radio_input_unblock_signals(p);


	if (f0 < f1)
		return TRUE;


	/* underflow, push down */

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
 * @brief see if one of the center frequency spin buttons are within legal
 *	 limits and clamp bandwidth if needed
 */

static gint radio_center_freq_value_changed(GtkSpinButton *sb, Radio *p)
{
	gdouble fc, bw2;

	gdouble fmin;
	gdouble fmax;

	gdouble v0, v1, vcent, vspan;


	fmin = (gdouble) p->cfg->c.freq_min_hz * 1e-6; /* to MHz */
	fmax = (gdouble) p->cfg->c.freq_max_hz * 1e-6; /* to MHz */


	fc  = gtk_spin_button_get_value(p->cfg->sb_frq_ce);
	bw2 = gtk_spin_button_get_value(p->cfg->sb_frq_bw) * 0.5;


	if (fc - bw2 < fmin)
		bw2 = fc - fmin;
	if (fc + bw2 > fmax)
		bw2 = fmax - fc;

	radio_input_block_signals(p);

	gtk_spin_button_set_value(p->cfg->sb_frq_bw, bw2 * 2.0);

	gtk_spin_button_set_value(p->cfg->sb_frq_lo, fc - bw2);
	gtk_spin_button_set_value(p->cfg->sb_frq_hi, fc + bw2);

	v0 = doppler_vel(fc - bw2, p->cfg->freq_ref_mhz);
	v1 = doppler_vel(fc + bw2, p->cfg->freq_ref_mhz);

	vcent = doppler_vel(fc, p->cfg->freq_ref_mhz);
	vspan = doppler_vel_relative(bw2 * 2.0, p->cfg->freq_ref_mhz);

	gtk_spin_button_set_value(p->cfg->sb_vel_ce, vcent);
	gtk_spin_button_set_value(p->cfg->sb_vel_bw, vspan);

	gtk_spin_button_set_value(p->cfg->sb_vel_lo, v0);
	gtk_spin_button_set_value(p->cfg->sb_vel_hi, v1);



	radio_input_unblock_signals(p);


	return TRUE;
}




/**
 * @brief create spectral frequency range controls
 */

GtkWidget *radio_acq_freq_range_ctrl_new(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *tmp;

	gchar *lbl;


	grid = GTK_GRID(new_default_grid());


	w = gui_create_desclabel("Acquisition Frequency Range",
				 "Configure the frequency range for spectrum "
				 "acquisition.\n"
				 "Note that the frequency resolution depends "
				 "on the receiver bandwidth settings.");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);

	/* we prime the label with a single whitespace, so the height
	 * is alreay correct when we do the first update so the widgets don't
	 * visually jump around as they are resized */
	tmp = gtk_label_new(NULL);
	lbl = g_strdup_printf("<span foreground='#7AAA7E' size = 'small'> "
			      "</span>");
	gtk_label_set_markup(GTK_LABEL(tmp), lbl);

	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);
	p->cfg->freq_cfg = GTK_LABEL(tmp);


	/* NOTE: we set the high value to MAX_DOUBLE and the initial low value
	 * to MIN_VALUE. Every time we get an update of the capabilities, we
	 * shrink the initial value if the current value does not fit,
	 * otherwise we leave it be
	 */

	w = gtk_label_new("Low [MHz]");
	gtk_grid_attach(grid, w, 2, 1, 1, 1);
	p->cfg->sb_frq_lo_center_lbl = GTK_LABEL(w);

	/* low freq spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the lower\n"
				       "frequency limit");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 1, 1, 1);

	p->cfg->id_fl = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_freq_value_changed), p);

	p->cfg->sb_frq_lo = GTK_SPIN_BUTTON(w);


	/* center freq spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the center\n"
				       "frequency");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	gtk_widget_hide(w); /* default off */

	p->cfg->id_fc = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_center_freq_value_changed), p);

	p->cfg->sb_frq_ce = GTK_SPIN_BUTTON(w);


	/* low vel spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the lower\n"
				       "frequency limit");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	gtk_widget_hide(w); /* default off */

	p->cfg->id_vl = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_vel_value_changed), p);

	p->cfg->sb_vel_lo = GTK_SPIN_BUTTON(w);


	/* center vel spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the center\n"
				       "velocity");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	gtk_widget_hide(w); /* default off */

	p->cfg->id_vc = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_center_vel_value_changed), p);

	p->cfg->sb_vel_ce = GTK_SPIN_BUTTON(w);



	w = gtk_label_new("High [MHz]");
	gtk_grid_attach(grid, w, 2, 2, 1, 1);
	p->cfg->sb_frq_hi_bw_lbl = GTK_LABEL(w);

	/* high freq spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the upper\n"
				       "frequency limit");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 2, 1, 1);

	p->cfg->id_fh = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_freq_value_changed), p);

	p->cfg->sb_frq_hi = GTK_SPIN_BUTTON(w);


	/* span freq spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the frequency\n"
				       "span");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 2, 1, 1);
	gtk_widget_hide(w); /* default off */

	p->cfg->id_fs = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_center_freq_value_changed), p);

	p->cfg->sb_frq_bw = GTK_SPIN_BUTTON(w);



	/* high vel spin button */

	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the upper\n"
				       "velocity limit");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 2, 1, 1);
	gtk_widget_hide(w); /* default off */

	p->cfg->id_vh = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_vel_value_changed), p);

	p->cfg->sb_vel_hi = GTK_SPIN_BUTTON(w);


	/* span freq spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the velocity\n"
				       "span");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 2, 1, 1);
	gtk_widget_hide(w); /* default off */

	p->cfg->id_vs = g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
					 G_CALLBACK(radio_center_vel_value_changed), p);

	p->cfg->sb_vel_bw = GTK_SPIN_BUTTON(w);





	return GTK_WIDGET(grid);
}


/**
 * @brief create spectral frequency/velocity input mode controls
 */

GtkWidget *radio_acq_input_mode_ctrl_new(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;


	GSList *group;


	grid = GTK_GRID(new_default_grid());


	w = gui_create_desclabel("Input Mode",
				 "Configure the input mode for for spectrum "
				 "acquisition.");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);


	w = gtk_radio_button_new_with_label(NULL, "Frequency");
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(w));
	gtk_grid_attach(grid, w, 2, 1, 1, 1);
	p->cfg->rb_frq = GTK_TOGGLE_BUTTON(w);

	g_signal_connect(GTK_TOGGLE_BUTTON(w), "toggled",
			 G_CALLBACK(radio_button_toggle), p);


	w = gtk_radio_button_new_with_label(group, "Velocity");
	gtk_grid_attach(grid, w, 2, 2, 1, 1);
	p->cfg->rb_vel = GTK_TOGGLE_BUTTON(w);

	g_signal_connect(GTK_TOGGLE_BUTTON(w), "toggled",
			 G_CALLBACK(radio_button_toggle), p);



	w = gtk_radio_button_new_with_label(NULL, "Low - High");
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(w));
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	p->cfg->rb_lohi = GTK_TOGGLE_BUTTON(w);

	g_signal_connect(GTK_TOGGLE_BUTTON(w), "toggled",
			 G_CALLBACK(radio_button_toggle), p);


	w = gtk_radio_button_new_with_label(group, "Center - Span");
	gtk_grid_attach(grid, w, 3, 2, 1, 1);
	p->cfg->rb_cbw = GTK_TOGGLE_BUTTON(w);

	g_signal_connect(GTK_TOGGLE_BUTTON(w), "toggled",
			 G_CALLBACK(radio_button_toggle), p);




	return GTK_WIDGET(grid);
}
