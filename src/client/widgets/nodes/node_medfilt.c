/**
 * @file    widgets/nodes/node_medfilt.c
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
 * @brief a GtkNode to apply a moving median filter to the y-Axis of a dataset
 */

#include <gtk/gtk.h>
#include <gtknode.h>
#include <gtknodesocket.h>
#include <nodes.h>


#define MEDFILT_BLINK_TIMEOUT_MS	100

struct medfilt_config {

	GtkWidget *input;
	GtkWidget *output;
	GtkWidget *plot;

	gsize n_elem;
	struct nodes_point *data;
	GByteArray *payload;

	gsize filter_len;

	guint id_out;
};


static void shell_sort(gdouble *arr, gsize n)
{
	gsize i, j, gap;

	gdouble tmp;


	/* let's use Shell's original O(n^2) gap sequence */
	for (gap = n/2; gap > 0; gap /= 2) {

		/* perform the gapped insertion sort */
		for(i = gap; i < n; i++) {

			/* save the first element in the subsequence */
			tmp = arr[i];

			/* shift down the elements from the current index
			 * until a proper slots has been found for the saved
			 * first element */
			for (j = i; j >= gap; j -= gap) {

				/* tmp can go to current index */
				if (arr[j - gap] <= tmp)
					break;

				/* tmp is still larger than any value in the
				 * current subsequence */
				arr[j] = arr[j - gap];
			}

			/* assign to the slot determined in the loop */
			arr[j] = tmp;
		}
	}
}


static gboolean node_median_deactivate_timeout_output (gpointer data)
{
	struct medfilt_config *cfg;


	cfg = (struct medfilt_config *) data;

	cfg->id_out = 0;
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_POINTS);

	return G_SOURCE_REMOVE;
}


static void node_medfilt_blink_output(struct medfilt_config *cfg)
{
	if (cfg->id_out)
		return;

	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_BLINK);

	cfg->id_out = g_timeout_add(MEDFILT_BLINK_TIMEOUT_MS,
				    node_median_deactivate_timeout_output,
				    cfg);
}

static void node_medfilt_output(struct medfilt_config *cfg)
{
	gtk_nodes_node_socket_write(GTKNODES_NODE_SOCKET(cfg->output),
				    cfg->payload);

	node_medfilt_blink_output(cfg);
}

static void node_medfilt_apply(struct medfilt_config *cfg)
{
	gsize i;
	gsize mid;
	gsize len;

	struct nodes_point *out;

	gdouble *y;
	gdouble *m;


	if (!cfg->data)
		return;

	if (cfg->n_elem < 3)
		return;

	mid = ((cfg->filter_len - 1) / 2);

	len = cfg->n_elem * sizeof(struct nodes_point);
	out = g_malloc(len);
	m   = g_malloc(cfg->filter_len * sizeof(gdouble));
	y   = g_malloc(cfg->n_elem * sizeof(gdouble));


	for (i = 0; i < cfg->n_elem; i++) {
		out[i].p0 = cfg->data[i].p0;
		y[i] = cfg->data[i].p1;
	}

	out[0].p1 = y[0];	/* copy first element */

	/* expand filter on lead-in segment (index < midpoint) */
	for (i = 1; i < mid; i++) {
		gsize tm = (i - 1) / 2;
		gsize tf =  2 * tm + 1;

		memcpy(m, &y[i - tm], tf * sizeof(gdouble));
		shell_sort(m, tf);

		out[i].p1 = m[tm];
	}

	/* filter main segment */
	for (; i < cfg->n_elem - mid; i++) {

		memcpy(m, &y[i - mid], cfg->filter_len * sizeof(gdouble));
		shell_sort(m, cfg->filter_len);

		out[i].p1 = m[mid];
	}

	/* de-expand filter on lead-out segment (remaining len < midpoint) */
	for (; i < cfg->n_elem - 1; i++) {
		gsize tm = (cfg->n_elem - i) / 2;
		gsize tf =  2 * tm + 1;

		memcpy(m, &y[i - tm], tf * sizeof(gdouble));
		shell_sort(m, tf);

		out[i].p1 = m[tm];
	}

	out[cfg->n_elem - 1].p1 = y[cfg->n_elem - 1];	/* copy last element */


	if (cfg->payload)
		g_byte_array_free (cfg->payload, TRUE);
	cfg->payload = g_byte_array_new_take((void *) out, len);


	node_medfilt_output(cfg);

	g_free(y);
	g_free(m);
}


static void node_med_filter_len(GtkWidget *w,  struct medfilt_config *cfg)
{
	cfg->filter_len = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

	node_medfilt_apply(cfg);
}


static void node_medfilt_input(GtkWidget *widget,
			       GByteArray *payload,
			       struct medfilt_config *cfg)
{
	if (!payload)
		return;

	if (!payload->len)
		return;

	if (cfg->data)
		g_free(cfg->data);

	cfg->n_elem = payload->len / sizeof(struct nodes_point);

	cfg->data = g_memdup(payload->data, payload->len);

	node_medfilt_apply(cfg);
}

static void node_medfilt_output_connected(GtkWidget *widget,
					  GtkWidget *source,
					  struct medfilt_config *cfg)
{
	/* push last dataset */
	node_medfilt_output(cfg);
}

static void node_medfilt_remove(GtkWidget *w, struct medfilt_config *cfg)
{
	if (cfg->id_out)
		g_source_remove(cfg->id_out);

	if (cfg->data)
		g_free(cfg->data);

	if (cfg->payload)
		g_byte_array_free (cfg->payload, TRUE);

	gtk_widget_destroy(w);
	g_free(cfg);
}


GtkWidget *node_medfilt_new(void)
{
	GtkWidget *w;
	GtkWidget *grid;
	GtkWidget *node;

	struct medfilt_config *cfg;


	cfg = g_malloc0(sizeof(struct medfilt_config));

	node = gtk_nodes_node_new();
	g_signal_connect(G_OBJECT(node), "node-func-clicked",
			 G_CALLBACK(node_medfilt_remove), cfg);

	gtk_nodes_node_set_label(GTKNODES_NODE (node), "Median Filter");

	/* input socket */

	w = gtk_label_new("Data");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	cfg->input = gtk_nodes_node_item_add(GTKNODES_NODE(node), w,
					    GTKNODES_NODE_ITEM_SINK);
	g_signal_connect(G_OBJECT(cfg->input), "socket-incoming",
			 G_CALLBACK(node_medfilt_input), cfg);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->input),
				       &COL_POINTS);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->input),
				      KEY_POINTS);


	/* grid containing user controls */

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_nodes_node_item_add(GTKNODES_NODE(node), grid,
				GTKNODES_NODE_ITEM_NONE);

	w = gtk_label_new("Length");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);
	w = gtk_spin_button_new_with_range(1, 21, 2);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(node_med_filter_len), cfg);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 3);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);



	/* output socket */
	w = gtk_label_new("Data");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	cfg->output = gtk_nodes_node_item_add(GTKNODES_NODE(node), w,
					      GTKNODES_NODE_ITEM_SOURCE);
	gtk_nodes_node_item_set_packing(GTKNODES_NODE(node), w, GTK_PACK_END);
	gtk_nodes_node_socket_set_rgba(GTKNODES_NODE_SOCKET(cfg->output),
				       &COL_POINTS);
	gtk_nodes_node_socket_set_key(GTKNODES_NODE_SOCKET(cfg->output),
				      KEY_POINTS);
	g_signal_connect(G_OBJECT(cfg->output), "socket-connect",
			 G_CALLBACK(node_medfilt_output_connected), cfg);


	return node;
}
