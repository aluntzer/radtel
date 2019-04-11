/**
 * @file    widgets/spectrum/spectrum_cfg.h
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

#ifndef _WIDGETS_SPECTRUM_CFG_H_
#define _WIDGETS_SPECTRUM_CFG_H_

#include <spectrum.h>
#include <cmd.h>
#include <xyplot.h>
#include <gio/gio.h>

struct spectrum {
	gdouble *x;
	gdouble *y;
	gsize    n;
};

struct fitdata {
	GArray *frq;
	GArray *amp;

	void *plt_ref_in;
	void *plt_ref_out;

	GtkLabel *fitpar;
};


struct _SpectrumConfig {

	GtkWidget *plot;

	GList                    *per;
	void                   *r_per;
	gsize                   n_per;
	enum xyplot_graph_style s_per;
	GdkRGBA                 c_per;

	GList                    *avg;
	void                   *r_avg;
	gsize                   n_avg;
	enum xyplot_graph_style s_avg;
	GdkRGBA                 c_avg;

	GtkSwitch *sw_acq;

	struct fitdata fit;
	struct spec_acq_cfg acq;

	guint id_spd;
	guint id_acq;
	guint id_ena;
	guint id_dis;
	guint id_cfg;
};


#endif /* _WIDGETS_SPECTRUM_CFG_H_ */
