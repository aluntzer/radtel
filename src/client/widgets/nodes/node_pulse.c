/**
 * @file    widgets/nodes/node_pulse.c
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
 * @brief a GtkNode emitting a configurable periodic or one-shot pulse
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>
#include <nodes.h>

struct _PulsePrivate {
	int var;
};

G_DEFINE_TYPE_WITH_PRIVATE(Pulse, pulse, GTKNODES_TYPE_NODE)


/* configurable pulse range */
#define PULSE_INTERVAL_MIN_MS	0
#define PULSE_INTERVAL_MAX_MS	100000
#define PULSE_INTERVAL_STP_MS	1

/* colour toggle and continuous ON limit */
#define PULSE_BLINK_TIMEOUT_MS	50
#define PULSE_BLINK_LIMIT_MS	(PULSE_BLINK_TIMEOUT_MS * 2)



struct pulse_config {

	GtkWidget  *output;
	GByteArray *payload;

	guint interval_ms;

	GdkRGBA rgba;

	gboolean pulse;
	guint id_to;
	guint id_col;
};


static gboolean node_pulse_deactivate_timeout (gpointer data)
{
	struct pulse_config *cfg;


	cfg = (struct pulse_config *) data;

	cfg->id_col = 0;
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &cfg->rgba);

	return G_SOURCE_REMOVE;
}


static void node_pulse_emit(struct pulse_config *cfg)
{
	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->output),
				    cfg->payload);

	/* do not set if interval is faster than our reset timeout, or periodic
	 * pulses; in this case, the colour has been set by the installer of the
	 * timeout
	 */

	if (cfg->id_to)
		if (cfg->interval_ms <= PULSE_BLINK_LIMIT_MS)
			return;

	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_BLINK);

	cfg->id_col = g_timeout_add(PULSE_BLINK_TIMEOUT_MS,
				    node_pulse_deactivate_timeout, cfg);
}


static gboolean node_pulse_timeout_cb(void *data)
{
	struct pulse_config *cfg;


	cfg = (struct pulse_config *) data;

	node_pulse_emit (cfg);

	return cfg->pulse;
}


void node_pulse_clicked(GtkWidget *button,  struct pulse_config *cfg)
{
	node_pulse_emit (cfg);
}


static gboolean node_pulse_toggle_periodic(GtkWidget *w,
					   gboolean state,
					   struct pulse_config *cfg)
{
	GtkNodesNodeSocket *socket;

	const GSourceFunc sf = node_pulse_timeout_cb;


	socket = GTKNODES_NODE_SOCKET(cfg->output);

	if (gtk_switch_get_active(GTK_SWITCH(w)) && !cfg->id_to) {

		if (cfg->pulse)
			return TRUE;

		cfg->pulse = G_SOURCE_CONTINUE;
		cfg->id_to = g_timeout_add(cfg->interval_ms, sf, cfg);

		/* set permanently "active" for very fast timeout */
		if (cfg->interval_ms <= PULSE_BLINK_LIMIT_MS)
			gtk_nodes_node_socket_set_rgba(socket, &COL_BLINK);
	} else {
		if (cfg->id_to) {
			g_source_remove(cfg->id_to);
			cfg->id_to = 0;
			gtk_nodes_node_socket_set_rgba(socket, &cfg->rgba);
		}
		cfg->pulse = G_SOURCE_REMOVE;
	}

	return FALSE;
}


static void node_pulse_timeout_changed(GtkWidget *w,
				       struct pulse_config *cfg)
{
	GtkNodesNodeSocket *socket;

	const GSourceFunc sf = node_pulse_timeout_cb;


	socket = GTKNODES_NODE_SOCKET(cfg->output);

	cfg->interval_ms = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

	if (!cfg->id_to)
		return;

	g_source_remove(cfg->id_to);
	cfg->id_to = g_timeout_add(cfg->interval_ms, sf, cfg);

	/* set permanently "active" for very fast timeout */
	if (cfg->interval_ms <= PULSE_BLINK_LIMIT_MS)
		gtk_nodes_node_socket_set_rgba(socket, &COL_BLINK);
}

static void node_pulse_remove(GtkWidget *w, struct pulse_config *cfg)
{
	if (cfg->id_to) {
		g_source_remove(cfg->id_to);
		cfg->id_to = 0;
	}

	if (cfg->id_col) {
		g_source_remove(cfg->id_col);
		cfg->id_col = 0;
	}

	gtk_widget_destroy(w);

	if (cfg->payload) {
		g_byte_array_free (cfg->payload, TRUE);
		cfg->payload = NULL;
	}

	g_free(cfg);
}


static void pulse_class_init(PulseClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}

static void pulse_init(Pulse *node)
{
	GtkWidget *w;
	GtkWidget *grid;

	struct pulse_config *cfg;

	const gchar *msg = "PULSE";


	cfg = g_malloc0(sizeof(struct pulse_config));


	/* our output payload is constant, allocate it here */
	cfg->payload = g_byte_array_new();
	g_byte_array_append (cfg->payload, msg, strlen(msg) + 1);

	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_pulse_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Pulse Generator");


	/* grid containing user controls */

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

	gtk_nodes_node_item_add(GTKNODES_NODE(node), grid,
				GTKNODES_NODE_SOCKET_DISABLE);

	/* output socket */
	w = gtk_label_new("Output");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->output = gtk_nodes_node_item_add(GTKNODES_NODE(node), w,
					      GTKNODES_NODE_SOCKET_SOURCE);
	gtk_box_set_child_packing(GTK_BOX(node), w, FALSE, FALSE, 0,
				  GTK_PACK_END);
	/* get original colour */
	gtk_nodes_node_socket_get_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &cfg->rgba);


	/* create main controls, they need the socket reference */
	w = gtk_label_new("Continuous");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_switch_new();
	gtk_widget_set_tooltip_text(w, "Enable/Disable continuous output\n");
	g_signal_connect(G_OBJECT(w), "state-set",
			 G_CALLBACK(node_pulse_toggle_periodic), cfg);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);


	w = gtk_label_new("Interval [ms]");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);
	w = gtk_spin_button_new_with_range(PULSE_INTERVAL_MIN_MS,
					   PULSE_INTERVAL_MAX_MS,
					   PULSE_INTERVAL_STP_MS);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(node_pulse_timeout_changed), cfg);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 500);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);

	w = gtk_button_new_with_label("Single");
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(node_pulse_clicked), cfg);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

	gtk_widget_show_all(grid);
}


GtkWidget *pulse_new(void)
{
	Pulse *pulse;


	pulse = g_object_new(TYPE_PULSE, NULL);

	return GTK_WIDGET(pulse);
}
