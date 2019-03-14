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
#include <radio_cfg.h>
#include <radio_internal.h>

#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <cmd.h>
#include <math.h>


G_DEFINE_TYPE_WITH_PRIVATE(Radio, radio, GTK_TYPE_BOX)


/**
 * @brief handle capabilities data
 */

static void radio_handle_pr_capabilities(gpointer instance,
					 const struct capabilities *c,
					 gpointer data)
{
	Radio *p;


	p = RADIO(data);

	p->cfg->c = (*c);

	radio_update_bw_divider(p);
	radio_update_bin_divider(p);
	radio_update_freq_range(p);
	radio_update_avg_range(p);
}


/**
 * @brief handle spectral acquisition configuration data
 */

static void radio_handle_pr_spec_acq_cfg(gpointer instance,
					 const struct spec_acq_cfg *acq,
					 gpointer data)
{
	int div;

	gdouble f0, f1;

	Radio *p;


	p = RADIO(data);


	f0 = (gdouble) acq->freq_start_hz * 1e-6;
	f1 = (gdouble) acq->freq_stop_hz  * 1e-6;

	if (p->cfg->c.bw_max_div_lin)
		div = p->cfg->c.bw_max_div_lin - acq->bw_div;
	else
		div = p->cfg->c.bw_max_div_rad2 - acq->bw_div;

	gtk_spin_button_set_value(p->cfg->sb_frq_lo,  f0);
	gtk_spin_button_set_value(p->cfg->sb_frq_hi,  f1);
	gtk_spin_button_set_value(p->cfg->sb_bw_div,  div);
	gtk_spin_button_set_value(p->cfg->sb_bin_div, acq->bin_div);
	gtk_spin_button_set_value(p->cfg->sb_avg,     acq->n_stack);

	radio_update_avg_lbl(p);
	radio_update_conf_freq_lbl(p);
	radio_update_conf_bw_lbl(p);


	/* acq_max is useless (counts down if running), ignore */
}



/**
 * @brief handle status acq
 *
 * @note we use the acq status to update the state of the acqusition control
 *       button, because there is no status-get command and I don't want to add
 *       one because the spectrometer backend is supposed to push the acq status
 *       anyway
 */

static void radio_handle_pr_status_acq(gpointer instance,
					    const struct status *s,
					    gpointer data)
{
	Radio *p;


	p = RADIO(data);

	if (!s->busy)
		return;

	if (!gtk_switch_get_active(p->cfg->sw_acq))
		radio_spec_acq_cmd_spec_acq_enable(instance, data);
}






static void gui_create_radio_controls(Radio *p)
{
	GtkWidget *w;


	w = radio_spec_cfg_ctrl_get_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = radio_vrest_ctrl_new();
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = radio_acq_freq_range_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = radio_acq_res_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = radio_spec_avg_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = radio_spec_cfg_ctrl_set_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	/* XXX move out of here */
	w = radio_spec_acq_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
}


/**
 * @brief destroy signal handler
 */

static gboolean radio_destroy(GtkWidget *w, void *data)
{
	Radio *p;


	p = RADIO(w);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cap);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cfg);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_acq);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_ena);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_dis);

	return TRUE;
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

	p->cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
			 G_CALLBACK(radio_handle_pr_capabilities),
			 (void *) p);

	p->cfg->id_cfg = g_signal_connect(sig_get_instance(), "pr-spec-acq-cfg",
			 G_CALLBACK(radio_handle_pr_spec_acq_cfg),
			 (void *) p);

	p->cfg->id_acq = g_signal_connect(sig_get_instance(), "pr-status-acq",
			 G_CALLBACK(radio_handle_pr_status_acq),
			 (void *) p);

	p->cfg->id_ena = g_signal_connect(sig_get_instance(),
			 "pr-spec-acq-enable",
			 G_CALLBACK(radio_spec_acq_cmd_spec_acq_enable),
			 (gpointer) p);

	p->cfg->id_dis = g_signal_connect(sig_get_instance(),
			 "pr-spec-acq-disable",
			 G_CALLBACK(radio_spec_acq_cmd_spec_acq_disable),
			 (gpointer) p);

	g_signal_connect(p, "destroy", G_CALLBACK(radio_destroy), NULL);
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
