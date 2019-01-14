/**
 * @file    widgets/radio/radio_update.c
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
 * @brief functions to handle configuration and label updates
 *
 */

#include <radio.h>
#include <radio_cfg.h>



/**
 *  @brief update frequency range spin button given the new configuration data
 */

static void radio_update_freq_range_spin_button(Radio *p, GtkSpinButton *b,
						gboolean is_bw)
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


	if (is_bw) {
		/* modify range and range check for bandwidth button */
		gtk_spin_button_set_range(b, 0.0, fmax - fmin);
		fmin = 0.0;
		fmax = fmax - fmin;
	} else {
		gtk_spin_button_set_range(b, fmin, fmax);
	}

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


/**
 * @brief update the lower/upper frequency range spin buttons
 */

void radio_update_freq_range(Radio *p)
{
	radio_update_freq_range_spin_button(p, p->cfg->sb_frq_lo, FALSE);
	radio_update_freq_range_spin_button(p, p->cfg->sb_frq_hi, FALSE);
	radio_update_freq_range_spin_button(p, p->cfg->sb_frq_center, FALSE);
	radio_update_freq_range_spin_button(p, p->cfg->sb_frq_bw, TRUE);
}


/**
 * @brief update the bandwidth divider setting and spin button
 */

void radio_update_bw_divider(Radio *p)
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


/**
 * @brief update the bin divider setting and spin button
 */

void radio_update_bin_divider(Radio *p)
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


/**
 * @brief update the averaging button
 */

void radio_update_avg_range(Radio *p)
{
	gtk_spin_button_set_range(p->cfg->sb_avg, 0, p->cfg->c.n_stack_max);
	gtk_spin_button_set_value(p->cfg->sb_avg, 0);
}



/**
 * @brief display the currently configured averaging on the remote
 */

void radio_update_avg_lbl(Radio *p)
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


/**
 * @brief display the currently configured bandwidth setting on the remote
 */

void radio_update_conf_bw_lbl(Radio *p)
{
	gchar *lbl;

	gdouble bw, bd;


	if (p->cfg->c.bw_max_div_lin)
		bw = p->cfg->c.bw_max_hz / (gdouble) (p->cfg->bw_div + 1);
	else
		bw = p->cfg->c.bw_max_hz / (gdouble) (1 << p->cfg->bw_div);

	if (p->cfg->c.bw_max_bin_div_lin)
		bd = p->cfg->c.bw_max_bins / (gdouble) (p->cfg->bin_div + 1);
	else
		bd = p->cfg->c.bw_max_bins / (gdouble) (1 << p->cfg->bin_div);


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
 * @brief display the currently configured frequency range on the remote
 */

void radio_update_conf_freq_lbl(Radio *p)
{
	gchar *lbl;

	gdouble f0, f1;


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
