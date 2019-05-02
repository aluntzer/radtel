/**
 * @file    widgets/includes/chatlog.h
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

#ifndef _WIDGETS_CHATLOG_H_
#define _WIDGETS_CHATLOG_H_


#include <gtk/gtk.h>


#define TYPE_CHATLOG		(chatlog_get_type())
#define CHATLOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_CHATLOG, ChatLog))
#define CHATLOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_CHATLOG, ChatLogClass))
#define IS_CHATLOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_CHATLOG))
#define IS_CHATLOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_CHATLOG))
#define CHATLOG_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_CHATLOG, ChatLogClass))

typedef struct _ChatLog	ChatLog;
typedef struct _ChatLogClass	ChatLogClass;
typedef struct _ChatLogConfig	ChatLogPrivate;


struct _ChatLog {
	GtkBox parent;
	ChatLogPrivate *cfg;
};

struct _ChatLogClass {
	GtkBoxClass parent_class;
};


GType chatlog_get_type(void) G_GNUC_CONST;


GtkWidget *chatlog_new(void);


#endif /* _WIDGETS_CHATLOG_H_ */

