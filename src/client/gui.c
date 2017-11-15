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

#include <xyplot.h>


/**
 * @brief create a label with a description in smaller text
 */

static GtkWidget *gui_create_desclabel(const gchar *text, const gchar *desc)
{
	GtkWidget *box;
	GtkWidget *label;

	gchar *str;



	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	str = g_strconcat ("<span size='small'>", desc, "</span>", NULL);
	gtk_label_set_markup(GTK_LABEL(label), str);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);

	gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

	return box;
}




static GtkWidget *gui_create_radio_controls(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *box;
	GtkWidget *box2;

	GtkWidget *w;
	GtkWidget *tmp;


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);

	/* row 1 */
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	w = gui_create_desclabel("Spectral Acquisition",
				 "Enable persistent acquisition of spectral data by the server");

	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 18);

	w = gtk_switch_new();
	gtk_box_pack_end(GTK_BOX(hbox), w, FALSE, FALSE, 18);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
	w = gtk_button_new_with_label("Single Shot");
	gtk_box_pack_end(GTK_BOX(hbox), w, FALSE, FALSE, 18);


	/* row 2 */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	w = gui_create_desclabel("Acquisition Frequency Range",
				 "Configure the upper and lower frequency limits of the receiver");

	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 18);


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_end(GTK_BOX(hbox), box, FALSE, FALSE, 18);


	box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_end(GTK_BOX(box), box2, FALSE, FALSE, 0);

	w = gtk_spin_button_new_with_range(0.0, 10.0, 1.0);
	gtk_box_pack_end(GTK_BOX(box2), w, FALSE, FALSE, 0);
	w = gtk_label_new("Lower");
	gtk_box_pack_end(GTK_BOX(box2), w, FALSE, FALSE, 12);


	box2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_end(GTK_BOX(box), box2, FALSE, FALSE, 0);
	w = gtk_spin_button_new_with_range(0.0, 10.0, 1.0);
	gtk_box_pack_end(GTK_BOX(box2), w, FALSE, FALSE, 0 );
	w = gtk_label_new("Upper");
	gtk_box_pack_end(GTK_BOX(box2), w, FALSE, FALSE, 12);


	/* row 3 */
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);

	w = gui_create_desclabel("Reference Rest Frequency",
				 "Used to calculate the radial (Doppler) velocity to the Local Standard of Rest");

	gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 18);



	/* note: for easier selection, always give J (total electronic angular
	 * momentum quantum number) and F (transitions between hyperfine
	 * levels)
	 *
	 * note on OH: the ground rotational state splits into a lambda-doublet
	 * sub-levels due to the interaction between the rotational and electronic
	 * angular momenta of the molecule. The sub-levels further split into
	 * two hyperfine levels as a result of the interaction between the
	 * electron and nuclear spins of the hydrogen atom. The transitions that
	 * connect sub-levels with the same F-values are called the main lines,
	 * whereas the transitions between sub-levels of different F-values are
	 * called the satellite lines. (See DICKE'S SUPERRADIANCE IN
	 * ASTROPHYSICS. II. THE OH 1612 MHz LINE, F. Rajabi and M. Houde,
	 * The Astrophysical Journal, Volume 828, Number 1.)
	 * The main lines are stronger than the satellite lines. In star
	 * forming regions, the 1665 MHz line exceeds the 1667 MHz line in
	 * intensity, while in equilibirium conditions, it is generally weaker.
	 * In late-type starts, the 1612 MHz line may sometimes be equal or
	 * even exceed the intensity of the main lines.
	 */

	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "1420.406", "Hydrogen (HI) J=1/2 F=1-0");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "1612.231", "Hydroxyl Radical (OH) J=3/2 F=1-2");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "1665.402", "Hydroxyl Radical (OH) J=3/2 F=1-1");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "1667.359", "Hydroxyl Radical (OH) J=3/2 F=2-2");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "1720.530", "Hydroxyl Radical (OH) J=3/2 F=2-1");

	tmp = gtk_button_new_with_label("Set");
	gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 18);

	tmp = gtk_entry_new();
	g_object_bind_property(w, "active-id", tmp,
			       "text", G_BINDING_BIDIRECTIONAL);
	gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 18);


	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);


	gtk_box_pack_end(GTK_BOX(hbox), tmp, FALSE, FALSE, 18);
	gtk_box_pack_end(GTK_BOX(hbox), w, FALSE, FALSE, 18);
	w = gtk_label_new("Presets:");
	gtk_box_pack_end(GTK_BOX(hbox), w, FALSE, FALSE, 12);




	return vbox;
}


static GtkWidget *gui_create_specplot(void)
{
	GtkWidget *plot;




	plot = xyplot_new();

	xyplot_set_xlabel(plot, "Frequency");
	xyplot_set_ylabel(plot, "Amplitude");


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

	return plot;
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

	w = gui_create_radio_controls();
	gtk_stack_add_named(GTK_STACK(stack), w, "RADIO");
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", "RADIO", NULL);

	w = gtk_label_new("SKY");
	gtk_stack_add_named(GTK_STACK(stack), w, "SKY");
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", "SKY", NULL);


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
