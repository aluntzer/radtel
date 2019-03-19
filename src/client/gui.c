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
#include <sys_status.h>
#include <obs_assist.h>



static GtkWidget *gui_create_chat(void)
{
	GtkWidget *vbox;
	GtkWidget *textview;

	GtkWidget *w;
	GtkWidget *tmp;

	GtkWidget *paned;

	GtkWidget *frame;

	GtkStyleContext *context;


	/** TODO general signals AND: send chat text on enter */

	/* vbox for chat output and input */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	/* chat output */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(w), textview);

	/* chat input */
	tmp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	context = gtk_widget_get_style_context(GTK_WIDGET(tmp));
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_LINKED);
	gtk_box_pack_start(GTK_BOX(vbox), tmp, FALSE, FALSE, 0);

	w = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(tmp), w, TRUE, TRUE, 0);
	w = gtk_button_new_with_label("Send");
	gtk_box_pack_start(GTK_BOX(tmp), w, FALSE, TRUE, 0);


	/* user list */
	frame = gtk_frame_new("Users");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	w = gtk_scrolled_window_new(NULL, NULL);
	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(w), textview);
	gtk_container_add(GTK_CONTAINER(frame), w);
	w = frame;


	/* GtkPaned for chat output and user list */
	paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

	/* pack vbox into left paned */
	frame = gtk_frame_new("Chat");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_paned_pack1(GTK_PANED(paned), frame, TRUE, TRUE);

	/* pack user list window in right paned */
	gtk_paned_pack2(GTK_PANED(paned), w, TRUE, TRUE);

	gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);


	return paned;
}

#include <glib.h>
static void log_output(const gchar *logdomain, GLogLevelFlags loglevel,
		       const gchar *message, gpointer userdata)
{
	GtkTextBuffer *b;

	GtkTextIter iter;
	GtkTextMark *mark;

	/* timestamp */
	gint64 now;
	time_t now_secs;
	struct tm *now_tm;
	char time_buf[128];
	char *stmp;


	b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(userdata));


	/* Timestamp */
	now = g_get_real_time();
	now_secs = (time_t) (now / 1000000);
	now_tm = localtime(&now_secs);

	strftime(time_buf, sizeof(time_buf), "%H:%M:%S", now_tm);

	stmp = g_strdup_printf("<tt>"
			       "<span foreground='#004F96'>%s.%03d </span>"
			       "%s</tt>\n",
			       time_buf, (gint) ((now / 1000) % 1000),
			       message);


	mark = gtk_text_buffer_get_mark(b, "end");

	gtk_text_buffer_get_iter_at_mark(b, &iter, mark);

	gtk_text_buffer_insert_markup(b, &iter, stmp, -1);

	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(userdata), mark);

	g_free(stmp);
}

static GtkWidget *gui_create_log(void)
{
	GtkWidget *frame;

	GtkWidget *w;
	GtkWidget *textview;




	frame = gtk_frame_new("Log");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	/* add event log to vbox */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(frame), w);

	textview = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 3);
	gtk_container_add(GTK_CONTAINER(w), textview);

	{
		GtkTextBuffer *b;
		GtkTextIter iter;
		b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
		gtk_text_buffer_get_end_iter(b, &iter);
		/* create right gravity mark on empty buffer, will always stay
		 * on the right of newly-inserted text
		 */
		gtk_text_buffer_create_mark(b, "end", &iter, FALSE);
	}
/**** XXX console for now... ****/
#if 0
	 g_log_set_handler(NULL, G_LOG_LEVEL_MASK, log_output, (gpointer) textview);
#endif
	/* TODO add callbacks, right-click menu */

	return frame;
}

static GtkWidget *gui_create_chatlog(void)
{
	GtkWidget *w;
	GtkWidget *paned;


	paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);

	w = gui_create_chat();
	gtk_paned_pack1(GTK_PANED(paned), w, TRUE, TRUE);
	w = gui_create_log();
	gtk_paned_pack2(GTK_PANED(paned), w, TRUE, TRUE);

	return paned;
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


static GtkWidget *gui_create_default_window(void)
{
	GtkWidget *win;
	GtkWidget *hdr;
	GtkWidget *btn;


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
#if 0
	w = gui_create_status_view();
	gtk_widget_set_halign(GTK_WIDGET(w), GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
#endif
	header = gtk_window_get_titlebar(GTK_WINDOW(*win));

	sswdnd_add_header_buttons(sswdnd, header);

	if (!gtk_widget_get_visible(*win))
		gtk_widget_show_all(*win);
	else
		gtk_widget_destroy(*win);
}


static GtkWidget *gui_create_stack_switcher(void)
{
	GtkWidget *w;
	GtkWidget *sswdnd;
	GtkStack *stack;



	sswdnd = sswdnd_new();

	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(w), obs_assist_new());
	sswdnd_add_named(sswdnd, w, "Observation");


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


	sswdnd_add_named(sswdnd, gui_create_chatlog(), "Log");
	sswdnd_add_named(sswdnd, sky_new(), "Sky View");


	stack = gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER(sswdnd));

#if 0
	g_signal_connect(stack, "sswdnd-create-window",
			 G_CALLBACK(gui_create_window_with_widget), NULL);
#endif
	return sswdnd;
}





int gui_client(int argc, char *argv[])
{
	GtkWidget *window;

	gtk_init(&argc, &argv);


	gui_create_window_with_widget(NULL, &window,
				      gui_create_stack_switcher(), NULL);

	return 0;
}

