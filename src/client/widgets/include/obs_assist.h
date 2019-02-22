/**
 * @file    widgets/includes/obs_assist.h
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

#ifndef _WIDGETS_OBS_ASSIST_H_
#define _WIDGETS_OBS_ASSIST_H_


#include <gtk/gtk.h>


#define TYPE_OBS_ASSIST			(obs_assist_get_type())
#define OBS_ASSIST(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_OBS_ASSIST, ObsAssist))
#define OBS_ASSIST_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_OBS_ASSIST, ObsAssistClass))
#define IS_OBS_ASSIST(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_OBS_ASSIST))
#define IS_OBS_ASSIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_OBS_ASSIST))
#define OBS_ASSIST_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_OBS_ASSIST, ObsAssistClass))

typedef struct _ObsAssist	ObsAssist;
typedef struct _ObsAssistClass	ObsAssistClass;
typedef struct _ObsAssistConfig	ObsAssistPrivate;


struct _ObsAssist {
	GtkBox parent;
	ObsAssistPrivate *cfg;
};

struct _ObsAssistClass {
	GtkBoxClass parent_class;
};


GtkWidget *obs_assist_new(void);


#endif /* _WIDGETS_OBS_ASSIST_H_ */

