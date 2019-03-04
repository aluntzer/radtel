/**
 * @file    widgets/sys_status/sys_status_position.c
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

#include <sys_status.h>
#include <sys_status_cfg.h>

#include <default_grid.h>
#include <signals.h>
#include <coordinates.h>


static void sys_status_update_hor_lbl(SysStatus *p)
{
	gdouble dd, mm, ss;

	gchar *lbl;


	dd = round(p->cfg->az);
	mm = round((p->cfg->az - dd) * 60.0);
	ss = ((p->cfg->az - dd) * 60.0 - mm) * 60.0;
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt>%3.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_az, lbl);

	dd = round(p->cfg->el);
	mm = round(((p->cfg->el - dd) * 60.0));
	ss = round(((p->cfg->el - dd) * 60.0 - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt> %02.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_el, lbl);
}



static void sys_status_update_equ_lbl(SysStatus *p)
{
	gdouble hh, dd, mm, ss;

	gchar *lbl;

	struct coord_horizontal hor;
	struct coord_equatorial	equ;



	hor.az = p->cfg->az;
	hor.el = p->cfg->el;

	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);

	hh = round(equ.ra);
	mm = round(((equ.ra - hh) * 60.0));
	ss = round(((equ.ra - hh) * 60.0 - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt> %02.0fh %02.0fm %05.2fs</tt>", hh, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_ra, lbl);


	dd = round(equ.dec);
	mm = round(((equ.dec - dd) * 60.));
	ss = round(((equ.dec - dd) * 60. - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt> %02.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_de, lbl);
}


static void sys_status_update_gal_lbl(SysStatus *p)
{
	gdouble dd, mm, ss;

	gchar *lbl;

	struct coord_horizontal hor;
	struct coord_galactic   gal;


	hor.az = p->cfg->az;
	hor.el = p->cfg->el;

	gal = horizontal_to_galactic(hor, p->cfg->lat, p->cfg->lon);

	dd = round(gal.lat);
	mm = round(((gal.lat - dd) * 60.0));
	ss = round(((gal.lat - dd) * 60. - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt> %02.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_glat, lbl);

	dd = round(gal.lon);
	mm = round(((gal.lon - dd) * 60.));
	ss = round(((gal.lon - dd) * 60. - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt>%3.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_glon, lbl);
}




static gboolean sys_status_gepos_azel_cb(gpointer instance, struct getpos *pos,
					  SysStatus *p)
{
	p->cfg->az = (gdouble) pos->az_arcsec / 3600.0;
	p->cfg->el = (gdouble) pos->el_arcsec / 3600.0;


	sys_status_update_hor_lbl(p);
	sys_status_update_equ_lbl(p);
	sys_status_update_gal_lbl(p);

	return TRUE;
}


/**
 * @brief callback to update current telescope position
 */

static gboolean sys_status_timeout_cb(void *data)
{
	SysStatus *p;

	p = (SysStatus *) data;


	sys_status_update_equ_lbl(p);
	sys_status_update_gal_lbl(p);


	return G_SOURCE_CONTINUE;
}


static GtkWidget *sys_status_create_align_lbl(const gchar *str, double align)
{
	GtkWidget *w;


	w = gtk_label_new(NULL);
	if (str)
		gtk_label_set_markup(GTK_LABEL(w), str);
	gtk_label_set_xalign(GTK_LABEL(w), align);

	return w;
}


GtkWidget *sys_status_pos_new(SysStatus *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	grid = GTK_GRID(gtk_grid_new());
	gtk_grid_set_row_spacing(grid, 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 12);




	w = sys_status_create_align_lbl("<span alpha='50%'>Azimuth</span>",
					1.0);
	gtk_grid_attach(grid, w, 0, 0, 1, 1);

	w = sys_status_create_align_lbl(NULL, 0.0);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	p->cfg->lbl_az = GTK_LABEL(w);

	w = sys_status_create_align_lbl("<span alpha='50%'>Elevation</span>",
					1.0);
	gtk_label_set_xalign(GTK_LABEL(w), 1.0);
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	w = sys_status_create_align_lbl(NULL, 0.0);
	gtk_grid_attach(grid, w, 1, 1, 1, 1);
	p->cfg->lbl_el = GTK_LABEL(w);

	w = sys_status_create_align_lbl("<span alpha='50%'>Right Ascension"
					"</span>", 1.0);
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	w = sys_status_create_align_lbl(NULL, 0.0);
	gtk_grid_attach(grid, w, 1, 2, 1, 1);
	p->cfg->lbl_ra = GTK_LABEL(w);

	w = sys_status_create_align_lbl("<span alpha='50%'>Declination</span>",
					1.0);
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	w = sys_status_create_align_lbl(NULL, 0.0);
	gtk_grid_attach(grid, w, 1, 3, 1, 1);
	p->cfg->lbl_de = GTK_LABEL(w);


	w = sys_status_create_align_lbl("<span alpha='50%'>Galactic Latitude"
					"</span>", 1.0);
	gtk_grid_attach(grid, w, 0, 4, 1, 1);

	w = sys_status_create_align_lbl(NULL, 0.0);
	gtk_grid_attach(grid, w, 1, 4, 1, 1);
	p->cfg->lbl_glat = GTK_LABEL(w);

	w = sys_status_create_align_lbl("<span alpha='50%'>Galactic Longitude"
					"</span>", 1.0);
	gtk_grid_attach(grid, w, 0, 5, 1, 1);

	w = sys_status_create_align_lbl(NULL, 0.0);
	gtk_grid_attach(grid, w, 1, 5, 1, 1);
	p->cfg->lbl_glon = GTK_LABEL(w);



	p->cfg->id_pos = g_signal_connect(sig_get_instance(), "pr-getpos-azel",
					  (GCallback) sys_status_gepos_azel_cb,
					  (gpointer) p);

	p->cfg->id_to = g_timeout_add_seconds(1, sys_status_timeout_cb, p);

	/* fetch configuration */
	cmd_capabilities(PKT_TRANS_ID_UNDEF);
	cmd_getpos_azel(PKT_TRANS_ID_UNDEF);



	return GTK_WIDGET(grid);
}

