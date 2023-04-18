/**
 * @file    widgets/video/video_cfg.h
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

#ifndef _WIDGETS_VIDEO_CFG_H_
#define _WIDGETS_VIDEO_CFG_H_

#include <video.h>

struct _VideoConfig {


	GstElement *playbin;
	GstState state;

	gchar *uri;

	guint id_uri;

};




#endif /* _WIDGETS_VIDEO_CFG_H_ */
