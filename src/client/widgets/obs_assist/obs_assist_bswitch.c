/**
 * @file    widgets/obs_assist/obs_assist_bswitch.c
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
 * @brief a scan a rectangle in azimuth and elevation
 *
 *
 * XXX this really needs some refactoring. for now, it works
 *
 */


#include <obs_assist.h>
#include <obs_assist_cfg.h>
#include <obs_assist_internal.h>

#include <coordinates.h>
#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <xyplot.h>
#include <cmd.h>
#include <math.h>


static void bswitch_free(ObsAssist *p)
{
	g_free(p->cfg->bswitch.pos1.x);
	g_free(p->cfg->bswitch.pos1.y);
	g_free(p->cfg->bswitch.pos2.x);
	g_free(p->cfg->bswitch.pos2.y);
	g_free(p->cfg->bswitch.tgt.x);
	g_free(p->cfg->bswitch.tgt.y);

	p->cfg->bswitch.pos1.x = NULL;
	p->cfg->bswitch.pos1.y = NULL;
	p->cfg->bswitch.pos1.n = 0;
	p->cfg->bswitch.pos2.x = NULL;
	p->cfg->bswitch.pos2.y = NULL;
	p->cfg->bswitch.pos2.n = 0;
	p->cfg->bswitch.tgt.x  = NULL;
	p->cfg->bswitch.tgt.y  = NULL;
	p->cfg->bswitch.tgt.n  = 0;
}



static void bswitch_show_abort_msg(ObsAssist *p)
{
	GtkWidget *dia;
	GtkWindow * win;

	win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(p)));
	dia = gtk_message_dialog_new(win,
				     GTK_DIALOG_MODAL,
				     GTK_MESSAGE_ERROR,
				     GTK_BUTTONS_CLOSE,
				     "Number of data bins in the "
				     "spectral data changed.\n"
				     "This is currently unsupported. "
				     "Observation aborted.");

	gtk_dialog_run(GTK_DIALOG(dia));
	gtk_widget_destroy(dia);

	p->cfg->abort = TRUE;
}



/**
 * @brief update the repeat progress bar
 */

static void bswitch_update_pbar_rpt(ObsAssist *p)
{
	gdouble frac;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->bswitch.pbar);

	frac = (gdouble) p->cfg->bswitch.rpt_cur /
	       (gdouble) p->cfg->bswitch.n_rpt;

	str = g_strdup_printf("Cycle: %d of %d complete",
			      p->cfg->bswitch.rpt_cur,
			      p->cfg->bswitch.n_rpt);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}



/**
 * @brief clear and draw the continuum plot
 */

static void cross_draw_continuum(ObsAssist *p)
{
	gsize i;

	void *ref;

	gdouble *x;
	gdouble *y;

	gdouble avg = 0.0;


	/* update graph */
	xyplot_drop_all_graphs(p->cfg->bswitch.plt_cont);
	x = g_memdup(p->cfg->bswitch.idx->data,
		     p->cfg->bswitch.idx->len * sizeof(gdouble));
	y = g_memdup(p->cfg->bswitch.amp->data,
		     p->cfg->bswitch.amp->len * sizeof(gdouble));


	for (i = 0; i < p->cfg->bswitch.amp->len; i++)
		avg += y[i];

	avg /= (gdouble) p->cfg->bswitch.amp->len;


	ref = xyplot_add_graph(p->cfg->bswitch.plt_cont, x, y, NULL,
			       p->cfg->bswitch.idx->len,
			       g_strdup_printf("Continuum"));

	xyplot_draw_indicator_y(p->cfg->bswitch.plt_cont, avg,
				g_strdup_printf("AVG: %g [K]", avg));



	xyplot_set_graph_style(p->cfg->bswitch.plt_cont, ref, IMPULSES);
	xyplot_redraw(p->cfg->bswitch.plt_cont);
}


/**
 * @brief add a new sample to graph
 */

static void bswitch_add_graph(GtkWidget *plt, struct spectrum *sp)
{
	gdouble *x;
	gdouble *y;

	void *ref;


	/* update graph */
	x = g_memdup(sp->x, sp->n * sizeof(gdouble));
	y = g_memdup(sp->y, sp->n * sizeof(gdouble));

	ref = xyplot_add_graph(plt, x, y, NULL, sp->n,
			       g_strdup_printf("Sample"));

	xyplot_set_graph_style(plt, ref, SQUARES);

	xyplot_redraw(plt);
}


/**
 * @brief verify position and issue move command if necessary
 *
 * @param az the actual target Azimuth
 * @param el the actual target Elevation
 *
 * @returns TRUE if in position
 * @note we use 2x the axis resolution for tolerance to avoid sampling issues
 */

static gboolean bswitch_in_position(ObsAssist *p, gdouble az, gdouble el)
{
	gdouble d_az;
	gdouble d_el;

	const gdouble az_tol = 2.0 * p->cfg->az_res;
	const gdouble el_tol = 2.0 * p->cfg->el_res;


	d_az = fabs(az - p->cfg->az);
	d_el = fabs(el - p->cfg->el);

	if ((d_az > az_tol) || (d_el > el_tol)) {

		obs_assist_clear_spec(p);

		/* disable acquisition first */
		if (p->cfg->acq_enabled)
			cmd_spec_acq_disable(PKT_TRANS_ID_UNDEF);

		/* update position if telescope is not moving already */
		if (!p->cfg->moving)
			cmd_moveto_azel(PKT_TRANS_ID_UNDEF, az, el);

		return FALSE;
	}


	return TRUE;
}



/**
 * @brief take a measurement
 *
 * @brief returns TRUE if measurement was taken
 */

static gboolean bswitch_measure(ObsAssist *p)
{
	gsize i;

	static gint samples;

	GtkWidget *plt;

	struct spectrum *obs;

	static struct spectrum *sp;



	/* enable acquisition at current position */
	if (!p->cfg->acq_enabled) {
		cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		return FALSE;
	}

	/* has new spectral data arrived? */
	if (!p->cfg->spec.n)
		return FALSE;

	if (!sp) {
		sp    = (struct spectrum *) g_malloc(sizeof(struct spectrum));
		sp->x = g_memdup(p->cfg->spec.x, p->cfg->spec.n * sizeof(gdouble));
		sp->y = g_memdup(p->cfg->spec.y, p->cfg->spec.n * sizeof(gdouble));
		sp->n = p->cfg->spec.n;
	}


	samples++;

	/* stack */
	if (samples < p->cfg->bswitch.n_avg) {
		for (i = 0; i < sp->n; i++)
			sp->y[i] += p->cfg->spec.y[i];

		obs_assist_clear_spec(p);
		return FALSE;
	}


	/* done stacking, divide if necessary */

	if (samples > 1) {
		for (i = 0; i < sp->n; i++)
			sp->y[i] /= samples;
	}

	switch (p->cfg->bswitch.pos) {
	case OFF1:
		obs = &p->cfg->bswitch.pos1;
		plt = p->cfg->bswitch.plt_pos1;
		break;
	case OFF2:
		obs = &p->cfg->bswitch.pos2;
		plt = p->cfg->bswitch.plt_pos2;
		break;
	case TGT:
		obs = &p->cfg->bswitch.tgt;
		plt = p->cfg->bswitch.plt_tgt;
		break;
	default:
		/* should never happen -.- */
		goto cleanup;
	}

	/* initialize if necessary */
	if (!obs->n) {
		obs->x = g_memdup(p->cfg->spec.x,
				 p->cfg->spec.n * sizeof(gdouble));
		obs->y = g_memdup(p->cfg->spec.y,
				 p->cfg->spec.n * sizeof(gdouble));
		obs->n = p->cfg->spec.n;
	}


	/* we currently do not support changes to the number of bins */
	if ((obs->n != p->cfg->spec.n)) {
		bswitch_show_abort_msg(p);
		goto cleanup;
	}


	bswitch_add_graph(plt, obs);

cleanup:

	g_free(sp->x);
	g_free(sp->y);
	g_free(sp);
	sp = NULL;
	samples = 0;

	obs_assist_clear_spec(p);

	return TRUE;
}


/**
 * @brief move into position in bswitch
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean bswitch_obs_pos(ObsAssist *p)
{
	gsize i;

	gint next;

	static gint prev;

	gdouble avg;
	gdouble tmp;

	struct coord_equatorial equ;
	struct coord_horizontal hor;



	equ.ra  = p->cfg->bswitch.ra_cent;
	equ.dec = p->cfg->bswitch.de_cent;
	hor = equatorial_to_horizontal(equ, p->cfg->lat, p->cfg->lon, 0.0);

	/* we correct the azimuth distance for elevation, so it is
	 * the specified angular distance from the vertical circle of the
	 * target azimuth
	 */

	if (p->cfg->bswitch.pos != TGT)
		prev = p->cfg->bswitch.pos;

	switch (p->cfg->bswitch.pos) {
	case OFF1:
		hor.el += p->cfg->bswitch.el_off1;
		hor.az += p->cfg->bswitch.az_off1 / cos(RAD(hor.el));
		next = TGT;
		break;
	case OFF2:
		hor.el += p->cfg->bswitch.el_off2;
		hor.az += p->cfg->bswitch.az_off2 / cos(RAD(hor.el));
		next = TGT;
		break;
	case TGT:
		if (prev == OFF1)
			next = OFF2;
		else
			next = OFF1;

		break;
	default:
		/* should never happen */
		break;

	}

#if 1
	/* actual pointing is done in horizon system */
	if (!bswitch_in_position(p, hor.az, hor.el))
		return TRUE;
#endif

	if (!bswitch_measure(p))
		return TRUE;

	obs_assist_clear_spec(p);

	/* end of cycle, apply background correction */
	if ((p->cfg->bswitch.pos != TGT) && p->cfg->bswitch.pos1.n
	    && p->cfg->bswitch.pos2.n) {

		tmp = 0.0;

		for (i = 0; i < p->cfg->bswitch.tgt.n; i++) {
			avg = 0.5 * (p->cfg->bswitch.pos1.y[i] +
				     p->cfg->bswitch.pos2.y[i]);
			p->cfg->bswitch.tgt.y[i] -= avg;

			tmp += p->cfg->bswitch.tgt.y[i];
		}

		avg = tmp / (gdouble) p->cfg->bswitch.tgt.n;

		tmp = (gdouble) p->cfg->bswitch.rpt_cur;
		g_array_append_val(p->cfg->bswitch.idx, tmp);
		g_array_append_val(p->cfg->bswitch.amp, avg);

		bswitch_add_graph(p->cfg->bswitch.plt_corr,
				  &p->cfg->bswitch.tgt);

		cross_draw_continuum(p);

		/* clear the next ref position */
		if (p->cfg->bswitch.pos == OFF1) {
			g_free(p->cfg->bswitch.pos2.x);
			g_free(p->cfg->bswitch.pos2.y);
			p->cfg->bswitch.pos2.x = NULL;
			p->cfg->bswitch.pos2.y = NULL;
			p->cfg->bswitch.pos2.n = 0;
		}

		if (p->cfg->bswitch.pos == OFF2) {
			g_free(p->cfg->bswitch.pos1.x);
			g_free(p->cfg->bswitch.pos1.y);
			p->cfg->bswitch.pos1.x = NULL;
			p->cfg->bswitch.pos1.y = NULL;
			p->cfg->bswitch.pos1.n = 0;
		}

		bswitch_free(p);
	}

	/* next position */
	p->cfg->bswitch.pos = next;

	if (p->cfg->bswitch.pos != OFF1)
		return TRUE;

	return FALSE;
}



/**
 * @brief perform bswitch scan
 */

static gboolean bswitch_obs(void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	if (p->cfg->abort)
		goto cleanup;


	if (bswitch_obs_pos(p))
		return G_SOURCE_CONTINUE;

	/* on final, we stay at the current position */

	if (p->cfg->bswitch.rpt_cur <= p->cfg->bswitch.n_rpt) {
		bswitch_update_pbar_rpt(p);
		p->cfg->bswitch.rpt_cur++;
		return G_SOURCE_CONTINUE;
	}

cleanup:
	bswitch_free(p);
	g_array_free(p->cfg->bswitch.idx, TRUE);
	g_array_free(p->cfg->bswitch.amp, TRUE);


	return G_SOURCE_REMOVE;
}


/**
 * @brief start the bswitch observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkWidget *box;

	GtkGrid *grid;


	p->cfg->bswitch.idx = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->bswitch.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));

	sig_tracking(FALSE, 0.0, 0.0);

	obs_assist_hide_procedure_selectors(p);


	grid = GTK_GRID(new_default_grid());

	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	w = xyplot_new();
	xyplot_set_xlabel(w, "Frequency [MHz]");
	xyplot_set_ylabel(w, "Flux [K]");
	xyplot_set_title(w, "Offset 1 Spectrum");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_vexpand(w, TRUE);
	gtk_widget_set_size_request(w, -1, 300);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);
	p->cfg->bswitch.plt_pos1 = w;


	w = xyplot_new();
	xyplot_set_xlabel(w, "Frequency [MHz]");
	xyplot_set_ylabel(w, "Flux [K]");
	xyplot_set_title(w, "Target Spectrum");


	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_vexpand(w, TRUE);
	gtk_widget_set_size_request(w, -1, 300);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);
	p->cfg->bswitch.plt_tgt = w;


	w = xyplot_new();
	xyplot_set_xlabel(w, "Frequency [MHz]");
	xyplot_set_ylabel(w, "Flux [K]");
	xyplot_set_title(w, "Offset 2 Spectrum");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_vexpand(w, TRUE);
	gtk_widget_set_size_request(w, -1, 300);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);
	p->cfg->bswitch.plt_pos2 = w;


	gtk_grid_attach(grid, box, 0, 0, 3, 1);


	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	w = xyplot_new();
	xyplot_set_xlabel(w, "Frequency [MHz]");
	xyplot_set_ylabel(w, "Corrected Flux [K]");
	xyplot_set_title(w, "Offset-Corrected Spectrum");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_vexpand(w, TRUE);
	gtk_widget_set_size_request(w, -1, 300);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);
	p->cfg->bswitch.plt_corr = w;


	w = xyplot_new();
	xyplot_set_xlabel(w, "Sample");
	xyplot_set_ylabel(w, "Average Flux / Bin [K]");
	xyplot_set_title(w, "Average Flux per Sampling Cycle");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_vexpand(w, TRUE);
	gtk_widget_set_size_request(w, -1, 300);
	gtk_box_pack_start(GTK_BOX(box), w, TRUE, TRUE, 0);
	p->cfg->bswitch.plt_cont = w;

	gtk_grid_attach(grid, box, 0, 1, 3, 1);



	w = gtk_label_new("Progress");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);
	p->cfg->bswitch.pbar = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->bswitch.pbar, TRUE);
	gtk_grid_attach(grid, p->cfg->bswitch.pbar, 1, 2, 1, 1);


	w = gtk_button_new_with_label("Quit");
	gtk_widget_set_tooltip_text(w, "Quit observation");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_abort), p);


	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));


	/* the actual work is done asynchronously, .5 seconds calls
	 * per should be fine
	 */
	g_timeout_add(500, bswitch_obs, p);
}


/**
 * @brief set up the bswitch observation
 */

static void obs_assist_on_prepare_bswitch(GtkWidget *as, GtkWidget *pg,
					    ObsAssist *p)
{
	gint cp;

	GtkWidget *w;
	GtkWidget *box;

	GtkSpinButton *sb;

	GtkAssistantPageType type;

	gchar *lbl;



	type = gtk_assistant_get_page_type(GTK_ASSISTANT(as), pg);
	if (type != GTK_ASSISTANT_PAGE_CONFIRM)
		return;


	p->cfg->bswitch.ra_cent = p->cfg->ra;
	p->cfg->bswitch.de_cent = p->cfg->de;
	p->cfg->bswitch.pos = OFF1;

	/* set configuration */
	sb = p->cfg->bswitch.sb_az_off1_deg;
	p->cfg->bswitch.az_off1 = gtk_spin_button_get_value(sb);

	sb = p->cfg->bswitch.sb_az_off2_deg;
	p->cfg->bswitch.az_off2 = gtk_spin_button_get_value(sb);

	sb = p->cfg->bswitch.sb_el_off1_deg;
	p->cfg->bswitch.el_off1 = gtk_spin_button_get_value(sb);

	sb = p->cfg->bswitch.sb_el_off2_deg;
	p->cfg->bswitch.el_off2 = gtk_spin_button_get_value(sb);

	sb = p->cfg->bswitch.sb_avg;
	p->cfg->bswitch.n_avg = gtk_spin_button_get_value_as_int(sb);

	sb = p->cfg->bswitch.sb_rpt;
	p->cfg->bswitch.n_rpt = gtk_spin_button_get_value_as_int(sb);


	p->cfg->bswitch.rpt_cur = 1;

	bzero(&p->cfg->bswitch.pos1, sizeof(struct spectrum));
	bzero(&p->cfg->bswitch.pos2, sizeof(struct spectrum));
	bzero(&p->cfg->bswitch.tgt,  sizeof(struct spectrum));


	cp  = gtk_assistant_get_current_page(GTK_ASSISTANT(as));
      	box = gtk_assistant_get_nth_page(GTK_ASSISTANT(as), cp);

	gtk_container_foreach(GTK_CONTAINER(box),
			      (GtkCallback) gtk_widget_destroy, NULL);

	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

	lbl = g_strdup_printf(
	      "This is your configuration:\n\n"
	      "<tt>"
	      "Offset 1:             <b>AZ: %5.2f° EL: %5.2f°</b>\n"
	      "Offset 2:             <b>AZ: %5.2f° EL: %5.2f°</b>\n"
	      "Samples per position: <b>%d</b>\n\n"
	      "Observation cycles:   <b>%d</b>\n\n"
	      "</tt>",
	      p->cfg->bswitch.az_off1, p->cfg->bswitch.el_off1,
	      p->cfg->bswitch.az_off2, p->cfg->bswitch.el_off2,
	      p->cfg->bswitch.n_avg, p->cfg->bswitch.n_rpt);


        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_set_halign(w, GTK_ALIGN_START);


	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, TRUE);

	gtk_widget_show_all(box);
}


/**
 * @brief create info page
 */

static void obs_assist_bswitch_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);

	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
		"This observation mode will switch  between target and offset "
		"positions to take on-source and off-source comparision "
		"measuremments. The offset position measurements are averaged "
		"and subtracted from the target position measurements.");

        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);

	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_show_all(box);


	gtk_assistant_append_page(as, box);
	gtk_assistant_set_page_complete(as, box, TRUE);
	gtk_assistant_set_page_title(as, box, "Info");
	gtk_assistant_set_page_type(as, box, GTK_ASSISTANT_PAGE_INTRO);
}


/**
 * @brief create info page
 */

static void obs_assist_bswitch_create_page_2(GtkAssistant *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;
	GtkSpinButton *sb;


	grid = GTK_GRID(new_default_grid());


	/** Azimuth Off 1 **/
	w = gui_create_desclabel("Azimuth Offsets for position 1 and 2",
				 "Specify Azimuth offsets in degrees.");
	gtk_grid_attach(grid, w, 0, 0, 1, 1);


	/* set an arbitrary limit of 20 degrees */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-20., 20.,
					    ceil(p->cfg->az_res * 20.) * 0.1));
	gtk_spin_button_set_value(sb, -6.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 0, 1, 1);
	p->cfg->bswitch.sb_az_off1_deg = sb;

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-20., 20.,
					    ceil(p->cfg->az_res * 20.) * 0.1));
	gtk_spin_button_set_value(sb, 6.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 2, 0, 1, 1);
	p->cfg->bswitch.sb_az_off2_deg = sb;




	/** Elevation Step **/
	w = gui_create_desclabel("Elevation Offsets for positions 1 and 2",
				 "Specify the Elevation offsets in degrees.");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set an arbitrary limit of 20 degrees */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-20., 20.,
				    ceil(p->cfg->el_res * 20.) * 0.1));

	gtk_spin_button_set_value(sb, -6.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->bswitch.sb_el_off1_deg = sb;


	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(-20., 20.,
				    ceil(p->cfg->el_res * 20.) * 0.1));

	gtk_spin_button_set_value(sb, 6.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 2, 1, 1, 1);
	p->cfg->bswitch.sb_el_off2_deg = sb;


	/** Averages **/
	w = gui_create_desclabel("Samples per position",
				 "Specify the number of measurements to be "
				 "averaged at each position.");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 20, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 2, 2, 1, 1);
	p->cfg->bswitch.sb_avg = GTK_SPIN_BUTTON(sb);


	/** Repeat **/
	w = gui_create_desclabel("Repeats",
				 "Specify the number of times to repeat the "
				 "switching operation.");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 100, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 2, 3, 1, 1);
	p->cfg->bswitch.sb_rpt = GTK_SPIN_BUTTON(sb);



	gtk_widget_show_all(GTK_WIDGET(grid));

	/* attach page */
	gtk_assistant_append_page(as, GTK_WIDGET(grid));
	gtk_assistant_set_page_complete(as, GTK_WIDGET(grid), TRUE);
	gtk_assistant_set_page_title(as, GTK_WIDGET(grid), "Setup");
	gtk_assistant_set_page_type(as, GTK_WIDGET(grid),
				    GTK_ASSISTANT_PAGE_CONTENT);
}


/**
 * @brief create summary page
 */

static void obs_assist_bswitch_create_page_3(GtkAssistant *as)
{
	GtkWidget *box;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_widget_show_all(box);

	gtk_assistant_append_page(as, box);
	gtk_assistant_set_page_title(as, box, "Confirm");
	gtk_assistant_set_page_complete(as, box, TRUE);
	gtk_assistant_set_page_type(as, box, GTK_ASSISTANT_PAGE_CONFIRM);
}


/**
 * @brief populate the assistant
 */

static void obs_assist_bswitch_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);

	p->cfg->abort = FALSE;
	bzero(&p->cfg->bswitch, sizeof(p->cfg->bswitch));

	/* info page */
	obs_assist_bswitch_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_bswitch_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_bswitch_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_bswitch), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create beamswitching selection
 */

GtkWidget *obs_assist_bswitch_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Beam Switching",
				 "Perform a beam switching observation");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Beam Switching");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_bswitch_setup_cb), p);


	return GTK_WIDGET(grid);
}
