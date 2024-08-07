/**
 * @file    widgets/sys_status/sys_status.c
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
 * @brief a widget to show local and remote systems status info
 *
 */


#include <sys_status.h>
#include <sys_status_cfg.h>
#include <sys_status_internal.h>

#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <cmd.h>
#include <math.h>
#include <net.h>


G_DEFINE_TYPE_WITH_PRIVATE(SysStatus, sys_status, GTK_TYPE_BOX)

#define SPIN_LABEL_CHAR_WIDTH	7

/**
 * @brief fetch needed configuration data
 */

static void sys_status_fetch_config(void)
{
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
	cmd_capabilities_load(PKT_TRANS_ID_UNDEF);
	cmd_getpos_azel(PKT_TRANS_ID_UNDEF);
	cmd_spec_acq_cfg_get(PKT_TRANS_ID_UNDEF);
}

/**
 * @brief reset hot load label
 */

static void sys_status_reset_hot(SysStatus *p)
{
	gchar *lbl;


	lbl = g_strdup_printf("<span foreground='#7AAA7E'> N/A </span>");

	gtk_label_set_markup(p->cfg->lbl_hot_load, lbl);

	g_free(lbl);
}


/**
 * @brief handle connected
 */

static void sys_status_connected(gpointer instance, gpointer data)
{
	sys_status_reset_hot(SYS_STATUS(data));
	sys_status_fetch_config();
}


/**
 * @brief handle status acq
 */

static void sys_status_handle_pr_status_acq(gpointer instance,
					    const struct status *s,
					    gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	if (s->busy)
		gtk_spinner_start(GTK_SPINNER(p->cfg->spin_acq));
	else
		gtk_spinner_stop(GTK_SPINNER(p->cfg->spin_acq));

	p->cfg->eta_acq = (gdouble) s->eta_msec / 1000.;
}


/**
 * @brief handle status rec
 */

static void sys_status_handle_pr_status_rec(gpointer instance,
					    const struct status *s,
					    gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	if (s->busy)
		gtk_spinner_start(GTK_SPINNER(p->cfg->spin_rec));
	else
		gtk_spinner_stop(GTK_SPINNER(p->cfg->spin_rec));

	p->cfg->eta_rec = (gdouble) s->eta_msec / 1000.;
}


/**
 * @brief handle status slew
 */

static void sys_status_handle_pr_status_slew(gpointer instance,
					     const struct status *s,
					     gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	if (s->busy)
		gtk_spinner_start(GTK_SPINNER(p->cfg->spin_slew));
	else
		gtk_spinner_stop(GTK_SPINNER(p->cfg->spin_slew));

	p->cfg->eta_slew = (gdouble) s->eta_msec / 1000.;
}


/**
 * @brief handle status move
 */

static void sys_status_handle_pr_status_move(gpointer instance,
					     const struct status *s,
					     gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	if (s->busy)
		gtk_spinner_start(GTK_SPINNER(p->cfg->spin_move));
	else
		gtk_spinner_stop(GTK_SPINNER(p->cfg->spin_move));

	p->cfg->eta_move = (gdouble) s->eta_msec / 1000.;
}




/**
 * @brief handle capabilities data
 */

static void sys_status_handle_pr_capabilities(gpointer instance,
					      const struct capabilities *c,
					      gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);

	p->cfg->lat = c->lat_arcsec / 3600.0;
	p->cfg->lon = c->lon_arcsec / 3600.0;
}

/**
 * @brief handle capabilities_load data
 */

static void sys_status_handle_pr_capabilities_load(gpointer instance,
						   const struct capabilities_load *c,
						   gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);

	p->cfg->lat = c->lat_arcsec / 3600.0;
	p->cfg->lon = c->lon_arcsec / 3600.0;

	p->cfg->hot_load_temp = (gdouble) c->hot_load;

	if (!c->hot_load)
		sys_status_reset_hot(data);

}


/**
 * @brief handle spectral acquisition configuration data
 */

static void sys_status_handle_pr_spec_acq_cfg(gpointer instance,
					      const struct spec_acq_cfg *acq,
					      gpointer data)
{
	SysStatus *p;

	gchar *lbl = NULL;


	p = SYS_STATUS(data);

	p->cfg->frq_lo = (gdouble) acq->freq_start_hz * 1e-6;
	p->cfg->frq_hi = (gdouble) acq->freq_stop_hz  * 1e-6;

	lbl = g_strdup_printf("<tt>%06.2f MHz</tt>", p->cfg->frq_lo);
	gtk_label_set_markup(p->cfg->lbl_frq_lo, lbl);
	g_free(lbl);

	lbl = g_strdup_printf("<tt>%06.2f MHz</tt>", p->cfg->frq_hi);
	gtk_label_set_markup(p->cfg->lbl_frq_hi, lbl);
	g_free(lbl);
}


/**
 * @brief handle status hot_load_enable
 */
static void sys_status_handle_pr_hot_load_enable(gpointer instance,
						 gpointer data)
{
	SysStatus *p;

	gchar *lbl;


	p = SYS_STATUS(data);


	/* if hot load turned on, but we don't know it's temperature yet,
	 * send another request
	 */
	if (p->cfg->hot_load_temp == 0.0) {
		cmd_capabilities_load(PKT_TRANS_ID_UNDEF);
		return;
	}

	/* convert mK to K */
	lbl = g_strdup_printf("<span foreground='#FF0000'> %g K </span>",
			       0.001 * (gdouble) p->cfg->hot_load_temp);


	gtk_label_set_markup(p->cfg->lbl_hot_load, lbl);

	g_free(lbl);
}


/**
 * @brief handle status hot_load_disable
 */
static void sys_status_handle_pr_hot_load_disable(gpointer instance,
						  gpointer data)
{
	SysStatus *p;

	gchar *lbl;


	p = SYS_STATUS(data);

	lbl = g_strdup_printf("<span foreground='#7AAA7E'> OFF </span>");

	gtk_label_set_markup(p->cfg->lbl_hot_load, lbl);

	g_free(lbl);
}


static void sys_status_hide_widgets(GtkWidget *w, gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	gtk_widget_set_no_show_all(p->cfg->grid, TRUE);
	gtk_widget_hide(p->cfg->grid);

	gtk_widget_set_no_show_all(p->cfg->btn_hide, TRUE);
	gtk_widget_hide(p->cfg->btn_hide);

	gtk_widget_set_no_show_all(p->cfg->btn_show, FALSE);
	gtk_widget_show(p->cfg->btn_show);
}


static void sys_status_show_widgets(GtkWidget *w, gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	gtk_widget_set_no_show_all(p->cfg->grid, FALSE);
	gtk_widget_show(p->cfg->grid);

	gtk_widget_set_no_show_all(p->cfg->btn_hide, FALSE);
	gtk_widget_show(p->cfg->btn_hide);

	gtk_widget_set_no_show_all(p->cfg->btn_show, TRUE);
	gtk_widget_hide(p->cfg->btn_show);
}




static void gui_create_sys_status_controls(SysStatus *p)
{
	GtkWidget *w;
	GtkWidget *box;
	GtkWidget *grid;
	GtkWidget *overlay;



	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);



	overlay = gtk_overlay_new();
	gtk_container_add(GTK_CONTAINER(overlay), box);
	gtk_box_pack_start(GTK_BOX(p), overlay, TRUE, TRUE, 6);




	grid = gtk_grid_new();
	p->cfg->grid = grid;

	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	g_object_set(grid, "margin", 6, NULL);

	gtk_box_pack_start(GTK_BOX(box), grid, FALSE, TRUE, 0);
	gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(grid, TRUE);


	w = sys_status_pos_new(p);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);

	{
		GtkWidget *grid2;

		grid2 = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(grid2), 6);
		gtk_grid_set_column_spacing(GTK_GRID(grid2), 12);


		w = sys_status_create_align_lbl("<span alpha='50%'>F<span size='x-small'>LO</span></span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 4, 1, 1);
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 4, 2, 1);
		p->cfg->lbl_frq_lo = GTK_LABEL(w);

		w = sys_status_create_align_lbl("<span alpha='50%'>F<span size='x-small'>HI</span></span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 5, 1, 1);
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 5, 2, 1);
		p->cfg->lbl_frq_hi = GTK_LABEL(w);;

		w = sys_status_create_align_lbl("<span alpha='50%'>Hot Load</span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 6, 1, 1);
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 6, 1, 1);
		p->cfg->lbl_hot_load = GTK_LABEL(w);

		/* reset label text */
		sys_status_reset_hot(p);


		gtk_grid_attach(GTK_GRID(grid), grid2, 2, 0, 1, 1);
	}

	w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_grid_attach(GTK_GRID(grid), w, 3, 0, 1, 1);


	{
		GtkWidget *grid2;

		grid2 = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(grid2), 6);
		gtk_grid_set_column_spacing(GTK_GRID(grid2), 12);


		w = sys_status_create_align_lbl("<span alpha='50%'>Acquisition</span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 0, 1, 1);
		w = gtk_spinner_new();
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 0, 1, 1);
		p->cfg->spin_acq = w;
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 2, 0, 1, 1);
		p->cfg->lbl_eta_acq = GTK_LABEL(w);
		gtk_label_set_width_chars(GTK_LABEL(w), SPIN_LABEL_CHAR_WIDTH);

		w = sys_status_create_align_lbl("<span alpha='50%'>Recording</span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 1, 1, 1);
		w = gtk_spinner_new();
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 1, 1, 1);
		p->cfg->spin_rec = w;
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 2, 1, 1, 1);
		p->cfg->lbl_eta_rec = GTK_LABEL(w);
		gtk_label_set_width_chars(GTK_LABEL(w), SPIN_LABEL_CHAR_WIDTH);

		w = sys_status_create_align_lbl("<span alpha='50%'>Slewing</span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 2, 1, 1);
		w = gtk_spinner_new();
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 2, 1, 1);
		p->cfg->spin_slew = w;
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 2, 2, 1, 1);
		p->cfg->lbl_eta_slew = GTK_LABEL(w);
		gtk_label_set_width_chars(GTK_LABEL(w), SPIN_LABEL_CHAR_WIDTH);

		w = sys_status_create_align_lbl("<span alpha='50%'>Moving</span>", 1.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 0, 3, 1, 1);
		w = gtk_spinner_new();
		gtk_grid_attach(GTK_GRID(grid2), w, 1, 3, 1, 1);
		p->cfg->spin_move = w;
		w = sys_status_create_align_lbl(NULL, 0.0);
		gtk_grid_attach(GTK_GRID(grid2), w, 2, 3, 1, 1);
		p->cfg->lbl_eta_move = GTK_LABEL(w);
		gtk_label_set_width_chars(GTK_LABEL(w), SPIN_LABEL_CHAR_WIDTH);

		gtk_grid_attach(GTK_GRID(grid), grid2, 4, 0, 1, 1);
	}





	w = sys_status_info_bar_new(p);
	gtk_widget_set_vexpand(w, FALSE);
	gtk_widget_set_hexpand(w, TRUE);

	gtk_widget_set_halign(w, GTK_ALIGN_FILL);
	gtk_widget_set_valign(w, GTK_ALIGN_END);

	gtk_overlay_add_overlay (GTK_OVERLAY (overlay), w);
	gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), w, TRUE);




	w = gtk_button_new_from_icon_name("pan-down-symbolic",
					    GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, TRUE, 6);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_widget_set_valign(w, GTK_ALIGN_END);
	gtk_widget_set_hexpand(w, TRUE);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(sys_status_hide_widgets),
			 (gpointer) p);
	p->cfg->btn_hide = w;

	w = gtk_button_new_from_icon_name("pan-up-symbolic",
					    GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, TRUE, 6);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_widget_set_valign(w, GTK_ALIGN_END);
	gtk_widget_set_hexpand(w, TRUE);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(sys_status_show_widgets),
			 (gpointer) p);
	gtk_widget_set_no_show_all(GTK_WIDGET(w), TRUE);
	p->cfg->btn_show = w;
}


/**
 * @brief destroy signal handler
 */

static gboolean sys_status_destroy(GtkWidget *w, void *data)
{
	SysStatus *p;


	p = SYS_STATUS(w);

	g_source_remove(p->cfg->id_to);

	if (p->cfg->id_to_msg)
	    g_source_remove(p->cfg->id_to_msg);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_pos);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cap);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_lod);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_acq);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_slw);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_mov);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_rec);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cfg);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_msg);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_con);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_hot_ena);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_hot_dis);


	return TRUE;
}


/**
 * @brief initialise the SysStatus class
 */

static void sys_status_class_init(SysStatusClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the SysStatus widget
 */

static void sys_status_init(SysStatus *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_SYS_STATUS(p));

	p->cfg = sys_status_get_instance_private(p);

	p->cfg->id_to_msg = 0;;

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_sys_status_controls(p);

	p->cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
				 G_CALLBACK(sys_status_handle_pr_capabilities),
				 (void *) p);

	p->cfg->id_lod = g_signal_connect(sig_get_instance(), "pr-capabilities-load",
				  G_CALLBACK(sys_status_handle_pr_capabilities_load),
				  (void *) p);



	p->cfg->id_acq = g_signal_connect(sig_get_instance(), "pr-status-acq",
				 G_CALLBACK(sys_status_handle_pr_status_acq),
				 (void *) p);

	p->cfg->id_slw = g_signal_connect(sig_get_instance(), "pr-status-slew",
				 G_CALLBACK(sys_status_handle_pr_status_slew),
				 (void *) p);

	p->cfg->id_mov = g_signal_connect(sig_get_instance(), "pr-status-move",
				 G_CALLBACK(sys_status_handle_pr_status_move),
				 (void *) p);

	p->cfg->id_rec = g_signal_connect(sig_get_instance(), "pr-status-rec",
				 G_CALLBACK(sys_status_handle_pr_status_rec),
				 (void *) p);

	p->cfg->id_cfg = g_signal_connect(sig_get_instance(), "pr-spec-acq-cfg",
				 G_CALLBACK(sys_status_handle_pr_spec_acq_cfg),
				 (void *) p);

	p->cfg->id_msg = g_signal_connect(sig_get_instance(), "status-push",
				 G_CALLBACK(sys_status_handle_status_push),
				 (void *) p);

	p->cfg->id_con = g_signal_connect(sig_get_instance(), "net-connected",
				 G_CALLBACK(sys_status_connected),
				 (void *) p);

	p->cfg->id_hot_ena = g_signal_connect(sig_get_instance(),
			 "pr-hot-load-enable",
			 G_CALLBACK(sys_status_handle_pr_hot_load_enable),
			 (gpointer) p);

	p->cfg->id_hot_dis = g_signal_connect(sig_get_instance(),
			 "pr-hot-load-disable",
			 G_CALLBACK(sys_status_handle_pr_hot_load_disable),
			 (gpointer) p);


	g_signal_connect(p, "destroy", G_CALLBACK(sys_status_destroy), NULL);

	if (net_is_connected())
		sys_status_fetch_config();
}


/**
 * @brief create a new SysStatus widget
 */

GtkWidget *sys_status_new(void)
{
	SysStatus *sys_status;


	sys_status = g_object_new(TYPE_SYS_STATUS, NULL);

	return GTK_WIDGET(sys_status);
}
