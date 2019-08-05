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


#if 0
gchar *g_object_property_to_string(const gchar *property_name,
				   GValue *value,
				   GType type)
{
	gchar *string = NULL;

	switch (type)
	{
	case G_TYPE_INVALID:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_NONE:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_INTERFACE:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_CHAR:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_UCHAR:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_BOOLEAN:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_INT:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup_printf ("%d", g_value_get_int (value));
		break;
	case G_TYPE_UINT:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup_printf ("%d", g_value_get_uint (value));
		break;
	case G_TYPE_LONG:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup_printf ("%d", g_value_get_long (value));
		break;
	case G_TYPE_ULONG:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup_printf ("%d", g_value_get_ulong (value));
		break;
	case G_TYPE_INT64:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup_printf ("%d", g_value_get_int64 (value));
		break;
	case G_TYPE_UINT64:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup_printf ("%d", g_value_get_uint64 (value));
		break;
	case G_TYPE_ENUM:
		g_print("%s %d\n", __func__, __LINE__);
		{
			GEnumClass *eclass;
			gchar *string = NULL;
			guint i;
			g_print("enum!\n\n");
			g_return_val_if_fail ((eclass = g_type_class_ref (type)) != NULL, NULL);
			for (i = 0; i < eclass->n_values; i++)
			{
				if (g_value_get_enum (value) == eclass->values[i].value)
				{
					string = g_strdup (eclass->values[i].value_nick);
					break;
				}
			}
			g_type_class_unref (eclass);
			return string;
		}
		break;
	case G_TYPE_FLAGS:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_FLOAT:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_DOUBLE:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_STRING:
		g_print("%s %d\n", __func__, __LINE__);
		string = g_strdup (g_value_get_string (value));
		break;
	case G_TYPE_POINTER:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_BOXED:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_PARAM:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_OBJECT:
		g_print("%s %d\n", __func__, __LINE__);
		break;
	case G_TYPE_VARIANT:
		g_print("%s %d\n", __func__, __LINE__);
		break;

	default:
		g_print("\nWEIRD: %x %s %d\n\n",type, __func__, __LINE__);
	}

	return string;
}



GObject *
g_object_clone(GObject *src)
{
    GObject *dst;
    GParameter *params;
    GParamSpec **specs;
    guint n, n_specs, n_params;

    specs = g_object_class_list_properties(G_OBJECT_GET_CLASS(src), &n_specs);
    params = g_new0(GParameter, n_specs);
    n_params = 0;


    g_print("Object %s\n", G_OBJECT_TYPE_NAME(src));


    for (n = 0; n < n_specs; ++n)
        if (strcmp(specs[n]->name, "parent") &&
            (specs[n]->flags & G_PARAM_READWRITE) == G_PARAM_READWRITE) {
            params[n_params].name = g_intern_string(specs[n]->name);
            g_value_init(&params[n_params].value, specs[n]->value_type);
            g_object_get_property(src, specs[n]->name, &params[n_params].value);

	    gchar *str = g_object_property_to_string (specs[n]->name,
					 &params[n_params].value,
					 specs[n]->value_type);

	  g_print("%s: %s\n", specs[n]->name, str);


            n_params++;


        }

//    dst = g_object_newv(G_TYPE_FROM_INSTANCE(src), n_params, params);
    g_free(specs);
    g_free(params);

    return dst;
}


static void
traverse_container (GtkWidget *widget,
		    gpointer   data)
{
	if (GTK_IS_WIDGET (widget))
		g_object_clone(G_OBJECT(widget));
	else if (GTK_IS_CONTAINER (widget))
		gtk_container_forall (GTK_CONTAINER (widget), traverse_container,
				      data);
}
#endif


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

#if 0
	gtk_container_forall (GTK_CONTAINER (node), traverse_container,
			 NULL);
#endif
#if 0
	gtk_widget_destroy(node);





	guint n_props = 0, i;
	GParamSpec **props;
	widget = GTK_WIDGET(n);
	g_print("widget %s has\n", G_OBJECT_TYPE_NAME(widget));
	props =	g_object_class_list_properties (G_OBJECT_GET_CLASS (widget), &n_props);
	for (i = 0; i < n_props; i++) {
		const gchar *name = g_param_spec_get_name (props[i]);
		g_print("\t %s\n", name);
	}



	gtk_builder_add_from_file(builder, "test.glade", NULL);
	n = gtk_builder_get_object(builder, "filt");
	g_print("ret: %p", n);
	gtk_container_add(GTK_CONTAINER(p->cfg->nodes.node_view), GTK_WIDGET(n));

#endif
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
	gtk_nodes_node_view_save (GTKNODES_NODE_VIEW(p->cfg->nodes.node_view));
}


static void obs_assist_node_load_cb(GtkWidget *widget, ObsAssist *p)
{
	  	/* apparently I have to load the once */
	GtkWidget *w;

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


	gtk_nodes_node_view_load(GTKNODES_NODE_VIEW (p->cfg->nodes.node_view));
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
