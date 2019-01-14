/**
 * @file    widgets/radio/radio_acq_res.c
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
 * @brief handle capabilities data and hide the spectral bins and bw selector
 *	  labels and spin buttons if the dividers are 0
 */

static void radio_acq_res_handle_pr_capabilities(gpointer instance,
						 const struct capabilities *c,
						 Radio *p)
{
	if (c->bw_max_div_lin || c->bw_max_div_rad2) {
		gtk_widget_show(GTK_WIDGET(p->cfg->sb_bw_div));
		gtk_widget_show(GTK_WIDGET(p->cfg->sb_bw_lbl));
	} else {
		gtk_widget_hide(GTK_WIDGET(p->cfg->sb_bw_div));
		gtk_widget_hide(GTK_WIDGET(p->cfg->sb_bw_lbl));
	}


	if (c->bw_max_bin_div_lin || c->bw_max_bin_div_rad2) {
		gtk_widget_show(GTK_WIDGET(p->cfg->sb_bin_div));
		gtk_widget_show(GTK_WIDGET(p->cfg->sb_bin_lbl));
	} else {
		gtk_widget_hide(GTK_WIDGET(p->cfg->sb_bin_div));
		gtk_widget_hide(GTK_WIDGET(p->cfg->sb_bin_lbl));
	}
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
 * @brief create spectral resolution controls
 *
 * @note here we configure the spectral resolution. The remote device
 *	 would typically support either 2^n (SRT: n_max=2) or linear dividers
 *	 based on a given acquisition bandwidth (500 kHz for the SRT) and a
 *	 range of bins (SRT: 64) with equally either 2^n or linear
 *	 dividiers (SRT: none) for which we must generate the proper selections
 *	 below
 */

GtkWidget *radio_acq_res_ctrl_new(Radio *p)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *tmp;


	grid = GTK_GRID(new_default_grid());



	w = gui_create_desclabel("Bandwidth Resolution",
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


	w = gtk_label_new("Bandwidth");
	gtk_grid_attach(grid, w, 1, 1, 1, 1);
	p->cfg->sb_bw_lbl = GTK_LABEL(w);

	w = gtk_spin_button_new(NULL, 1, 1);
	gtk_widget_set_tooltip_text(w, "Set the acquisition\n"
				       "bandwidth");
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
	p->cfg->sb_bin_lbl = GTK_LABEL(w);

	w = gtk_spin_button_new(NULL, 1, 1);
	gtk_widget_set_tooltip_text(w, "Set the number of data\n"
				       "bins per bandwidth");
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 1, 1);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w), 3);

	gtk_grid_attach(grid, w, 2, 2, 1, 1);
	g_signal_connect(G_OBJECT(w), "input",
			 G_CALLBACK(radio_sb_bin_div_input_cb), p);
	g_signal_connect(G_OBJECT(w), "output",
			 G_CALLBACK(radio_sb_bin_div_output_cb), p);

	p->cfg->sb_bin_div = GTK_SPIN_BUTTON(w);


	g_signal_connect(sig_get_instance(), "pr-capabilities",
			 G_CALLBACK(radio_acq_res_handle_pr_capabilities), p);



	return GTK_WIDGET(grid);
}

