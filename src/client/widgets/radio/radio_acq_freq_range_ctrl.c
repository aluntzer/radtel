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
 */


#include <radio.h>
#include <radio_cfg.h>

#include <default_grid.h>
#include <desclabel.h>


static gint radio_freq_value_changed(GtkSpinButton *sb, Radio *p);
static gint radio_center_freq_value_changed(GtkSpinButton *sb, Radio *p);



/**
 * @brief show low/high range configuration
 */

static void radio_button_show_lohi(Radio *p)
{
	gtk_label_set_text(p->cfg->sb_frq_lo_center_lbl, "Low");
	gtk_label_set_text(p->cfg->sb_frq_hi_bw_lbl, "High");
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_lo));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_hi));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_center));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_bw));
}


/**
 * @brief show center/bandwidth range configuration
 */

static void radio_button_show_center_bw(Radio *p)
{
	gtk_label_set_text(p->cfg->sb_frq_lo_center_lbl, "Center");
	gtk_label_set_text(p->cfg->sb_frq_hi_bw_lbl, "Bandwidth");
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_center));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_frq_bw));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_lo));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_frq_hi));

}

/**
 * @brief signal handler for the lo-hi input mode radio button
 */

static void radio_button_toggle_lohi(GtkWidget *button, Radio *p)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		radio_button_show_lohi(p);
	else
		radio_button_show_center_bw(p);
}


/**
 * @brief signal handler for the center input mode radio button
 */

static void radio_button_toggle_center(GtkWidget *button, Radio *p)
{
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
		radio_button_show_lohi(p);
	else
		radio_button_show_center_bw(p);
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
	gdouble f0, f1;

	const GCallback cb = G_CALLBACK(radio_center_freq_value_changed);


	f0 = gtk_spin_button_get_value(p->cfg->sb_frq_lo);
	f1 = gtk_spin_button_get_value(p->cfg->sb_frq_hi);


	/* must block handler or we'll enter a update loop */
	g_signal_handlers_block_matched(p->cfg->sb_frq_center,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	g_signal_handlers_block_matched(p->cfg->sb_frq_bw,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	/* always update hidden spin buttons */
	gtk_spin_button_set_value(p->cfg->sb_frq_center, (f0 + f1) * 0.5);
	gtk_spin_button_set_value(p->cfg->sb_frq_bw, (f1 - f0));

	g_signal_handlers_unblock_matched(p->cfg->sb_frq_center,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
	g_signal_handlers_unblock_matched(p->cfg->sb_frq_bw,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);	if (f0 < f1)
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

	/* update hidden */
	g_signal_handlers_block_matched(p->cfg->sb_frq_center,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	g_signal_handlers_block_matched(p->cfg->sb_frq_bw,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);

	gtk_spin_button_set_value(p->cfg->sb_frq_center, (f0 + f1) * 0.5);
	gtk_spin_button_set_value(p->cfg->sb_frq_bw, (f1 - f0));

	g_signal_handlers_unblock_matched(p->cfg->sb_frq_center,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
	g_signal_handlers_unblock_matched(p->cfg->sb_frq_bw,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);

	return TRUE;
}


/**
 * @brief see if one of the center frequency spin buttons are within legal
 *	 limits and clamp bandwidth if needed
 */

static gint radio_center_freq_value_changed(GtkSpinButton *sb, Radio *p)
{
	gdouble fc, bw2;
	gdouble f0, f1;

	gdouble fmin;
	gdouble fmax;

	const GCallback cb = G_CALLBACK(radio_freq_value_changed);


	fmin = (gdouble) p->cfg->c.freq_min_hz * 1e-6; /* to MHz */
	fmax = (gdouble) p->cfg->c.freq_max_hz * 1e-6; /* to MHz */


	fc  = gtk_spin_button_get_value(p->cfg->sb_frq_center);
	bw2 = gtk_spin_button_get_value(p->cfg->sb_frq_bw) * 0.5;


	/* always update hidden spin buttons */
	g_signal_handlers_block_matched(p->cfg->sb_frq_lo,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	g_signal_handlers_block_matched(p->cfg->sb_frq_hi,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);

	gtk_spin_button_set_value(p->cfg->sb_frq_lo, fc - bw2);
	gtk_spin_button_set_value(p->cfg->sb_frq_hi, fc + bw2);


	g_signal_handlers_unblock_matched(p->cfg->sb_frq_lo,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
	g_signal_handlers_unblock_matched(p->cfg->sb_frq_hi,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);


	if (fc - bw2 > fmin)
		if (fc + bw2 < fmax)
			return TRUE;



	if (fc - bw2 < fmin)
		bw2 = fc - fmin;


	if (fc + bw2 > fmax)
		bw2 = fmax - fc;

	gtk_spin_button_set_value(p->cfg->sb_frq_bw, bw2 * 2.0);

	/* update hidden */
	g_signal_handlers_block_matched(p->cfg->sb_frq_lo,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	g_signal_handlers_block_matched(p->cfg->sb_frq_hi,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);

	gtk_spin_button_set_value(p->cfg->sb_frq_lo, fc - bw2);
	gtk_spin_button_set_value(p->cfg->sb_frq_hi, fc + bw2);

	g_signal_handlers_unblock_matched(p->cfg->sb_frq_lo,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
	g_signal_handlers_unblock_matched(p->cfg->sb_frq_hi,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);

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
	GtkWidget *box;

	gchar *lbl;

	GSList *group = NULL;


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


	tmp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_margin_start(tmp, 8);
	gtk_box_pack_start(GTK_BOX(w), tmp, FALSE, FALSE, 0);

	w = gtk_label_new("Input Mode:");
	gtk_box_pack_start(GTK_BOX(tmp), w, FALSE, FALSE, 0);

	w = gtk_radio_button_new_with_label(group, "Low - High");
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(w));
	g_signal_connect(GTK_TOGGLE_BUTTON(w), "toggled",
			 G_CALLBACK(radio_button_toggle_lohi), p);
	gtk_box_pack_start(GTK_BOX(tmp), w, FALSE, FALSE, 0);


	w = gtk_radio_button_new_with_label(group, "Center - Bandwidth ");
	g_signal_connect(GTK_TOGGLE_BUTTON(w), "toggled",
			 G_CALLBACK(radio_button_toggle_center), p);
	gtk_box_pack_start(GTK_BOX(tmp), w, FALSE, FALSE, 0);



	/* NOTE: we set the high value to MAX_DOUBLE and the initial low value
	 * to MIN_VALUE. Every time we get an update of the capabilities, we
	 * shrink the initial value if the current value does not fit,
	 * otherwise we leave it be
	 */

	w = gtk_label_new("Low");
	gtk_grid_attach(grid, w, 2, 1, 1, 1);
	p->cfg->sb_frq_lo_center_lbl = GTK_LABEL(w);

	/* low spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the lower\n"
				       "frequency limit");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 1, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(radio_freq_value_changed), p);

	p->cfg->sb_frq_lo = GTK_SPIN_BUTTON(w);


	/* center spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the center\n"
				       "frequency");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MINDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 1, 1, 1);
	gtk_widget_hide(w); /* default off */

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(radio_center_freq_value_changed), p);

	p->cfg->sb_frq_center = GTK_SPIN_BUTTON(w);



	w = gtk_label_new("High");
	gtk_grid_attach(grid, w, 2, 2, 1, 1);
	p->cfg->sb_frq_hi_bw_lbl = GTK_LABEL(w);

	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the upper\n"
				       "frequency limit");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 2, 1, 1);

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(radio_freq_value_changed), p);

	p->cfg->sb_frq_hi = GTK_SPIN_BUTTON(w);


	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_widget_set_tooltip_text(w, "Set the frequency\n"
				       "bandwidth");
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), G_MINDOUBLE, G_MAXDOUBLE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), G_MAXDOUBLE);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 2, 1, 1);
	gtk_widget_hide(w); /* default off */

	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(radio_center_freq_value_changed), p);

	p->cfg->sb_frq_bw = GTK_SPIN_BUTTON(w);





	return GTK_WIDGET(grid);
}

