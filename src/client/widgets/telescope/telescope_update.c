/**
 * @file    widgets/telescope/telescope_update.c
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
 * @brief functions to handle configuration and label updates
 *
 */

#include <telescope.h>
#include <telescope_cfg.h>


/**
 * @brief update ranges in AZ/RA/GLON spin button
 */

static void telescope_update_sb_az_ra_glon(Telescope *p)
{
	GtkSpinButton *b;


	b = p->cfg->sb_az;

	p->cfg->az_min = (gdouble) p->cfg->c.az_min_arcsec / 3600.0;
	p->cfg->az_max = (gdouble) p->cfg->c.az_max_arcsec / 3600.0;
	p->cfg->az_res = (gdouble) p->cfg->c.az_res_arcsec / 3600.0;

	if (p->cfg->az_min != p->cfg->az_max)
		gtk_spin_button_set_range(b, p->cfg->az_min, p->cfg->az_max);
	else
		gtk_spin_button_set_range(b, 0.0, 360.0);	/* full range available */

	gtk_spin_button_set_increments(b,
				       p->cfg->az_res,
				       p->cfg->az_res * 10.0);

	/* force update, snaps value to ticks if inc changed */
	gtk_spin_button_update(b);
}


/**
 * @brief update ranges in EL/DE/GLAT spin button
 */

static void telescope_update_sb_el_de_glat(Telescope *p)
{
	GtkSpinButton *b;


	b =p->cfg->sb_el;

	p->cfg->el_min = (gdouble) p->cfg->c.el_min_arcsec / 3600.0;
	p->cfg->el_max = (gdouble) p->cfg->c.el_max_arcsec / 3600.0;
	p->cfg->el_res = (gdouble) p->cfg->c.el_res_arcsec / 3600.0;

	gtk_spin_button_set_range(b, p->cfg->el_min, p->cfg->el_max);
	gtk_spin_button_set_increments(b,
				       p->cfg->el_res,
				       p->cfg->el_res * 10.0);

	/* force update, snaps value to ticks if inc changed */
	gtk_spin_button_update(b);
}




/**
 * @brief update the movement ranges for the telescope
 */

void telescope_update_movement_range(Telescope *p)
{
	telescope_update_sb_az_ra_glon(p);
	telescope_update_sb_el_de_glat(p);

}
