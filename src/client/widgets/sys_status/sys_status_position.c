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


	dd = trunc(p->cfg->az);
	mm = trunc((p->cfg->az - dd) * 60.0);
	ss = ((p->cfg->az - dd) * 60.0 - mm) * 60.0;
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt>%3.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_az, lbl);

	dd = trunc(p->cfg->el);
	mm = trunc(((p->cfg->el - dd) * 60.0));
	ss = trunc(((p->cfg->el - dd) * 60.0 - mm) * 60.0);
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

	hh = trunc(equ.ra);
	mm = trunc(((equ.ra - hh) * 60.0));
	ss = trunc(((equ.ra - hh) * 60.0 - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt> %02.0fh %02.0fm %05.2fs</tt>", hh, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_ra, lbl);


	dd = trunc(equ.dec);
	mm = trunc(((equ.dec - dd) * 60.));
	ss = trunc(((equ.dec - dd) * 60. - mm) * 60.0);
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

	dd = trunc(gal.lat);
	mm = trunc(((gal.lat - dd) * 60.0));
	ss = trunc(((gal.lat - dd) * 60. - mm) * 60.0);
	mm = fabs(mm);
	ss = fabs(ss);
        lbl = g_strdup_printf("<tt> %02.0f° %02.0f' %05.2f\"</tt>", dd, mm, ss);
        gtk_label_set_markup(p->cfg->lbl_glat, lbl);

	dd = trunc(gal.lon);
	mm = trunc(((gal.lon - dd) * 60.));
	ss = trunc(((gal.lon - dd) * 60. - mm) * 60.0);
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

	/** XXX separate function! */
	gchar *lbl = NULL;

	if (p->cfg->eta_acq > 0.0) {
	        lbl = g_strdup_printf("<tt> %03.2fs</tt>", p->cfg->eta_acq);
		gtk_label_set_markup(p->cfg->lbl_eta_acq, lbl);
		p->cfg->eta_acq = p->cfg->eta_acq - 1.0; /* 1 sec updates */
		g_free(lbl);
	} else {
		gtk_label_set_text(p->cfg->lbl_eta_acq, "");
	}

	if (p->cfg->eta_rec > 0.0) {
		gboolean  active;
	        lbl = g_strdup_printf("<tt> %03.2fs</tt>", p->cfg->eta_rec);
		gtk_label_set_markup(p->cfg->lbl_eta_rec, lbl);

		/* ugly temporary hack: update only when slew spinner is active
		 * (i.e. telescope actually moving */
		g_object_get(G_OBJECT(p->cfg->spin_acq), "active", &active, NULL);
		if (active)
			p->cfg->eta_rec = p->cfg->eta_rec - 1.0; /* 1 sec updates */
		g_free(lbl);
	} else {
		gtk_label_set_text(p->cfg->lbl_eta_rec, "");
	}


	if (p->cfg->eta_slew > 0.0) {
	        lbl = g_strdup_printf("<tt> %03.2fs</tt>", p->cfg->eta_slew);
		gtk_label_set_markup(p->cfg->lbl_eta_slew, lbl);
		p->cfg->eta_slew = p->cfg->eta_slew - 1.0; /* 1 sec updates */
		g_free(lbl);
	} else {
		gtk_label_set_text(p->cfg->lbl_eta_slew, "");
	}


	if (p->cfg->eta_move > 0.0) {
		gboolean  active;
	        lbl = g_strdup_printf("<tt> %03.2fs</tt>", p->cfg->eta_move);
		gtk_label_set_markup(p->cfg->lbl_eta_move, lbl);

		/* ugly temporary hack: update only when slew spinner is active
		 * (i.e. telescope actually moving */
		g_object_get(G_OBJECT(p->cfg->spin_slew), "active", &active, NULL);
		if (active)
			p->cfg->eta_move = p->cfg->eta_move - 1.0; /* 1 sec updates */
		g_free(lbl);
	} else {
		gtk_label_set_text(p->cfg->lbl_eta_move, "");
	}





	return G_SOURCE_CONTINUE;
}


GtkWidget *sys_status_create_align_lbl(const gchar *str, double align)
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


	return GTK_WIDGET(grid);
}

