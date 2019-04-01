/**
 * @file    widgets/includes/history.h
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

#ifndef _WIDGETS_HISTORY_H_
#define _WIDGETS_HISTORY_H_


#include <gtk/gtk.h>


#define TYPE_HISTORY		(history_get_type())
#define HISTORY(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_HISTORY, History))
#define HISTORY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_HISTORY, HistoryClass))
#define IS_HISTORY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_HISTORY))
#define IS_HISTORY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_HISTORY))
#define HISTORY_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_HISTORY, HistoryClass))

typedef struct _History	History;
typedef struct _HistoryClass	HistoryClass;
typedef struct _HistoryConfig	HistoryPrivate;


struct _History {
	GtkBox parent;
	HistoryPrivate *cfg;
};

struct _HistoryClass {
	GtkBoxClass parent_class;
};


GtkWidget *history_new(void);


#endif /* _WIDGETS_HISTORY_H_ */

