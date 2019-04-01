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


	GArray		       *hst_idx;
	GArray		       *hst_pwr;
	void                   *r_hst;
	gsize                   n_hst;
	gsize			idx;
	enum xyplot_graph_style s_hst;
	GdkRGBA                 c_hst;

	guint id_spd;
};


#endif /* _WIDGETS_HISTORY_CFG_H_ */
