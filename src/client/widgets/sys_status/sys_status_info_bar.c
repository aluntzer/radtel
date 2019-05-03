/**
 * @file    widgets/sys_status/sys_status_info_bar.c
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
 */

#include <sys_status.h>
#include <sys_status_cfg.h>

#include <default_grid.h>
#include <signals.h>


/**
 * @brief status bar response signal handler
 */

static void sys_status_on_bar_response(GtkInfoBar *bar, gint resp_id,
				       SysStatus *p)
{
	if (resp_id != GTK_RESPONSE_CLOSE)
		return;

	gtk_info_bar_set_revealed(bar, FALSE);
}


/**
 * @brief keep showing the messages until the buffer is empty
 */

static gboolean sys_status_bar_update_cb(gpointer data)
{
	SysStatus *p;


	p = SYS_STATUS(data);


	gtk_info_bar_set_revealed(GTK_INFO_BAR(p->cfg->info_bar), FALSE);

	p->cfg->id_to_msg = 0;

	return G_SOURCE_REMOVE;
}


/**
 * @brief handle status bar messages
 */

void sys_status_handle_status_push(gpointer instance, const gchar *msg,
				   gpointer data)
{
	const GSourceFunc sf = sys_status_bar_update_cb;


	gint64 now;
	time_t now_secs;
	struct tm *now_tm;
	char time_buf[128];

	gchar *showmsg;

	SysStatus *p;


	p = SYS_STATUS(data);


	/* timestamp */
	now = g_get_real_time();
	now_secs = (time_t) (now / 1000000);
	now_tm = localtime(&now_secs);

	strftime(time_buf, sizeof(time_buf), "%H:%M:%S", now_tm);

	showmsg = g_strdup_printf("%s %s",time_buf, msg);

	gtk_label_set_markup(p->cfg->info_bar_lbl, showmsg);
	gtk_info_bar_set_revealed(GTK_INFO_BAR(p->cfg->info_bar), TRUE);

	g_free(showmsg);

	if (p->cfg->id_to_msg)
	    g_source_remove(p->cfg->id_to_msg);

	p->cfg->id_to_msg = g_timeout_add_seconds(10, sf, p);
}


/**
 * @brief create a info bar which can update its label and appear on signal
 */

GtkWidget *sys_status_info_bar_new(SysStatus *p)
{
	GtkWidget *w;
	GtkWidget *label;


	w = gtk_info_bar_new();

	gtk_info_bar_set_show_close_button(GTK_INFO_BAR (w), TRUE);
	gtk_info_bar_set_revealed(GTK_INFO_BAR(w), FALSE);
	gtk_info_bar_set_message_type(GTK_INFO_BAR(w), GTK_MESSAGE_INFO);
	g_signal_connect(w, "response",
			 G_CALLBACK(sys_status_on_bar_response), p);

	p->cfg->info_bar = w;


	label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(label), 0);

	w = gtk_info_bar_get_content_area(GTK_INFO_BAR(p->cfg->info_bar));
	gtk_box_pack_start(GTK_BOX(w), label, FALSE, FALSE, 0);

	p->cfg->info_bar_lbl = GTK_LABEL(label);


	return p->cfg->info_bar;
}

