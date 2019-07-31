/**
 * @file    widgets/nodes/node_spec_src.c
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
#include <xyplot.h>


#define SPEC_SRC_BLINK_TIMEOUT_MS	100

struct spec_src_config {

	GtkWidget *output;
	GtkWidget *plot;

	GByteArray *payload;

	guint id_out;
	guint id_spd;
};


static gboolean node_spec_src_deactivate_timeout_output (gpointer data)
{
	struct spec_src_config *cfg;


	cfg = (struct spec_src_config *) data;

	cfg->id_out = 0;
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_POINTS);

	return G_SOURCE_REMOVE;
}

static void node_spec_src_blink_output(struct spec_src_config *cfg)
{
	if (cfg->id_out)
		return;

	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_BLINK);

	cfg->id_out = g_timeout_add(SPEC_SRC_BLINK_TIMEOUT_MS,
				    node_spec_src_deactivate_timeout_output,
				    cfg);
}


static void node_spec_src_output(struct spec_src_config *cfg)
{
	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->output),
				    cfg->payload);

	node_spec_src_blink_output(cfg);
}

static void node_spec_src_handle_pr_spec_data(gpointer instance,
					      const struct spec_data *s,
					      struct spec_src_config *cfg)
{
	uint64_t i;
	uint64_t f;

	gsize len;

	gdouble *x;
	gdouble *y;
	struct nodes_point *spec;


	if (!s->n)
		return;

	len  = s->n * sizeof(struct nodes_point);
	spec = g_malloc(len);

	x = g_malloc(s->n * sizeof(gdouble));
	y = g_malloc(s->n * sizeof(gdouble));


	for (i = 0, f = s->freq_min_hz; i < s->n; i++, f += s->freq_inc_hz) {
		x[i] = spec[i].p0 = (gdouble) f * 1e-6;		/* Hz to Mhz */
		y[i] = spec[i].p1 = (gdouble) s->spec[i] * 0.001;	/* mK to K */
	}


	if (cfg->payload)
		g_byte_array_free (cfg->payload, TRUE);
	cfg->payload = g_byte_array_new_take((void *) spec, len);

	xyplot_drop_all_graphs(cfg->plot);
	xyplot_add_graph(cfg->plot, x, y, NULL, s->n,
			 g_strdup_printf("Spectrum"));
	xyplot_redraw(cfg->plot);
	node_spec_src_output(cfg);
}

static void node_spec_src_output_connected(GtkWidget *widget,
					   GtkWidget *source,
					   struct spec_src_config *cfg)
{
	/* push last spectrum */
	node_spec_src_output(cfg);
}


static void node_spec_src_remove(GtkWidget *w, struct spec_src_config *cfg)
{

	if (cfg->id_out)
		g_source_remove(cfg->id_out);

	if (cfg->payload)
		g_byte_array_free (cfg->payload, TRUE);

	g_signal_handler_disconnect(sig_get_instance(), cfg->id_spd);

	gtk_widget_destroy(w);
	g_free(cfg);
}


GtkWidget *node_spec_src_new(void)
{
	GtkWidget *w;
	GtkWidget *node;

	struct spec_src_config *cfg;


	cfg = g_malloc0(sizeof(struct spec_src_config));

	node = gtk_nodes_node_new();
	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_spec_src_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Spectrum Source");


	/* spectrum display */
	cfg->plot = xyplot_new();
	xyplot_set_xlabel(cfg->plot, "Frequency [MHz]");
	xyplot_set_ylabel(cfg->plot, "Amplitude [K]");
	gtk_widget_set_size_request(cfg->plot, 250, 250);
	gtk_nodes_node_item_add(GTKNODES_NODE(node), cfg->plot,
				GTKNODES_NODE_ITEM_NONE);
	gtk_nodes_node_item_set_fill(GTKNODES_NODE(node), cfg->plot, TRUE);
	gtk_nodes_node_item_set_expand(GTKNODES_NODE(node), cfg->plot, TRUE);

	/* output socket */
	w = gtk_label_new("Spectrum");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->output = gtk_nodes_node_item_add(GTKNODES_NODE(node), w,
					      GTKNODES_NODE_ITEM_SOURCE);
	gtk_nodes_node_item_set_packing(GTKNODES_NODE(node), w, GTK_PACK_END);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_POINTS);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->output),
				      KEY_POINTS);
	g_signal_connect(G_OBJECT(cfg->output), "socket-connect",
			 G_CALLBACK(node_spec_src_output_connected), cfg);

	/* connect external data source */
	cfg->id_spd = g_signal_connect(sig_get_instance(), "pr-spec-data",
				       G_CALLBACK(node_spec_src_handle_pr_spec_data),
				       cfg);

	return node;
}
