/**
 * @file    widgets/telescope/telescope_coord_ctrl.c
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


#include <telescope.h>
#include <telescope_cfg.h>

#include <default_grid.h>
#include <desclabel.h>
#include <coordinates.h>



/**
 * @brief hide label on realization, it is only shown if coordinates are
 *	  not observable from the telescope's location
 */

static void telescope_coord_not_vis_lbl_realize(GtkWidget *w, gpointer user_data)
{
	gtk_widget_hide(w);
}


/**
 * @brief update the horizontal reference if any other coordinate input mode
 *        is selected
 */

void telescope_update_azel_internal(Telescope *p)
{
	struct coord_horizontal hor;
	struct coord_equatorial	equ = {0};
	struct coord_galactic gal = {0};

	gtk_widget_show(GTK_WIDGET(p->cfg->not_vis_lbl));

	switch (gtk_combo_box_get_active(p->cfg->coord_ref_cb)) {
	case 0:
		gtk_widget_hide(GTK_WIDGET(p->cfg->not_vis_lbl));
		return;
	case 1:
		equ.ra  = gtk_spin_button_get_value(p->cfg->sb_ra_glat);
		equ.dec = gtk_spin_button_get_value(p->cfg->sb_de_glon);

		hor = equatorial_to_horizontal(equ, p->cfg->lat, p->cfg->lon,
					       0.0);
		break;
	case 2:
		gal.lat = gtk_spin_button_get_value(p->cfg->sb_ra_glat);
		gal.lon = gtk_spin_button_get_value(p->cfg->sb_de_glon);

		hor = galactic_to_horizontal(gal, p->cfg->lat, p->cfg->lon,
					     0.0);
		break;
	default:
		g_warning("Unknown coord. ref. in %s:%d", __func__, __LINE__);
		break;
	};


	hor.az = fmod(hor.az, 360.0);

	if (p->cfg->az_min != p->cfg->az_max) {
		if (hor.az < p->cfg->az_min || hor.az > p->cfg->az_max)
			return;
	}

	if (hor.el < p->cfg->el_min || hor.el > p->cfg->el_max)
		return;


	/* we're good, update azel spin buttons and hide warning */
	gtk_widget_hide(GTK_WIDGET(p->cfg->not_vis_lbl));

	gtk_spin_button_set_value(p->cfg->sb_az, hor.az);
	gtk_spin_button_set_value(p->cfg->sb_el, hor.el);

	return;
}


static gint telescope_coord_value_changed(GtkSpinButton *sb, Telescope *p)
{
	telescope_update_azel_internal(p);
	return TRUE;
}



/**
 * @brief hide EQU/GAL and show HOR
 */

static void telescope_switch_to_hor(Telescope *p)
{
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_ra_glat));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_de_glon));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_az));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_el));

	gtk_label_set_text(p->cfg->sb_az_ra_glat_lbl, "Azimuth");
	gtk_label_set_text(p->cfg->sb_el_de_glon_lbl, "Elevation");

	/* horizontal is always valid, hide */
	gtk_widget_hide(GTK_WIDGET(p->cfg->not_vis_lbl));
}


/**
 * @brief hide HOR and switch to EQU
 */

static void telescope_switch_to_equ(Telescope *p)
{
	GtkSpinButton *sb;

	struct coord_horizontal hor;
	struct coord_equatorial	equ;

	const GCallback cb = G_CALLBACK(telescope_coord_value_changed);



	gtk_widget_show(GTK_WIDGET(p->cfg->sb_ra_glat));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_de_glon));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_az));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_el));

	gtk_label_set_text(p->cfg->sb_az_ra_glat_lbl, "Right Ascension");
	gtk_label_set_text(p->cfg->sb_el_de_glon_lbl, "Declination");


	hor.az = gtk_spin_button_get_value(p->cfg->sb_az);
	hor.el = gtk_spin_button_get_value(p->cfg->sb_el);

	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);

	g_message("> EQU: %g %g to %g %g", hor.az, hor.el, equ.ra, equ.dec);

	g_signal_handlers_block_matched(p->cfg->sb_ra_glat,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	g_signal_handlers_block_matched(p->cfg->sb_de_glon,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	sb = p->cfg->sb_ra_glat;
	gtk_spin_button_set_range(sb, 0.0, 24.0);
	gtk_spin_button_set_value(sb, equ.ra);

	sb = p->cfg->sb_de_glon;
	gtk_spin_button_set_range(sb, -90.0, 90.0);
	gtk_spin_button_set_value(sb, equ.dec);

	g_signal_handlers_unblock_matched(p->cfg->sb_ra_glat,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
	g_signal_handlers_unblock_matched(p->cfg->sb_de_glon,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);

	/* horizontal is always valid, hide */
	gtk_widget_hide(GTK_WIDGET(p->cfg->not_vis_lbl));
}


/**
 * @brief hide HOR and switch to GAL
 */

static void telescope_switch_to_gal(Telescope *p)
{
	GtkSpinButton *sb;

	struct coord_horizontal hor;
	struct coord_galactic gal;

	const GCallback cb = G_CALLBACK(telescope_coord_value_changed);


	gtk_widget_show(GTK_WIDGET(p->cfg->sb_ra_glat));
	gtk_widget_show(GTK_WIDGET(p->cfg->sb_de_glon));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_az));
	gtk_widget_hide(GTK_WIDGET(p->cfg->sb_el));


	gtk_label_set_text(p->cfg->sb_az_ra_glat_lbl, "Latitude");
	gtk_label_set_text(p->cfg->sb_el_de_glon_lbl, "Longitude");


	hor.az = gtk_spin_button_get_value(p->cfg->sb_az);
	hor.el = gtk_spin_button_get_value(p->cfg->sb_el);

	gal = horizontal_to_galactic(hor, p->cfg->lat, p->cfg->lon);

	g_message("> GAL: %g %g to %g %g", hor.az, hor.el, gal.lat, gal.lon);

	g_signal_handlers_block_matched(p->cfg->sb_ra_glat,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);
	g_signal_handlers_block_matched(p->cfg->sb_de_glon,
					G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					cb, NULL);

	sb = p->cfg->sb_ra_glat;
	gtk_spin_button_set_range(sb, -90.0, 90.0);
	gtk_spin_button_set_value(sb, gal.lat);

	sb = p->cfg->sb_de_glon;
	gtk_spin_button_set_range(sb, 0., 360.);
	gtk_spin_button_set_value(sb, gal.lon);

	g_signal_handlers_unblock_matched(p->cfg->sb_ra_glat,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);
	g_signal_handlers_unblock_matched(p->cfg->sb_de_glon,
					  G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					  cb, NULL);

	/* horizontal is always valid, hide */
	gtk_widget_hide(GTK_WIDGET(p->cfg->not_vis_lbl));
}



static void telescope_coord_ref_changed(GtkComboBox *cb, Telescope *p)
{
	switch (gtk_combo_box_get_active(cb)) {
	case 0:
		telescope_switch_to_hor(p);
		break;
	case 1:
		telescope_switch_to_equ(p);
		break;
	case 2:
		telescope_switch_to_gal(p);
		break;
	default:
		break;
	};
}


/**
 * @brief create the celestial reference label and selector
 */

static void telescope_create_ref_selector(GtkGrid *grid, Telescope *p)
{
	GtkWidget *w;

	w = gui_create_desclabel("Celestial Coordinate System",
				 "Configure the input coordinate system for "
				 "the on-sky position of the telescope.");

	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 3);

	w = gtk_label_new("Reference");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 2, 0, 1, 1);

	w = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "HOR", "Horizontal");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "EQU", "Equatorial");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w), "GAL", "Galactic");

	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);

	gtk_grid_attach(GTK_GRID(grid), w, 3, 0, 1, 1);

	g_signal_connect(GTK_COMBO_BOX(w), "changed",
			 G_CALLBACK(telescope_coord_ref_changed), p);

	p->cfg->coord_ref_cb = GTK_COMBO_BOX(w);
}


static void telescope_create_coord_input(GtkGrid *grid, Telescope *p)
{
	GtkWidget *w;
	GtkWidget *tmp;

	gchar *lbl;


	w = gui_create_desclabel("Telescope Position",
				 "Configure the on-sky position of the "
				 "telescope.");
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_widget_set_hexpand(w, TRUE);
	gtk_grid_attach(GTK_GRID(grid), w, 0, 3, 1, 3);


	w = gtk_label_new("Azimuth");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 2, 4, 1, 1);
	p->cfg->sb_az_ra_glat_lbl = GTK_LABEL(w);

	/* AZ/RA/GLON spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), 0.0, 360.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 0.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 4, 1, 1);
	p->cfg->sb_az = GTK_SPIN_BUTTON(w);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(telescope_coord_value_changed), p);

	/* spin button is in same place */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 0.1, 1.0);

	gtk_grid_attach(grid, w, 3, 4, 1, 1);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(telescope_coord_value_changed), p);

	p->cfg->sb_ra_glat = GTK_SPIN_BUTTON(w);


	w = gtk_label_new("Elevation");
	gtk_label_set_xalign(GTK_LABEL(w), 0.0);
	gtk_grid_attach(grid, w, 2, 5, 1, 1);
	p->cfg->sb_el_de_glon_lbl = GTK_LABEL(w);

	/* EL/DE/GLAT spin button */
	w = gtk_spin_button_new(NULL, 1.2, 4);

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(w), 0.0, 90.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), 0.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), TRUE);
	gtk_grid_attach(grid, w, 3, 5, 1, 1);
	p->cfg->sb_el = GTK_SPIN_BUTTON(w);

	/* spin button is in same place */
	w = gtk_spin_button_new(NULL, 1.2, 4);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w), TRUE);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(w), 0.1, 1.0);
	gtk_grid_attach(grid, w, 3, 5, 1, 1);
	g_signal_connect(GTK_SPIN_BUTTON(w), "value-changed",
			 G_CALLBACK(telescope_coord_value_changed), p);
	p->cfg->sb_de_glon = GTK_SPIN_BUTTON(w);


	tmp = gtk_label_new(NULL);
	lbl = g_strdup_printf("<span foreground='#E1370F' size = 'small'> "
			      "Specified coordinates not visible.</span>");
	gtk_label_set_markup(GTK_LABEL(tmp), lbl);

	gtk_label_set_xalign(GTK_LABEL(tmp), 0.0);
	gtk_grid_attach(grid, tmp, 2, 7, 2, 1);
	g_signal_connect(tmp, "realize",
			 G_CALLBACK(telescope_coord_not_vis_lbl_realize), NULL);
	p->cfg->not_vis_lbl = GTK_LABEL(tmp);
}




/**
 * @brief create telescope coordinate controls
 */

GtkWidget *telescope_coord_ctrl_new(Telescope *p)
{
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	telescope_create_ref_selector(grid, p);
	telescope_create_coord_input(grid, p);


	return GTK_WIDGET(grid);
}
