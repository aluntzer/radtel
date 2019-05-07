/**
 * @file    widgets/history/history_cfg.h
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

#ifndef _WIDGETS_HISTORY_CFG_H_
#define _WIDGETS_HISTORY_CFG_H_

#include <history.h>
#include <cmd.h>
#include <xyplot.h>

struct _HistoryConfig {

	GtkWidget *plot;

	GTimer		       *timer;

	GArray		       *hst_idx;
	GArray		       *hst_pwr;
	void                   *r_hst;
	void                   *r_lst;
	gsize                   n_hst;
	enum xyplot_graph_style s_hst;
	GdkRGBA                 c_hst;

	GdkPixbuf	       *wf_pb;
	GtkDrawingArea	       *wf_da;

	GtkScale               *s_lo;
	GtkScale               *s_hi;

	GtkScale               *s_min;

	gdouble th_lo;
	gdouble th_hi;

	gdouble refresh;

	gint wf_n;
	gint wf_n_max;

	gdouble wf_min;

	gdouble wf_av_min;
	gdouble wf_av_max;

	guint id_spd;
};


#endif /* _WIDGETS_HISTORY_CFG_H_ */
