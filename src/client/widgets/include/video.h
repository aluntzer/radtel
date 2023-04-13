/**
 * @file    widgets/includes/video.h
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

#ifndef _WIDGETS_VIDEO_H_
#define _WIDGETS_VIDEO_H_


#include <gtk/gtk.h>


#define TYPE_VIDEO		(video_get_type())
#define VIDEO(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_VIDEO, Video))
#define VIDEO_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_VIDEO, VideoClass))
#define IS_VIDEO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_VIDEO))
#define IS_VIDEO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_VIDEO))
#define VIDEO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_VIDEO, VideoClass))

typedef struct _Video	Video;
typedef struct _VideoClass	VideoClass;
typedef struct _VideoConfig	VideoPrivate;


struct _Video {
	GtkDrawingArea parent;
	VideoPrivate *cfg;
};

struct _VideoClass {
	GtkDrawingAreaClass parent_class;
};


GtkWidget *video_new(void);


#endif /* _WIDGETS_VIDEO_H_ */

