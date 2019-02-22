/**
 * @file    widgets/telescope/telescope.c
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
 * @brief a widget to control the telescope position
 *
 */


#include <telescope.h>
#include <telescope_cfg.h>
#include <telescope_internal.h>

#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <cmd.h>
#include <math.h>


G_DEFINE_TYPE_WITH_PRIVATE(Telescope, telescope, GTK_TYPE_BOX)



/**
 * @brief handle capabilities data
 */

static void telescope_handle_pr_capabilities(gpointer instance,
					     const struct capabilities *c,
					     gpointer data)
{
	Telescope *p;


	p = TELESCOPE(data);

	p->cfg->c = (*c);

	p->cfg->lat = p->cfg->c.lat_arcsec / 3600.0;
	p->cfg->lon = p->cfg->c.lon_arcsec / 3600.0;

	telescope_update_movement_range(p);
}



static void gui_create_telescope_controls(Telescope *p)
{
	GtkWidget *w;

	w = telescope_get_pos_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = telescope_coord_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = telescope_pos_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = telescope_park_ctrl_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = telescope_recal_pointing_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

	w = telescope_track_sky_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);

}



/**
 * @brief initialise the Telescope class
 */

static void telescope_class_init(TelescopeClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the Telescope widget
 */

static void telescope_init(Telescope *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_TELESCOPE(p));

	p->cfg = telescope_get_instance_private(p);

	/* initialise with nosensical values */
	p->cfg->track_ra = DBL_MIN;
	p->cfg->track_de = DBL_MIN;

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_telescope_controls(p);

	g_signal_connect(sig_get_instance(), "pr-capabilities",
			 G_CALLBACK(telescope_handle_pr_capabilities),
			 (void *) p);

#if 0
	g_signal_connect(sig_get_instance(), "pr-spec-acq-cfg",
			 G_CALLBACK(telescope_handle_pr_spec_acq_cfg),
			 (void *) p);
#endif
}


/**
 * @brief create a new Telescope widget
 */

GtkWidget *telescope_new(void)
{
	Telescope *telescope;


	telescope = g_object_new(TYPE_TELESCOPE, NULL);

	return GTK_WIDGET(telescope);
}
