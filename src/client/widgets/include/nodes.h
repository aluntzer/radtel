/**
 * @file    widgets/include/nodes.h
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

#ifndef _WIDGETS_INCLUDE_NODES_H_
#define _WIDGETS_INCLUDE_NODES_H_

#include <gtk/gtk.h>


static const GdkRGBA node_red        = {0.635, 0.078, 0.184, 1.0};
static const GdkRGBA node_orange     = {0.851, 0.325, 0.098, 1.0};
static const GdkRGBA node_green      = {0.467, 0.675, 0.188, 1.0};
static const GdkRGBA node_light_blue = {0.302, 0.745, 0.933, 1.0};



struct nodes_point {
	gdouble p0;
	gdouble p1;
};



#define KEY_INT		0x00000001
#define KEY_DOUBLE	0x00000002
#define KEY_POINTS	0x00000003


#define COL_BLINK	node_green
#define COL_DOUBLE	node_light_blue
#define COL_POINTS	node_orange


/* generic nodes */
GtkWidget *node_pulse_new(void);
GtkWidget *node_step_new(void);
GtkWidget *node_medfilt_new(void);


/* special nodes */
GtkWidget *node_spec_src_new(void);
GtkWidget *node_plot_new(void);



#endif /* _WIDGETS_INCLUDE_NODES_H_ */
