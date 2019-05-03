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
 * @brief handle connected
 */

static void chatlog_connected(gpointer instance, gpointer data)
{


	GSettings *s;

	const gchar *nick;


	s = g_settings_new("org.uvie.radtel.config");
	if (!s)
		return;

	nick = g_settings_get_string(s, "nickname");

	cmd_nick(PKT_TRANS_ID_UNDEF, nick, strlen(nick));

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

static void chatlog_userlist(gpointer instance, const gchar *msg,
			     gpointer data)
{
	GtkTextBuffer *b;

	GtkTextIter start;
	GtkTextIter end;

	ChatLog *p;


	p = CHATLOG(data);


	b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(p->cfg->ulist));

	gtk_text_buffer_get_start_iter(b, &start);
	gtk_text_buffer_get_end_iter(b, &end);
	gtk_text_buffer_delete(b, &start, &end);
	gtk_text_buffer_insert_markup(b, &start, msg, -1);
}



static void chatlog_msg_output(gpointer instance, const gchar *msg,
			       gpointer data)
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

	ChatLog *p;


	p = CHATLOG(data);


	b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(p->cfg->chat));


	/* Timestamp */
	now = g_get_real_time();
	now_secs = (time_t) (now / 1000000);
	now_tm = localtime(&now_secs);

	strftime(time_buf, sizeof(time_buf), "%H:%M:%S", now_tm);

	stmp = g_strdup_printf("<tt>"
			       "<span foreground='#004F96'>%s.%03d </span>"
			       "%s</tt>",
			       time_buf, (gint) ((now / 1000) % 1000),
			       msg);


	mark = gtk_text_buffer_get_mark(b, "end");

	gtk_text_buffer_get_iter_at_mark(b, &iter, mark);

	gtk_text_buffer_insert_markup(b, &iter, stmp, -1);

	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(p->cfg->chat), mark);

	sig_status_push(msg);
	g_free(stmp);
}


static void chatlog_send_msg(GtkWidget *w, ChatLog *p)
{
	const gchar *buf;
	gchar *esc;


	buf = gtk_entry_get_text(GTK_ENTRY(p->cfg->input));

	esc = g_markup_escape_text(buf, strlen(buf));

	cmd_message(PKT_TRANS_ID_UNDEF, esc, strlen(esc));

	gtk_entry_set_text(GTK_ENTRY(p->cfg->input), "");

	g_free(esc);
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


	GtkTextBuffer *b;
	GtkTextIter iter;


	/** TODO general signals AND: send chat text on enter */

	/* vbox for chat output and input */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	/* chat output */
	w = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), w, TRUE, TRUE, 0);
	textview = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(w), textview);


	b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_get_end_iter(b, &iter);
	/* create right gravity mark on empty buffer, will always stay
	 * on the right of newly-inserted text
	 */
	gtk_text_buffer_create_mark(b, "end", &iter, FALSE);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);

	p->cfg->chat = textview;


	/* chat input */
	tmp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	context = gtk_widget_get_style_context(GTK_WIDGET(tmp));
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_LINKED);
	gtk_box_pack_start(GTK_BOX(vbox), tmp, FALSE, FALSE, 0);

	w = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(tmp), w, TRUE, TRUE, 0);
#if GTK_CHECK_VERSION(3,24,0) > GTK_CHECK_VERSION(GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION)
	g_object_set(w, "enable-emoji-completion", TRUE, NULL);
	g_object_set(w, "show-emoji-icon", TRUE, NULL);
#endif
	g_signal_connect(G_OBJECT(w), "activate",
			 G_CALLBACK(chatlog_send_msg), p);

	p->cfg->input = w;
	w = gtk_button_new_with_label("Send");
	gtk_box_pack_start(GTK_BOX(tmp), w, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(chatlog_send_msg), p);

	/* user list */
	frame = gtk_frame_new("Users");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	w = gtk_scrolled_window_new(NULL, NULL);
	textview = gtk_text_view_new();
	p->cfg->ulist = textview;
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);


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
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);

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
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_msg);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_uli);

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

	p->cfg->id_con = g_signal_connect(sig_get_instance(), "net-connected",
				 G_CALLBACK(chatlog_connected),
				 (void *) p);

	p->cfg->id_msg = g_signal_connect(sig_get_instance(), "pr-message",
				 G_CALLBACK(chatlog_msg_output),
				 (void *) p);

	p->cfg->id_uli = g_signal_connect(sig_get_instance(), "pr-userlist",
				 G_CALLBACK(chatlog_userlist),
				 (void *) p);


	g_signal_connect(p, "destroy", G_CALLBACK(chatlog_destroy), NULL);
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
