/**
 * @file    widgets/nodes/node_coordinates.c
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
 * @brief a GtkNode to convert and pair-unpair coordinates, only one value per
 *	  call is considered, i.e. array inputs are ignored except for the first
 *	  element
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>
#include <nodes.h>

#include <cmd.h>
#include <signals.h>
#include <coordinates.h>

struct _CoordinatesPrivate {
	int var;
};

G_DEFINE_TYPE_WITH_PRIVATE(Coordinates, coordinates, GTKNODES_TYPE_NODE)


struct coordinates_config {

	GtkWidget *i_c1;
	GtkWidget *i_c2;
	GtkWidget *i_cx;
	GtkWidget *i_tr;

	GtkWidget *o_c1;
	GtkWidget *o_c2;
	GtkWidget *o_cx;

	GtkWidget *ic1_lbl;
	GtkWidget *ic2_lbl;
	GtkWidget *oc1_lbl;
	GtkWidget *oc2_lbl;

	gboolean trigger;

	GByteArray *payload_c1;
	GByteArray *payload_c2;
	GByteArray *payload_cx;


	struct nodes_coordinate coord_in;	/* combined inputs */
	struct nodes_coordinate *coord_out;	/* complex output */
	gdouble *c1;				/* individiual outputs */
	gdouble *c2;

	gdouble lat;
	gdouble lon;

	guint id_con;
	guint id_cap;
};



static void node_coordinates_convert(struct coordinates_config *cfg)
{
	struct coord_horizontal hor;
	struct coord_equatorial equ;
	struct coord_galactic   gal;


	switch (cfg->coord_in.coord_type) {
	case HOR:
		hor.az = cfg->coord_in.c1;
		hor.el = cfg->coord_in.c2;
		break;
	case EQU:
		equ.ra  = cfg->coord_in.c1;
		equ.dec = cfg->coord_in.c2;
		hor = equatorial_to_horizontal(equ, cfg->lat, cfg->lon, 0.0);
		break;
	case GAL:
		gal.lat  = cfg->coord_in.c1;
		gal.lon = cfg->coord_in.c2;
		hor = galactic_to_horizontal(gal, cfg->lat, cfg->lon, 0.0);
		break;
	default:
		return;
	}

	switch (cfg->coord_out->coord_type) {
	case HOR:
		(*cfg->c1) = cfg->coord_out->c1 = hor.az;
		(*cfg->c2) = cfg->coord_out->c2 = hor.el;
		break;
	case EQU:
		equ = horizontal_to_equatorial(hor, cfg->lat, cfg->lon, 0.0);
		(*cfg->c1) = cfg->coord_out->c1 = equ.ra;
		(*cfg->c2) = cfg->coord_out->c2 = equ.dec;
		break;
	case GAL:
		gal = horizontal_to_galactic(hor, cfg->lat, cfg->lon);
		(*cfg->c1) = cfg->coord_out->c1 = gal.lat;
		(*cfg->c2) = cfg->coord_out->c2 = gal.lon;
		break;
	default:
		return;
	}
}

static void node_coordinates_output(struct coordinates_config *cfg)
{
	node_coordinates_convert(cfg);

	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->o_c1),
				    cfg->payload_c1);

	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->o_c2),
				    cfg->payload_c2);

	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->o_cx),
				    cfg->payload_cx);
}


static void node_coord_input_changed(GtkComboBox *cb,
				     struct coordinates_config *cfg)
{

	switch (gtk_combo_box_get_active(cb)) {
	case 0:
		gtk_label_set_text(GTK_LABEL(cfg->ic1_lbl), "Azimuth");
		gtk_label_set_text(GTK_LABEL(cfg->ic2_lbl), "Elevation");
		cfg->coord_in.coord_type = HOR;
		break;
	case 1:
		gtk_label_set_text(GTK_LABEL(cfg->ic1_lbl), "Right Ascension");
		gtk_label_set_text(GTK_LABEL(cfg->ic2_lbl), "Declination");
		cfg->coord_in.coord_type = EQU;
		break;
	case 2:
		gtk_label_set_text(GTK_LABEL(cfg->ic1_lbl), "Galactic Latitude");
		gtk_label_set_text(GTK_LABEL(cfg->ic2_lbl), "Galactic Longitude");
		cfg->coord_in.coord_type = GAL;
		break;
	default:
		break;
	}
}

static void node_coord_output_changed(GtkComboBox *cb,
				      struct coordinates_config *cfg)
{

	switch (gtk_combo_box_get_active(cb)) {
	case 0:
		gtk_label_set_text(GTK_LABEL(cfg->oc1_lbl), "Azimuth");
		gtk_label_set_text(GTK_LABEL(cfg->oc2_lbl), "Elevation");
		cfg->coord_out->coord_type = HOR;
		break;
	case 1:
		gtk_label_set_text(GTK_LABEL(cfg->oc1_lbl), "Right Ascension");
		gtk_label_set_text(GTK_LABEL(cfg->oc2_lbl), "Declination");
		cfg->coord_out->coord_type = EQU;
		break;
	case 2:
		gtk_label_set_text(GTK_LABEL(cfg->oc1_lbl), "Galactic Latitude");
		gtk_label_set_text(GTK_LABEL(cfg->oc2_lbl), "Galactic Longitude");
		cfg->coord_out->coord_type = GAL;
		break;
	default:
		break;
	}
}

static void node_coordinates_input_1(GtkWidget *widget,
				     GByteArray *payload,
				     struct coordinates_config *cfg)
{
	if (!payload)
		return;

	if (payload->len < sizeof(struct nodes_coordinate))
		return;

	cfg->coord_in = ((struct nodes_coordinate *)payload->data)[0];

	if (!cfg->trigger)
		node_coordinates_output(cfg);
}

static void node_coordinates_input_2(GtkWidget *widget,
				     GByteArray *payload,
				     struct coordinates_config *cfg)
{
	if (!payload)
		return;

	if (payload->len < sizeof(gdouble))
		return;

	cfg->coord_in.c1 = ((gdouble *)payload->data)[0];

	if (!cfg->trigger)
		node_coordinates_output(cfg);
}

static void node_coordinates_input_3(GtkWidget *widget,
				     GByteArray *payload,
				     struct coordinates_config *cfg)
{
	if (!payload)
		return;

	if (payload->len < sizeof(gdouble))
		return;

	cfg->coord_in.c2 = ((gdouble *)payload->data)[0];

	if (!cfg->trigger)
		node_coordinates_output(cfg);
}

static void node_coordinates_trigger(GtkWidget *widget,
				     GByteArray *payload,
				     struct coordinates_config *cfg)
{
	node_coordinates_output(cfg);
}

static void node_coordinates_output_connected(GtkWidget *widget,
					      GtkWidget *source,
					      struct coordinates_config *cfg)
{
	if (!cfg->trigger)
		node_coordinates_output(cfg);
}


static gboolean node_coordinate_toggle_immediate(GtkWidget *w,
						 gboolean state,
						 struct coordinates_config *cfg)
{
	if (gtk_switch_get_active(GTK_SWITCH(w)))
		cfg->trigger = FALSE;
	else
		cfg->trigger = TRUE;

	return FALSE;
}

static void node_coordinates_handle_pr_capabilities(gpointer instance,
						    const struct capabilities *c,
						    struct coordinates_config *cfg)
{
	cfg->lat = (gdouble) c->lat_arcsec / 3600.0;
	cfg->lon = (gdouble) c->lon_arcsec / 3600.0;
}

static void node_coordinates_connected(gpointer instance,
				       struct coordinates_config *cfg)
{
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
}


static void node_coordinates_remove(GtkWidget *w, struct coordinates_config *cfg)
{
	if (cfg->id_cap) {
		g_signal_handler_disconnect(sig_get_instance(), cfg->id_cap);
		cfg->id_cap = 0;
	}

	if (cfg->id_con) {
		g_signal_handler_disconnect(sig_get_instance(), cfg->id_con);
		cfg->id_con = 0;
	}

	if (cfg->payload_c1) {
		g_byte_array_free (cfg->payload_c1, TRUE);
		cfg->payload_c1 = NULL;
	}

	if (cfg->payload_c2) {
		g_byte_array_free (cfg->payload_c2, TRUE);
		cfg->payload_c2 = NULL;
	}

	if (cfg->payload_cx) {
		g_byte_array_free (cfg->payload_cx, TRUE);
		cfg->payload_cx = NULL;
	}

	gtk_widget_destroy(w);
	g_free(cfg);
}


static void coordinates_class_init(CoordinatesClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}

static void coordinates_init(Coordinates *node)
{
	GtkWidget *w;
	GtkWidget *grid;

	struct coordinates_config *cfg;


	cfg = g_malloc0(sizeof(struct coordinates_config));

	cfg->trigger   = TRUE;

	cfg->c1        = g_malloc0(sizeof(double));
	cfg->c2        = g_malloc0(sizeof(double));
	cfg->coord_out = g_malloc0(sizeof(struct nodes_coordinate));

	/* our outputs Garrays are constant size, create them here */
	cfg->payload_c1 = g_byte_array_new_take((void *) cfg->c1, sizeof(double));
	cfg->payload_c2 = g_byte_array_new_take((void *) cfg->c2, sizeof(double));
	cfg->payload_cx = g_byte_array_new_take((void *) cfg->coord_out,
						sizeof(struct nodes_coordinate));

	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_coordinates_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Coordinates");

	/* input sockets */

	w = gtk_label_new("Coordinate");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->i_cx = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					    GTKNODES_NODE_SOCKET_SINK);
	g_signal_connect(G_OBJECT(cfg->i_cx), "socket-incoming",
			 G_CALLBACK(node_coordinates_input_1), cfg);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->i_cx),
				       &COL_COORDINATES);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->i_cx),
				      KEY_COORDINATES);

	cfg->ic1_lbl = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(cfg->ic1_lbl), 0.0);
	cfg->i_c1 = gtk_nodes_node_add_item(GTKNODES_NODE(node), cfg->ic1_lbl,
					    GTKNODES_NODE_SOCKET_SINK);
	g_signal_connect(G_OBJECT(cfg->i_c1), "socket-incoming",
			 G_CALLBACK(node_coordinates_input_2), cfg);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->i_c1),
				       &COL_DOUBLE);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->i_c1),
				      KEY_DOUBLE);

	cfg->ic2_lbl = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(cfg->ic2_lbl), 0.0);
	cfg->i_c2 = gtk_nodes_node_add_item(GTKNODES_NODE(node), cfg->ic2_lbl,
					    GTKNODES_NODE_SOCKET_SINK);
	g_signal_connect(G_OBJECT(cfg->i_c2), "socket-incoming",
			 G_CALLBACK(node_coordinates_input_3), cfg);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->i_c2),
				       &COL_DOUBLE);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->i_c2),
				      KEY_DOUBLE);




	/* grid containing user controls */
	grid = gtk_grid_new();
	g_object_set(grid, "margin", 6, NULL);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_nodes_node_add_item(GTKNODES_NODE(node), grid,
				GTKNODES_NODE_SOCKET_DISABLE);




	/* outputs */
	cfg->oc1_lbl = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(cfg->oc1_lbl), 1.0);
	cfg->o_c1 = gtk_nodes_node_add_item(GTKNODES_NODE(node), cfg->oc1_lbl,
					    GTKNODES_NODE_SOCKET_SOURCE);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->o_c1),
				       &COL_DOUBLE);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->o_c1),
				      KEY_DOUBLE);
	g_signal_connect(G_OBJECT(cfg->o_c1), "socket-connect",
			 G_CALLBACK(node_coordinates_output_connected), cfg);

	cfg->oc2_lbl = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(cfg->oc2_lbl), 1.0);
	cfg->o_c2 = gtk_nodes_node_add_item(GTKNODES_NODE(node), cfg->oc2_lbl,
					    GTKNODES_NODE_SOCKET_SOURCE);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->o_c2),
				       &COL_DOUBLE);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->o_c2),
				      KEY_DOUBLE);
	g_signal_connect(G_OBJECT(cfg->o_c1), "socket-connect",
			 G_CALLBACK(node_coordinates_output_connected), cfg);


	w = gtk_label_new("Coordinate");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->o_cx = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					    GTKNODES_NODE_SOCKET_SOURCE);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->o_cx),
				       &COL_COORDINATES);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->o_cx),
				      KEY_COORDINATES);
	g_signal_connect(G_OBJECT(cfg->o_cx), "socket-connect",
			 G_CALLBACK(node_coordinates_output_connected), cfg);



	/* user controls requiring sources already created */
	w = gtk_label_new("Input");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);
	/* style selector */
	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "AZ-EL");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "RA-DEC");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "GLAT-GLON");
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(node_coord_input_changed), cfg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);	/* default AZEL */

	w = gtk_label_new("Output");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);
	/* style selector */
	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "AZ-EL");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "RA-DEC");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "GLAT-GLON");
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(node_coord_output_changed), cfg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);	/* default AZEL */

	w = gtk_label_new("Immediate");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable immediate or triggered output\n");
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(node_coordinate_toggle_immediate), cfg);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);



	/* trigger input */
	w = gtk_label_new("Trigger");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->i_tr = gtk_nodes_node_add_item(GTKNODES_NODE(node),w,
					    GTKNODES_NODE_SOCKET_SINK);
	g_signal_connect(G_OBJECT(cfg->i_tr), "socket-incoming",
			 G_CALLBACK(node_coordinates_trigger), cfg);



	/* connect external data sources */
	cfg->id_con = g_signal_connect(sig_get_instance(), "net-connected",
				       G_CALLBACK(node_coordinates_connected),
				       (gpointer) cfg);
	cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
				       G_CALLBACK(node_coordinates_handle_pr_capabilities),
				       (gpointer) cfg);

	/* request lat/lon update */
	cmd_capabilities(PKT_TRANS_ID_UNDEF);

	gtk_widget_show_all(grid);
}


GtkWidget *coordinates_new(void)
{
	Coordinates *coordinates;


	coordinates = g_object_new(TYPE_COORDINATES, NULL);

	return GTK_WIDGET(coordinates);
}
