/**
 * @file    widgets/nodes/node_plot.c
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
 * @brief a GtkNode for plotting data
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>
#include <nodes.h>

#include <cmd.h>
#include <signals.h>
#include <xyplot.h>

struct _PlotPrivate {
	int var;
};

G_DEFINE_TYPE_WITH_PRIVATE(Plot, plot, GTKNODES_TYPE_NODE)


struct plot_config {
	GtkWidget *clear;
	GtkWidget *data;
	GtkWidget *plot;

	GdkRGBA rgba_graph;

	enum xyplot_graph_style style;

	guint id_data;
	guint id_clear;
};


static void node_plot_set_style(gint active, enum xyplot_graph_style *s)
{
	switch (active) {
	case 0:
		(*s) = STAIRS;
		break;
	case 1:
		(*s) = LINES;
		break;
	case 2:
		(*s) = DASHES;
		break;
	case 3:
		(*s) = CURVES;
		break;
	case 4:
		(*s) = CIRCLES;
		break;
	case 5:
		(*s) = SQUARES;
		break;
	case 6:
		(*s) = IMPULSES;
		break;
	case 7:
		(*s) = MARIO;
		break;
	default:
		break;
	}
}

static void node_plot_data_style_changed(GtkComboBox *cb,
					 struct plot_config *cfg)
{
	node_plot_set_style(gtk_combo_box_get_active(cb), &cfg->style);
}


static void node_plot_colour_changed(GtkColorButton *w,
				     struct plot_config *cfg)
{
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &cfg->rgba_graph);
}



static void node_plot_clear(GtkWidget *widget,
			    GByteArray *payload,
			    struct plot_config *cfg)
{
	xyplot_drop_all_graphs(cfg->plot);
	xyplot_redraw(cfg->plot);
}

static void node_plot_data(GtkWidget *widget,
			   GByteArray *payload,
			   struct plot_config *cfg)
{
	gsize i;
	gsize len;

	gdouble *x;
	gdouble *y;

	void *ref;



	struct nodes_point *spec;

	if (!payload) {
		g_warning("empty payload!");
		return;
	}

	len = payload->len / sizeof(struct nodes_point);
	x = g_malloc(len * sizeof(gdouble));
	y = g_malloc(len * sizeof(gdouble));


	spec = (struct nodes_point *) payload->data;

	for (i = 0; i < len; i++) {
		x[i] = spec[i].p0;
		y[i] = spec[i].p1;
	}

	ref = xyplot_add_graph(cfg->plot, x, y, NULL, len, g_strdup_printf("Graph"));
	xyplot_set_graph_style(cfg->plot, ref, cfg->style);
	xyplot_set_graph_rgba(cfg->plot, ref, cfg->rgba_graph);
	xyplot_redraw(cfg->plot);
}


void node_plot_clear_cb(GtkWidget *button, struct plot_config *cfg)
{
	node_plot_clear(NULL, NULL, cfg);
}

static void node_plot_remove(GtkWidget *w, struct plot_config *cfg)
{
	if (cfg->id_data) {
		g_source_remove(cfg->id_data);
		cfg->id_data = 0;
	}

	if (cfg->id_clear) {
		g_source_remove(cfg->id_clear);
		cfg->id_clear = 0;
	}

	gtk_widget_destroy(w);
	g_free(cfg);
}


static void plot_class_init(PlotClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}

static void plot_init(Plot *node)
{
	GtkWidget *w;
	GtkWidget *grid;

	struct plot_config *cfg;


	cfg = g_malloc0(sizeof(struct plot_config));

	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_plot_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Plot");

	/* input socket */

	w = gtk_label_new("Graph");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->data = gtk_nodes_node_item_add(GTKNODES_NODE(node), w,
					    GTKNODES_NODE_SOCKET_SINK);
	cfg->id_data = g_signal_connect(G_OBJECT(cfg->data), "socket-incoming",
					G_CALLBACK(node_plot_data), cfg);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->data),
				       &COL_POINTS);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->data),
				      KEY_POINTS);

	w = gtk_label_new("Clear");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->clear = gtk_nodes_node_item_add(GTKNODES_NODE(node), w,
					     GTKNODES_NODE_SOCKET_SINK);
	cfg->id_clear = g_signal_connect(G_OBJECT(cfg->clear), "socket-incoming",
					 G_CALLBACK(node_plot_clear), cfg);


	/* spectrum display */
	cfg->plot = xyplot_new();
	gtk_widget_set_size_request(cfg->plot, 250, 250);
	gtk_nodes_node_item_add(GTKNODES_NODE(node), cfg->plot,
				GTKNODES_NODE_SOCKET_DISABLE);
	gtk_box_set_child_packing(GTK_BOX(node), cfg->plot, TRUE, TRUE, 0,
				  GTK_PACK_START);


	/* grid containing user controls */
	grid = gtk_grid_new();
	g_object_set(grid, "margin", 6, NULL);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

	gtk_nodes_node_item_add(GTKNODES_NODE(node), grid,
				GTKNODES_NODE_SOCKET_DISABLE);


	w = gtk_button_new_with_label("Clear");
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(node_plot_clear_cb), cfg);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	cfg->rgba_graph = COLOR_YELLOW_PHOS;
	w = gtk_color_button_new_with_rgba(&cfg->rgba_graph);
	gtk_color_chooser_set_use_alpha(GTK_COLOR_CHOOSER(w), TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);
	g_signal_connect(w, "color-set",
			 G_CALLBACK(node_plot_colour_changed), cfg);

	/* style selector */
	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "HiStep");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Dashed Line");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Bézier");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Circle");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Square");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Impulses");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), NULL, "Mario");
	gtk_grid_attach(GTK_GRID(grid), w, 2, 0, 1, 1);
	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(node_plot_data_style_changed), cfg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 4);	/* default circles */


	gtk_widget_show_all(grid);
}


GtkWidget *plot_new(void)
{
	Plot *plot;

	plot = g_object_new(TYPE_PLOT, NULL);

	return GTK_WIDGET(plot);
}
