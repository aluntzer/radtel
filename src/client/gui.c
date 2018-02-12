/**
 * @file    client/gui.c
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
 * @brief client GUI setup
 */

#include <gtk/gtk.h>

#include <sky.h>
#include <xyplot.h>
#include <radio.h>
#include <signals.h>


static gboolean gui_spec_data_cb(gpointer instance, struct spec_data *s, gpointer *data)
{
	GtkWidget *plot;

	gdouble *xdata;
	gdouble *ydata;

	uint64_t f;
	uint64_t i;

	plot = GTK_WIDGET(data);

	xdata = g_malloc(s->n * sizeof(gdouble));
	ydata = g_malloc(s->n * sizeof(gdouble));


	s->freq_min_hz = 1420000000;
	s->freq_inc_hz = 150;

	s->freq_max_hz = s->freq_min_hz + s->freq_inc_hz * s->n;

	for (i = 0, f = s->freq_min_hz; i < s->n; i++, f += s->freq_inc_hz) {
		xdata[i] = (gdouble) f;
		ydata[i] = (gdouble) s->spec[i];
	}

	xyplot_set_data(plot, xdata, ydata, s->n);

	g_message("spec data CB!");

	return TRUE;
}



static GtkWidget *gui_create_specplot(void)
{
	GtkWidget *plot;




	plot = xyplot_new();

	xyplot_set_xlabel(plot, "Frequency");
	xyplot_set_ylabel(plot, "Amplitude");
#if 0

	{ /* dummy */
	#define LEN   8
	#define X0   -3.0
	#define INC   1.0

	gdouble *xdata;
	gdouble *ydata;

	gdouble d = X0;
	int i;
	xdata = g_malloc(LEN * sizeof(gdouble));
	ydata = g_malloc(LEN * sizeof(gdouble));


	for (i = 0; i < LEN; i++) {
		xdata[i] = d;
		ydata[i] = d * d;

		d += INC;
	}

	xyplot_set_data(plot, xdata, ydata, LEN);
	}
#endif
	g_signal_connect(sig_get_instance(), "cmd-spec-data",
			  (GCallback) gui_spec_data_cb,
			  (gpointer) plot);

	return plot;
}


static GtkWidget *gui_create_chat(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *textview;

	GtkWidget *w;
	GtkWidget *tmp;



	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

	/* vbox for chat, text input, log */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 12);

	/* chat output */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(w), textview);

	/* chat input */
	tmp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), tmp, FALSE, FALSE, 0);

	w = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(tmp), w, TRUE, TRUE, 0);
	w = gtk_button_new_with_label("Send");
	gtk_box_pack_start(GTK_BOX(tmp), w, FALSE, FALSE, 0);

	/** TODO general signals AND: send chat text on enter */

	/* user list */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(hbox), w, TRUE, TRUE, 12);
	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(w), textview);

	return hbox;
}


static GtkWidget *gui_create_log(void)
{
	GtkWidget *vbox;

	GtkWidget *w;
	GtkWidget *textview;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

	/* add event log to vbox */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, FALSE, 0);

	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(w), textview);

	w = gtk_button_new_with_label("Save Log");
	gtk_widget_set_hexpand(w, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, FALSE, 0);

	/* TODO add callbacks */

	return vbox;
}

static GtkWidget *gui_create_chatlog(void)
{
	GtkWidget *vbox;

	GtkWidget *w;


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);

	w = gui_create_chat();
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);

	w = gui_create_log();
	gtk_box_pack_start(GTK_BOX(vbox), w, FALSE, FALSE, 0);

	return vbox;
}



static GtkWidget *gui_create_stack_switcher(void)
{
	GtkWidget *stack;
	GtkWidget *stack_sw;
	GtkWidget *w;



	stack_sw = gtk_stack_switcher_new();


	stack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(stack),
			      GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

	gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_sw),
				     GTK_STACK(stack));

	w = sky_new();
	gtk_stack_add_named(GTK_STACK(stack), w, "SKY");
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", "SKY", NULL);
	w = radio_new();

	gtk_stack_add_named(GTK_STACK(stack), w, "RADIO");
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", "RADIO", NULL);

	w = gui_create_chatlog();
	gtk_stack_add_named(GTK_STACK(stack), w, "LOG");
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", "LOG", NULL);




	w = gui_create_specplot();
	gtk_stack_add_named(GTK_STACK(stack), w, "SPEC");
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", "SPEC", NULL);



	return stack_sw;
}

static GtkWidget *gui_create_status_view(void)
{
	GtkWidget *grid;

	GtkWidget *w;




	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
	g_object_set(grid, "margin", 18, NULL);



	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Azimuth</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_label_new("180°");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Elevation</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);

	w = gtk_label_new("45°");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>RA</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

	w = gtk_label_new("1h 2m 3s");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>DE</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 3, 1, 1);

	w = gtk_label_new("1° 2m 3s");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 3, 1, 1);




	w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_grid_attach(GTK_GRID(grid), w, 2, 0, 1, 4);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>F<sub>upper</sub></span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 3, 0, 1, 1);

	w = gtk_label_new("1000 MHz");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 4, 0, 1, 1);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>F<sub>lower</sub></span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 3, 1, 1, 1);

	w = gtk_label_new("1200 MHz");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 4, 1, 1, 1);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Bandwith</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 3, 2, 1, 1);

	w = gtk_label_new("123 kHz");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 4, 2, 1, 1);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Resolution</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 3, 3, 1, 1);

	w = gtk_label_new("50 kHz");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 4, 3, 1, 1);





	w = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_grid_attach(GTK_GRID(grid), w, 5, 0, 1, 4);

	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Network Data Rate</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 6, 0, 1, 1);

	w = gtk_label_new("720 kB/s");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 7, 0, 1, 1);


	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Readout Rate</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 6, 1, 1, 1);

	w = gtk_label_new("100 Hz");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 7, 1, 1, 1);


	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Refresh Rate</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 6, 2, 1, 1);

	w = gtk_label_new("10 Hz");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 7, 2, 1, 1);


	w = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(w), "<span alpha='50%'>Averaging</span>");
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(GTK_GRID(grid), w, 6, 3, 1, 1);

	w = gtk_label_new("5x");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(GTK_GRID(grid), w, 7, 3, 1, 1);

	return grid;
}


int gui_client(int argc, char *argv[])
{
	static GtkWidget *window = NULL;
	GtkWidget *box;
	GtkWidget *stack_sw;
	GtkWidget *w;
	GtkWidget *header;
	guint i;


	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	gtk_widget_set_size_request(window, 500, 350);



	header = gtk_header_bar_new();

	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);

	gtk_window_set_titlebar(GTK_WINDOW(window), header);

	gtk_window_set_title(GTK_WINDOW(window), "GUI");

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_widget_destroyed),
			 &window);


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);

	stack_sw = gui_create_stack_switcher();
	gtk_widget_set_halign(GTK_WIDGET(stack_sw), GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(box), stack_sw, FALSE, FALSE, 0);

	w = GTK_WIDGET(gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER(stack_sw)));
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);

	w = gui_create_status_view();
	gtk_widget_set_halign(GTK_WIDGET(w), GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(window), box);


	if (!gtk_widget_get_visible (window))
		gtk_widget_show_all (window);
	else
		gtk_widget_destroy (window);


	return 0;

}
