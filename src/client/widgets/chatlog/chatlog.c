/**
 * @file    widgets/chatlog/chatlog.c
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
 * @brief a widget to show local and remote systems status info
 *
 */


#include <chatlog.h>
#include <chatlog_cfg.h>

#include <desclabel.h>
#include <signals.h>
#include <cmd.h>


G_DEFINE_TYPE_WITH_PRIVATE(ChatLog, chatlog, GTK_TYPE_BOX)


/**
 * @brief fetch needed configuration data
 */

static void chatlog_fetch_config(void)
{
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
	cmd_getpos_azel(PKT_TRANS_ID_UNDEF);
}


/**
 * @brief handle connected
 */

static void chatlog_connected(gpointer instance, gpointer data)
{
	chatlog_fetch_config();
}




static void chatlog_log_output(const gchar *logdomain, GLogLevelFlags loglevel,
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


static GtkWidget *chatlog_create_chat(ChatLog *p)
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


static GtkWidget *chatlog_create_log(ChatLog *p)
{
	GtkWidget *frame;

	GtkWidget *w;
	GtkWidget *textview;

	GtkTextBuffer *b;
	GtkTextIter iter;

	const GLogLevelFlags flags = G_LOG_LEVEL_MASK & ~G_LOG_LEVEL_DEBUG;


	frame = gtk_frame_new("Log");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	/* add event log to vbox */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(frame), w);

	textview = gtk_text_view_new();
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(textview), 3);
	gtk_container_add(GTK_CONTAINER(w), textview);

	b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_get_end_iter(b, &iter);
	/* create right gravity mark on empty buffer, will always stay
	 * on the right of newly-inserted text
	 */
	gtk_text_buffer_create_mark(b, "end", &iter, FALSE);

	p->cfg->id_log = g_log_set_handler(NULL, flags, chatlog_log_output,
					   (gpointer) textview);

	return frame;
}


static void gui_create_chatlog(ChatLog *p)
{
	GtkWidget *w;
	GtkWidget *paned;


	paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);

	w = chatlog_create_chat(p);
	gtk_paned_pack1(GTK_PANED(paned), w, TRUE, TRUE);
	w = chatlog_create_log(p);
	gtk_paned_pack2(GTK_PANED(paned), w, TRUE, TRUE);

	gtk_box_pack_start(GTK_BOX(p), paned, TRUE, TRUE, 6);
}


/**
 * @brief destroy signal handler
 */

static gboolean chatlog_destroy(GtkWidget *w, void *data)
{
	ChatLog *p;


	p = CHATLOG(w);

	g_log_remove_handler(NULL, p->cfg->id_log);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_con);

	return TRUE;
}


/**
 * @brief initialise the ChatLog class
 */

static void chatlog_class_init(ChatLogClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the ChatLog widget
 */

static void chatlog_init(ChatLog *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_CHATLOG(p));

	p->cfg = chatlog_get_instance_private(p);

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_chatlog(p);

	p->cfg->id_con = g_signal_connect(sig_get_instance(), "connected",
				 G_CALLBACK(chatlog_connected),
				 (void *) p);

	g_signal_connect(p, "destroy", G_CALLBACK(chatlog_destroy), NULL);

	chatlog_fetch_config();
}


/**
 * @brief create a new ChatLog widget
 */

GtkWidget *chatlog_new(void)
{
	ChatLog *chatlog;


	chatlog = g_object_new(TYPE_CHATLOG, NULL);

	return GTK_WIDGET(chatlog);
}
