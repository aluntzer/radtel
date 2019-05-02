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

#include <signals.h>
#include <sswdnd.h>
#include <sky.h>
#include <xyplot.h>
#include <radio.h>
#include <telescope.h>
#include <spectrum.h>
#include <history.h>
#include <sys_status.h>
#include <obs_assist.h>
#include <chatlog.h>
#include <default_grid.h>


static void gui_net_cmd_failed(gpointer instance, guint16 trans_id,
			       gpointer data)
{
	gchar *msg;

	msg = g_strdup_printf("Server rejected command packet (tr_id: %d)",
			      trans_id);
	sig_status_push(msg);
	g_free(msg);
}


static void gui_font_set_cb(GtkFontButton *w, gpointer data)
{
	GSettings *s;

	gchar *name;


	s = g_settings_new("org.uvie.radtel.config");

	name = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(w));

	g_object_set(gtk_settings_get_default(),
		     "gtk-font-name", name,
		     NULL);

	if (s)
		g_settings_set_string(s, "gui-font", name);

	g_free(name);
}


static void gui_host_entry_changed_cb(GtkEditable *ed, gpointer data)
{
	GSettings *s;


	s = g_settings_new("org.uvie.radtel.config");
	if (!s)
		return;

	if (!gtk_entry_get_text_length(GTK_ENTRY(ed)))
		return;

	g_settings_set_string(s, "server-addr", gtk_entry_get_text(GTK_ENTRY(ed)));
}


static void gui_port_entry_changed_cb(GtkEditable *ed, gpointer data)
{
	guint port;

	GSettings *s;

	gchar *new = NULL;


	s = g_settings_new("org.uvie.radtel.config");
	if (!s)
		return;

	if (!gtk_entry_get_text_length(GTK_ENTRY(ed)))
		return;


	port = strtol(gtk_entry_get_text(GTK_ENTRY(ed)), NULL, 10);

	if (!port)
		new = g_strdup_printf("%d", 1);

	if (port > 0xffff)
		new = g_strdup_printf("%d", 0xffff);

	if (new) {
		gtk_entry_set_text(GTK_ENTRY(ed), new);
		g_free(new);
	}

	g_settings_set_uint(s, "server-port", (guint) port);
}


static void gui_port_entry_insert_text_cb(GtkEditable *ed, gchar *new_text,
					  gint new_text_len, gpointer pos,
					  gpointer data)
{
	gint i;


	if ((gtk_entry_get_text_length(GTK_ENTRY(ed)) + new_text_len) > 5) {
		g_signal_stop_emission_by_name(ed, "insert-text");
		return;
	}

	for (i = 0; i < new_text_len; i++) {
		if (!g_ascii_isdigit(new_text[i])) {
			g_signal_stop_emission_by_name(ed, "insert-text");
			break;
		}
	}
}


static GtkWidget *gui_create_popover_menu(GtkWidget *widget)
{
	GtkGrid *grid;

	GtkWidget *w;
	GtkWidget *pop;

	GSettings *s;

	gchar *name;
	guint port;



	s = g_settings_new("org.uvie.radtel.config");
	if (!s)
		return NULL;


	grid = GTK_GRID(new_default_grid());

	w = gtk_label_new("Host");
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_widget_set_valign(w, GTK_ALIGN_BASELINE);
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_entry_new();
	name = g_settings_get_string(s, "server-addr");
	gtk_entry_set_text(GTK_ENTRY(w), name);
	g_free(name);

	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	g_signal_connect(w, "changed", G_CALLBACK(gui_host_entry_changed_cb),
			 NULL);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 0, 1, 1);



	w = gtk_label_new("Port");
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_widget_set_valign(w, GTK_ALIGN_BASELINE);
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 1, 1, 1);

	w = gtk_entry_new();
	port = g_settings_get_uint(s, "server-port");
	name = g_strdup_printf("%d", port);

	gtk_entry_set_text(GTK_ENTRY(w), name);
	g_free(name);

	gtk_entry_set_alignment(GTK_ENTRY(w), 1.0);
	g_signal_connect(w, "insert-text",
			 G_CALLBACK(gui_port_entry_insert_text_cb), NULL);
	g_signal_connect(w, "changed", G_CALLBACK(gui_port_entry_changed_cb),
			 NULL);
	gtk_entry_set_input_purpose(GTK_ENTRY(w), GTK_INPUT_PURPOSE_DIGITS);
	gtk_grid_attach(GTK_GRID(grid), w, 1, 1, 1, 1);





	w = gtk_label_new("Font");
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_widget_set_valign(w, GTK_ALIGN_BASELINE);
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "dim-label");
	gtk_grid_attach(GTK_GRID(grid), w, 0, 2, 1, 1);

	w = gtk_font_button_new();
	g_object_get(gtk_settings_get_default(), "gtk-font-name", &name, NULL);
	gtk_font_chooser_set_font(GTK_FONT_CHOOSER(w), name);
	g_signal_connect(w, "font-set", G_CALLBACK(gui_font_set_cb), NULL);
	g_free(name);

	gtk_grid_attach(GTK_GRID(grid), w, 1, 2, 1, 1);




	pop = gtk_popover_new(widget);
	gtk_popover_set_position(GTK_POPOVER(pop), GTK_POS_TOP);
	gtk_container_add(GTK_CONTAINER(pop), GTK_WIDGET(grid));
	gtk_container_set_border_width(GTK_CONTAINER(pop), 6);

	gtk_widget_show_all(GTK_WIDGET(grid));




	return pop;
}


static GtkWidget *gui_create_default_window(void)
{
	GtkWidget *win;
	GtkWidget *hdr;


	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), "GUI");
	gtk_window_set_default_size(GTK_WINDOW(win), 800, 800);
	gtk_window_set_resizable(GTK_WINDOW(win), TRUE);

	g_signal_connect(win, "destroy", G_CALLBACK(gtk_widget_destroyed),
			 NULL);

	hdr = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hdr), TRUE);

	gtk_window_set_titlebar(GTK_WINDOW(win), hdr);


	return win;
}


static void gui_create_window_with_widget(GtkWidget *p, GtkWidget **win,
					  GtkWidget *sswdnd, gpointer data)
{
	GtkWidget *w;
	GtkWidget *box;

	GtkStack *stack;
	GtkWidget *header;


	(*win) = gui_create_default_window();

	stack = gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER(sswdnd));
	g_signal_connect(GTK_WIDGET(stack), "sswdnd-create-window",
			 G_CALLBACK(gui_create_window_with_widget), NULL);


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER((*win)), box);

	gtk_widget_set_halign(GTK_WIDGET(sswdnd), GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(box), sswdnd, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(stack), TRUE, TRUE, 6);

	w = sys_status_new();
	gtk_widget_set_halign(GTK_WIDGET(w), GTK_ALIGN_FILL);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);


	header = gtk_window_get_titlebar(GTK_WINDOW(*win));

	sswdnd_add_header_buttons(sswdnd, header);


	w = gtk_menu_button_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "circular");
	gtk_button_set_always_show_image(GTK_BUTTON(w), TRUE);
	gtk_button_set_image(GTK_BUTTON(w),
		     gtk_image_new_from_icon_name("emblem-system-symbolic",
						  GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_tooltip_text(w, "Settings");
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(w),
				    gui_create_popover_menu(w));
	gtk_header_bar_pack_end(GTK_HEADER_BAR(header), w);



	if (!gtk_widget_get_visible(*win))
		gtk_widget_show_all(*win);
	else
		gtk_widget_destroy(*win);
}


static GtkWidget *gui_create_stack_switcher(void)
{
	GtkWidget *w;
	GtkWidget *sswdnd;



	sswdnd = sswdnd_new();


	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), chatlog_new());
	sswdnd_add_named(sswdnd, w, "Chat & Log");



	sswdnd_add_named(sswdnd, sky_new(), "Sky View");


	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), spectrum_new());
	sswdnd_add_named(sswdnd, w, "Spectrum");


	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), telescope_new());
	sswdnd_add_named(sswdnd, w, "Telescope");


	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), radio_new());
	sswdnd_add_named(sswdnd, w, "Spectrometer");


	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), history_new());
	sswdnd_add_named(sswdnd, w, "History");


	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), obs_assist_new());
	sswdnd_add_named(sswdnd, w, "Observation");



	return sswdnd;
}





int gui_client(int argc, char *argv[])
{
	GSettings *s;

	GtkWidget *window;

	gchar *name = NULL;


	gtk_init(&argc, &argv);

	g_object_set(gtk_settings_get_default(),
		     "gtk-application-prefer-dark-theme", TRUE,
		     NULL);


	s = g_settings_new("org.uvie.radtel.config");

	if (s) {
		name = g_settings_get_string(s, "gui-font");
		if (name)
			g_object_set(gtk_settings_get_default(),
				     "gtk-font-name", name, NULL);
	}



	g_free(name);


	gui_create_window_with_widget(NULL, &window,
				      gui_create_stack_switcher(), NULL);


	/* setup here until we have a transaction log */
	g_signal_connect(sig_get_instance(), "pr-fail",
				  (GCallback) gui_net_cmd_failed,
				  NULL);

	return 0;
}

