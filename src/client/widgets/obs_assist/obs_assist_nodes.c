/**
 * @file    widgets/obs_assist/obs_assist_nodes.c
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
 * @brief freely program an observation using functional flow graph nodes
 *
 */



#include <obs_assist.h>
#include <obs_assist_cfg.h>
#include <obs_assist_internal.h>

#include <coordinates.h>
#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <xyplot.h>
#include <cmd.h>
#include <math.h>

#include <gtknodeview.h>
#include <nodes.h>


static void obs_assist_node_create_pulse_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;


	node = pulse_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);
	gtk_widget_show_all(node);
}


static void obs_assist_node_create_step_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;

	node = step_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);
	gtk_widget_show_all(p->cfg->nodes.node_view);
}

static void obs_assist_node_create_medfilt_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;

	node = medfilt_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);

	gtk_widget_show_all(p->cfg->nodes.node_view);
}

static void obs_assist_node_create_spec_src_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;

	node = specsrc_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);
	gtk_widget_show_all(p->cfg->nodes.node_view);
}

static void obs_assist_node_create_plot_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;

	node = plot_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);
	gtk_widget_show_all(p->cfg->nodes.node_view);
}


static void obs_assist_node_create_coordinates_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;

	node = coordinates_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);
	gtk_widget_show_all(p->cfg->nodes.node_view);
}


static void obs_assist_node_create_target_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *node;

	node = target_new();
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), node);
	gtk_widget_show_all(p->cfg->nodes.node_view);
}


static void obs_assist_node_save_cb(GtkWidget *widget, ObsAssist *p)
{
	gtk_nodes_node_view_save (GTKNODES_NODE_VIEW(p->cfg->nodes.node_view),
				  "test.glade");
}


static void obs_assist_node_load_cb(GtkWidget *widget, ObsAssist *p)
{
  	/* apparently I have to load them once, need export? */
	GtkWidget *w;
#if 1
	w = coordinates_new();
	gtk_widget_destroy(w);

	w = medfilt_new();
	gtk_widget_destroy(w);

	w = plot_new();
	gtk_widget_destroy(w);

	w = pulse_new();
	gtk_widget_destroy(w);

	w = specsrc_new();
	gtk_widget_destroy(w);

	w = step_new();
	gtk_widget_destroy(w);

	w = target_new();
	gtk_widget_destroy(w);
#endif

	gtk_nodes_node_view_load(GTKNODES_NODE_VIEW (p->cfg->nodes.node_view),
				 "test.glade");
}


/**
 * @brief build the right-click popup menu
 */

static void obs_assist_node_build_popup_menu(ObsAssist *p)
{
	GtkWidget *menuitem;


	if (p->cfg->nodes.menu)
		gtk_widget_destroy(p->cfg->nodes.menu);

	p->cfg->nodes.menu = gtk_menu_new();


	menuitem = gtk_menu_item_new_with_label("LOAD");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_load_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("SAVE");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_save_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Pulse");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_pulse_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Step");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_step_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Coordinates");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_coordinates_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Target");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_target_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Spectrum Source");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_spec_src_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Plot");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_plot_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);

	menuitem = gtk_menu_item_new_with_label("Median Filter");
	g_signal_connect(menuitem, "activate",
			 G_CALLBACK(obs_assist_node_create_medfilt_cb), p);
	gtk_menu_shell_append(GTK_MENU_SHELL(p->cfg->nodes.menu), menuitem);



	gtk_widget_show_all(p->cfg->nodes.menu);
}

/**
 * @brief show the right-click popup menu
 */

static void obs_assist_node_popup_menu(GtkWidget *widget, ObsAssist *p)
{
	if (!p->cfg->nodes.menu)
		obs_assist_node_build_popup_menu(p);

	gtk_menu_popup_at_pointer(GTK_MENU(p->cfg->nodes.menu), NULL);
}


static gboolean obs_assist_node_button_press_cb(GtkWidget *widget,
						GdkEventButton *event,
						ObsAssist *p)
{
	if (event->type == GDK_BUTTON_PRESS)
		if (event->button == 3)
			obs_assist_node_popup_menu(widget, p);

	return TRUE;
}


/**
 * @brief populate the assistant
 */

static void obs_assist_node_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *frame;

	p->cfg->abort = FALSE;
	obs_assist_hide_procedure_selectors(p);


	frame = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(p), frame, TRUE, TRUE, 0);

	p->cfg->nodes.node_view = gtk_nodes_node_view_new();
	gtk_container_add(GTK_CONTAINER(frame), p->cfg->nodes.node_view);

	g_signal_connect (G_OBJECT(p->cfg->nodes.node_view),
			  "button-press-event",
			  G_CALLBACK(obs_assist_node_button_press_cb), p);

	gtk_widget_show_all(GTK_WIDGET(frame));
}


/**
 * @brief create nodes selection
 */

GtkWidget *obs_assist_nodes_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	p->cfg->nodes.menu = NULL;

	w = gui_create_desclabel("Node Editor",
				 "Construct your observation.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Node Editor.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_node_setup_cb), p);


	return GTK_WIDGET(grid);
}
