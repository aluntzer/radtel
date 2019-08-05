/**
 * @file    widgets/nodes/node_step.c
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
 * @brief a GtkNode stepping through a configurable interval
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>
#include <nodes.h>


struct _StepPrivate {
	int var;
};

G_DEFINE_TYPE_WITH_PRIVATE(Step, step, GTKNODES_TYPE_NODE)


/* configurable pulse range */
#define STEP_INTERVAL_MIN	-1000.0
#define STEP_INTERVAL_MAX	 1000.0
#define STEP_INTERVAL_STP	 0.01

#define STEP_BLINK_TIMEOUT_MS	100

struct step_config {

	GtkWidget *rst;
	GtkWidget *trg_i;
	GtkWidget *trg_o;
	GtkWidget *dat_o;
	GtkWidget *bar;

	GByteArray *payload;

	gdouble *cur;
	gdouble min;
	gdouble max;
	gdouble stp;

	GdkRGBA rgba_trg_o;

	guint id_trg;
	guint id_out;
};


static gboolean node_step_deactivate_timeout_trg_out (gpointer data)
{
	struct step_config *cfg;


	cfg = (struct step_config *) data;

	cfg->id_trg = 0;
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->trg_o),
				       &cfg->rgba_trg_o);

	return G_SOURCE_REMOVE;
}

static gboolean node_step_deactivate_timeout_dat_out (gpointer data)
{
	struct step_config *cfg;


	cfg = (struct step_config *) data;

	cfg->id_out = 0;
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->dat_o),
				       &COL_DOUBLE);

	return G_SOURCE_REMOVE;
}


static void node_step_blink_data_out(struct step_config *cfg)
{
	if (cfg->id_out)
		return;

	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->dat_o),
				       &COL_BLINK);

	cfg->id_out = g_timeout_add(STEP_BLINK_TIMEOUT_MS,
				    node_step_deactivate_timeout_dat_out,
				    cfg);
}

static void node_step_blink_trg_out(struct step_config *cfg)
{
	if (cfg->id_trg)
		return;

	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->trg_o),
				       &COL_BLINK);

	cfg->id_out = g_timeout_add(STEP_BLINK_TIMEOUT_MS,
				    node_step_deactivate_timeout_trg_out,
				    cfg);
}

static void node_step_field_changed(GtkWidget *w, gdouble *val)
{
	(*val) = gtk_spin_button_get_value(GTK_SPIN_BUTTON(w));
}

static void node_progress_bar_update(struct step_config *cfg)
{
	gdouble frac;
	gchar *buf;

	frac = ((* cfg->cur) - cfg->min) / (cfg->max - cfg->min);

	buf = g_strdup_printf("%6.2f of [%g : %g]", (* cfg->cur),
			      cfg->min, cfg->max);

	if (frac >= 0.0) {
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cfg->bar), frac);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(cfg->bar), buf);
	}
	g_free(buf);
}

static void node_step_output(struct step_config *cfg)
{
	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->dat_o),
				    cfg->payload);

	node_step_blink_data_out(cfg);
}


static void node_step_trigger(GtkWidget *widget,
			      GByteArray *payload,
			      struct step_config *cfg)
{
	if (cfg->min < cfg->max) {
		if ((* cfg->cur) < cfg->max)
			(* cfg->cur) += cfg->stp;
	} else {
		if ((* cfg->cur) > cfg->max)
			(* cfg->cur) += cfg->stp;
	}

	node_progress_bar_update(cfg);
	node_step_output(cfg);

	if (cfg->min < cfg->max) {
		if ((* cfg->cur) < cfg->max)
			return;
	} else {
		if ((* cfg->cur) > cfg->max)
			return;
	}

	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->trg_o),
				    cfg->payload);
	node_step_blink_trg_out(cfg);
}

static void node_step_reset(GtkWidget *widget,
			    GByteArray *payload,
			    struct step_config *cfg)
{

	(* cfg->cur) = cfg->min;

	node_progress_bar_update(cfg);
}

static void node_step_data_connected(GtkWidget *widget,
				     GtkWidget *source,
				     struct step_config *cfg)
{
	/* push last value */
	node_step_output(cfg);
}


void node_step_clicked(GtkWidget *button,  struct step_config *cfg)
{
	node_step_trigger(NULL, NULL, cfg);
}
void node_reset_clicked(GtkWidget *button,  struct step_config *cfg)
{
	node_step_reset(NULL, NULL, cfg);
}

static void node_step_remove(GtkWidget *w, struct step_config *cfg)
{
	if (cfg->id_trg) {
		g_source_remove(cfg->id_trg);
		cfg->id_trg = 0;
	}

	gtk_widget_destroy(w);

	if (cfg->payload) {
		g_byte_array_free (cfg->payload, TRUE);
		cfg->payload = NULL;
	}

	g_free(cfg);
}

static void step_class_init(StepClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}

static void step_init(Step *node)
{
	GtkWidget *w;
	GtkWidget *grid;

	struct step_config *cfg;


	cfg = g_malloc0(sizeof(struct step_config));


	/* our output payload is one double, allocate it here */
	cfg->cur = g_malloc0(sizeof(double));
	cfg->payload = g_byte_array_new_take((void *) cfg->cur, sizeof(double));

	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_step_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Range Stepper");


	/* input sockets */
	w = gtk_label_new("Trigger");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->trg_i = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					     GTKNODES_NODE_SOCKET_SINK);
	g_signal_connect(G_OBJECT(cfg->trg_i), "socket-incoming",
			 G_CALLBACK(node_step_trigger), cfg);

	w = gtk_label_new("Reset");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->rst = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					   GTKNODES_NODE_SOCKET_SINK);
	g_signal_connect(G_OBJECT(cfg->rst), "socket-incoming",
			 G_CALLBACK(node_step_reset), cfg);



	/* grid containing user controls */

	grid = gtk_grid_new();
	g_object_set(grid, "margin", 6, NULL);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

	gtk_nodes_node_add_item(GTKNODES_NODE(node), grid,
				GTKNODES_NODE_SOCKET_DISABLE);



	/* output sockets */
	w = gtk_label_new("Output");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->dat_o = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					     GTKNODES_NODE_SOCKET_SOURCE);
	gtk_box_set_child_packing(GTK_BOX(node), w, FALSE, FALSE, 0,
				  GTK_PACK_END);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->dat_o),
				       &COL_DOUBLE);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->dat_o),
				      KEY_DOUBLE);
	g_signal_connect(G_OBJECT(cfg->dat_o), "socket-connect",
			 G_CALLBACK(node_step_data_connected), cfg);


	w = gtk_label_new("Last");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->trg_o = gtk_nodes_node_add_item(GTKNODES_NODE(node), w,
					     GTKNODES_NODE_SOCKET_SOURCE);
	gtk_box_set_child_packing(GTK_BOX(node), w, FALSE, FALSE, 0,
				  GTK_PACK_END);
	/* get original colour */
	gtk_nodes_node_socket_get_rgba(GTKNODES_NODE_SOCKET(cfg->trg_o),
				       &cfg->rgba_trg_o);




	/* create main controls, they need the socket reference */
	w = gtk_label_new("START");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);
	w = gtk_spin_button_new_with_range(STEP_INTERVAL_MIN,
					   STEP_INTERVAL_MAX,
					   STEP_INTERVAL_STP);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(node_step_field_changed), &cfg->min);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 0.);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);


	w = gtk_label_new("STOP");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);
	w = gtk_spin_button_new_with_range(STEP_INTERVAL_MIN,
					   STEP_INTERVAL_MAX,
					   STEP_INTERVAL_STP);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(node_step_field_changed), &cfg->max);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 360.);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);

	w = gtk_label_new("STEP");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);
	w = gtk_spin_button_new_with_range(STEP_INTERVAL_MIN,
					   STEP_INTERVAL_MAX,
					   STEP_INTERVAL_STP);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(node_step_field_changed), &cfg->stp);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 0.5);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);

	cfg->bar = gtk_progress_bar_new();
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(cfg->bar), TRUE);
	gtk_progress_bar_set_ellipsize(GTK_PROGRESS_BAR(cfg->bar), TRUE);
	gtk_grid_attach(GTK_GRID(grid), cfg->bar, 0, 3, 2, 1);

	w = gtk_button_new_with_label("Step");
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(node_step_clicked), cfg);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 4, 1, 1);

	w = gtk_button_new_with_label("Reset");
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(node_reset_clicked), cfg);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 5, 1, 1);

	gtk_widget_show_all(grid);
}


GtkWidget *step_new(void)
{
	Step *step;


	step = g_object_new(TYPE_STEP, NULL);

	return GTK_WIDGET(step);
}
