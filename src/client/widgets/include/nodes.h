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
static const GdkRGBA node_purple     = {0.494, 0.184, 0.557, 1.0};



struct nodes_point {
	gdouble p0;
	gdouble p1;
};

struct nodes_coordinate {
	gdouble c1;
	gdouble c2;
	enum {HOR, EQU, GAL} coord_type;
};


#define KEY_INT		0x00000001
#define KEY_DOUBLE	0x00000002
#define KEY_POINTS	0x00000003
#define KEY_COORDINATES	0x00000004


#define COL_BLINK	node_green
#define COL_DOUBLE	node_light_blue
#define COL_POINTS	node_orange
#define COL_COORDINATES	node_purple


/* generic nodes */
GtkWidget *pulse_new(void);
GtkWidget *step_new(void);
GtkWidget *medfilt_new(void);


/* special nodes */
GtkWidget *specsrc_new(void);
GtkWidget *plot_new(void);
GtkWidget *coordinates_new(void);
GtkWidget *target_new(void);


#include <gtknode.h>


#define TYPE_COORDINATES		(coordinates_get_type())
#define COORDINATES(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_COORDINATES, Coordinates))
#define COORDINATES_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_COORDINATES, CoordinatesClass))
#define IS_COORDINATES(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_COORDINATES))
#define IS_COORDINATES_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_COORDINATES))
#define COORDINATES_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_COORDINATES, CoordinatesClass))

typedef struct _Coordinates		Coordinates;
typedef struct _CoordinatesPrivate	CoordinatesPrivate;
typedef struct _CoordinatesClass	CoordinatesClass;

struct _Coordinates {
	GtkNodesNode parent;
	CoordinatesPrivate *cfg;
};

struct _CoordinatesClass {
	GtkNodesNodeClass parent_class;
};


#define TYPE_MEDFILT		(medfilt_get_type())
#define MEDFILT(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_MEDFILT, Medfilt))
#define MEDFILT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_MEDFILT, MedfiltClass))
#define IS_MEDFILT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_MEDFILT))
#define IS_MEDFILT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_MEDFILT))
#define MEDFILT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_MEDFILT, MedfiltClass))

typedef struct _Medfilt		Medfilt;
typedef struct _MedfiltPrivate	MedfiltPrivate;
typedef struct _MedfiltClass	MedfiltClass;

struct _Medfilt {
	GtkNodesNode parent;
	MedfiltPrivate *cfg;
};

struct _MedfiltClass {
	GtkNodesNodeClass parent_class;
};

#define TYPE_PLOT		(plot_get_type())
#define PLOT(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_PLOT, Plot))
#define PLOT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_PLOT, PlotClass))
#define IS_PLOT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_PLOT))
#define IS_PLOT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_PLOT))
#define PLOT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_PLOT, PlotClass))

typedef struct _Plot		Plot;
typedef struct _PlotPrivate	PlotPrivate;
typedef struct _PlotClass	PlotClass;

struct _Plot {
	GtkNodesNode parent;
	PlotPrivate *cfg;
};

struct _PlotClass {
	GtkNodesNodeClass parent_class;
};


#define TYPE_PULSE		(pulse_get_type())
#define PULSE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_PULSE, Pulse))
#define PULSE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_PULSE, PulseClass))
#define IS_PULSE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_PULSE))
#define IS_PULSE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_PULSE))
#define PULSE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_PULSE, PulseClass))

typedef struct _Pulse		Pulse;
typedef struct _PulsePrivate	PulsePrivate;
typedef struct _PulseClass	PulseClass;

struct _Pulse {
	GtkNodesNode parent;
	PulsePrivate *cfg;
};

struct _PulseClass {
	GtkNodesNodeClass parent_class;
};


#define TYPE_SPECSRC		(specsrc_get_type())
#define SPECSRC(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SPECSRC, Specsrc))
#define SPECSRC_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_SPECSRC, SpecsrcClass))
#define IS_SPECSRC(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_SPECSRC))
#define IS_SPECSRC_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_SPECSRC))
#define SPECSRC_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_SPECSRC, SpecsrcClass))

typedef struct _Specsrc		Specsrc;
typedef struct _SpecsrcPrivate	SpecsrcPrivate;
typedef struct _SpecsrcClass	SpecsrcClass;

struct _Specsrc {
	GtkNodesNode parent;
	SpecsrcPrivate *cfg;
};

struct _SpecsrcClass {
	GtkNodesNodeClass parent_class;
};


#define TYPE_STEP		(step_get_type())
#define STEP(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_STEP, Step))
#define STEP_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_STEP, StepClass))
#define IS_STEP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_STEP))
#define IS_STEP_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_STEP))
#define STEP_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_STEP, StepClass))

typedef struct _Step		Step;
typedef struct _StepPrivate	StepPrivate;
typedef struct _StepClass	StepClass;

struct _Step {
	GtkNodesNode parent;
	StepPrivate *cfg;
};

struct _StepClass {
	GtkNodesNodeClass parent_class;
};

#define TYPE_TARGET		(target_get_type())
#define TARGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TARGET, Target))
#define TARGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_TARGET, TargetClass))
#define IS_TARGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TARGET))
#define IS_TARGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_TARGET))
#define TARGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_TARGET, TargetClass))

typedef struct _Target		Target;
typedef struct _TargetPrivate	TargetPrivate;
typedef struct _TargetClass	TargetClass;

struct _Target {
	GtkNodesNode parent;
	TargetPrivate *cfg;
};

struct _TargetClass {
	GtkNodesNodeClass parent_class;
};


#endif /* _WIDGETS_INCLUDE_NODES_H_ */
