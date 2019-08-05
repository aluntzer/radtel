/**
 * @file    widgets/nodes/node_target.c
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
 * @brief a GtkNode emitting spectral data received from the server
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>
#include <nodes.h>

#include <cmd.h>
#include <signals.h>
#include <coordinates.h>


struct _TargetPrivate {
	int var;
};

G_DEFINE_TYPE_WITH_PRIVATE(Target, target, GTKNODES_TYPE_NODE)

struct target_config {
	GtkWidget *input;
	GtkWidget *lbl;

	gdouble lat;
	gdouble lon;

	guint id_con;
	guint id_cap;
};


static void node_target_update(struct nodes_coordinate *coord,
			       struct target_config *cfg)


{
	struct coord_horizontal hor;
	struct coord_equatorial equ;
	struct coord_galactic   gal;


	switch (coord->coord_type) {
	case HOR:
		hor.az = coord->c1;
		hor.el = coord->c2;
		break;
	case EQU:
		equ.ra  = coord->c1;
		equ.dec = coord->c2;
		hor = equatorial_to_horizontal(equ, cfg->lat, cfg->lon, 0.0);
		break;
	case GAL:
		gal.lat = coord->c1;
		gal.lon = coord->c2;
		hor = galactic_to_horizontal(gal, cfg->lat, cfg->lon, 0.0);
		break;
	default:
		return;
	}

	cmd_moveto_azel(PKT_TRANS_ID_UNDEF, hor.az, hor.el);
}


static void node_target_incoming(GtkWidget *widget,
				 GByteArray *payload,
				 struct target_config *cfg)
{
	struct nodes_coordinate *coord;

	if (!payload)
		return;

	if (!payload->len)
		return;

	if (payload->len < sizeof(struct nodes_coordinate))
		return;

	/* we accept only the first coordinate for each call, as in principle
	 * we could have received an array
	 */

	coord = (struct nodes_coordinate *) payload->data;


	node_target_update(coord, cfg);
}

static void node_target_handle_pr_capabilities(gpointer instance,
						    const struct capabilities *c,
						    struct target_config *cfg)
{
	cfg->lat = (gdouble) c->lat_arcsec / 3600.0;
	cfg->lon = (gdouble) c->lon_arcsec / 3600.0;
}

static void node_target_connected(gpointer instance,
				       struct target_config *cfg)
{
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
}


static void node_target_remove(GtkWidget *w, struct target_config *cfg)
{
	if (cfg->id_cap) {
		g_signal_handler_disconnect(sig_get_instance(), cfg->id_cap);
		cfg->id_cap = 0;
	}

	if (cfg->id_con) {
		g_signal_handler_disconnect(sig_get_instance(), cfg->id_con);
		cfg->id_con = 0;
	}

	gtk_widget_destroy(w);
	g_free(cfg);
}


static void target_class_init(TargetClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}

static void target_init(Target *node)
{
	GtkWidget *w;

	struct target_config *cfg;


	cfg = g_malloc0(sizeof(struct target_config));

	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_target_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Sky Target");

	/* input socket */
	w = gtk_label_new("Coordinates");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->input = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					     GTKNODES_NODE_SOCKET_SINK);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->input),
				       &COL_COORDINATES);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->input),
				      KEY_COORDINATES);
	g_signal_connect(G_OBJECT(cfg->input), "socket-incoming",
			 G_CALLBACK(node_target_incoming), cfg);


	/* connect external data sources */
	cfg->id_con = g_signal_connect(sig_get_instance(), "net-connected",
				       G_CALLBACK(node_target_connected),
				       (gpointer) cfg);
	cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
				       G_CALLBACK(node_target_handle_pr_capabilities),
				       (gpointer) cfg);

	/* request lat/lon update */
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
}


GtkWidget *target_new(void)
{
	Medfilt *target;


	target = g_object_new(TYPE_TARGET, NULL);

	return GTK_WIDGET(target);
}
