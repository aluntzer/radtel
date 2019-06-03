/**
 * @file    widgets/sky/sky_cfg.h
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

#ifndef _WIDGETS_SKY_CFG_H_
#define _WIDGETS_SKY_CFG_H_

#include <sky.h>
#include <coordinates.h>

/**
 * a sky catalog object
 */

struct sky_obj {

	gchar *name;

	float x;
	float y;

	gdouble radius;
	gboolean selected;

	struct coord_equatorial eq;
	struct coord_horizontal hor;
};


struct _SkyConfig {
	cairo_surface_t *plot;		/* the plotting surface */
	cairo_surface_t *render;	/* the rendered surface */

	GList *obj;			/* list of sky objects */

	GTimer *timer;
	gdouble refresh;

	struct {			/* reset button */
		gdouble x0, x1;
		gdouble y0, y1;
	} rst;

	struct {			/* mouse pointer position */
		gdouble x;
		gdouble y;
		gboolean inside;
	} mptr;


	gdouble lat;	/* telescope location */
	gdouble lon;

	gdouble az_res;	/* azel step resolution */
	gdouble el_res;

	struct sky_obj *sel;		/* the currently selected sky object */

	struct coord_horizontal pos;	/* telescope current pointing */
	struct coord_horizontal tgt;	/* telescope target pointing */

	struct coord_horizontal lim[2];	/* telescope axis limits */

	guint32 n_local_hor;
	struct local_horizon *local_hor; /* local horizon */


	gdouble xc;			/* sky grid x center */
	gdouble yc;			/* sky grid y center */
	gdouble r;			/* sky grid radius   */

	gdouble width;
	gdouble height;

	gdouble mb3_x;		/* last mouse button 3 click coordinate */
	gdouble time_off;	/* current time offset */


	guint id_to;

	guint id_cap;
	guint id_pos;
	guint id_tgt;
	guint id_trk;
	guint id_con;
};




#endif /* _WIDGETS_SKY_CFG_H_ */
