/**
 * @file    widgets/obs_assist/obs_assist_cross.c
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
 * @brief a widget to show local and remote systems status info
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
#include <levmar.h>
#include <cmd.h>
#include <math.h>


/**
 * @brief the gaussian we use for fitting the beam
 */

static double gaussian(gdouble *p, gdouble x)
{
	return  p[3] + p[0] * exp( - pow( ((x - p[2]) / p[1]), 2));
}


/**
 * @brief determine initial parameters for the gaussian
 *
 * @param par the parameter array (4 elements, see gaussian())
 *
 * @param x the array of x values
 * @param y the array of y values
 * @param n the number of data elements
 */

static void gaussian_calc_param(gdouble par[4],
				const gdouble *x, const gdouble *y, size_t n)
{
	size_t i;

	gdouble tmp;

	gdouble sig = 0.0;
	gdouble mean = 0.0;

	gdouble xmin = DBL_MAX;
	gdouble xmax = DBL_MIN;
	gdouble ymin = DBL_MAX;
	gdouble ymax = DBL_MIN;


	if (!n)
		return;

	for (i = 0; i < n; i++) {

		if (x[i] > xmax)
			xmax = x[i];

		if (x[i] < xmin)
			xmin = x[i];

		if (y[i] > ymax)
			ymax = y[i];

		if (y[i] < ymin)
			ymin = y[i];

		mean += x[i];
	}

	mean /= (gdouble) n;

	for (i = 0; i < n; i++) {
		tmp = x[i] - mean;
		sig += tmp * tmp;
	}

	sig /= (gdouble) n;

	sig = sqrt(sig);

	par[0] = (ymax - ymin);		/* amplitude */
	par[1] = sig;			/* sigma */
	par[2] = mean;			/* center shift */
	par[3] = ymin;			/* baseline shift */
}


/**
 * @brief function to do the fitting
 *
 * @returns 0 on success, otherwise error
 *
 * @note the number of data must be at least the number of parameters
 */

static int fit_gaussian(gdouble par[4],
			 const gdouble *x, const gdouble *y, size_t n)
{
	struct lm_ctrl lm;


	if (n < 4)
		return -1;

	lm_init(&lm);
	lm_set_fit_param(&lm, &gaussian, NULL, par, 4);


	lm_min(&lm, x, y, NULL, n);


	return 0;
}


/**
 * @brief plot a gaussian
 */

static void plot_gaussian(GtkWidget *w, gdouble par[4], size_t n,
			  struct crossax *ax)
{
	size_t i;


	gdouble *x;
	gdouble *y;

	gdouble pmin, pmax;
	gdouble smin, smax, symin, symax;

	const GdkRGBA red = {1.0, 0.0, 0.0, 1.0};


	x = g_malloc(n * sizeof(gdouble));
	y = g_malloc(n * sizeof(gdouble));


	xyplot_get_data_axis_range(w, &pmin, &pmax, NULL, NULL);
	xyplot_get_sel_axis_range(w, &smin, &smax, &symin,&symax);


	for (i = 0; i < n; i++) {
		x[i] = pmin + ((gdouble) i) * (pmax - pmin) / ((double) n);
		y[i] = gaussian(par, x[i]);
	}


	/* graph outside box */
	xyplot_drop_graph(w, ax->plt_ref_in);
	ax->plt_ref_in = xyplot_add_graph(w, x, y, NULL, n,
					  g_strdup_printf("FIT"));

	xyplot_set_graph_style(w, ax->plt_ref_in, DASHES);
	xyplot_set_graph_rgba(w, ax->plt_ref_in, red);


	/* graph inside box */
	x = g_malloc(n * sizeof(gdouble));
	y = g_malloc(n * sizeof(gdouble));

	for (i = 0; i < n; i++) {
		x[i] = smin + ((gdouble) i) * (smax - smin) / ((double) n);
		y[i] = gaussian(par, x[i]);

		if ((y[i] > symax) || (y[i] < symin)) {
			x[i] = NAN;
			y[i] = NAN;
		}

	}

	xyplot_drop_graph(w, ax->plt_ref_out);
	ax->plt_ref_out = xyplot_add_graph(w, x, y, NULL, n, g_strdup_printf("FIT"));

	xyplot_set_graph_style(w, ax->plt_ref_out, NAN_LINES);
	xyplot_set_graph_rgba(w, ax->plt_ref_out, red);

	xyplot_redraw(w);
}




/**
 * @brief fit selection box callback
 */

static gboolean cross_plt_fitbox_selected(GtkWidget *w, gpointer data)
{
	int ret;
	size_t n;

	gdouble *x;
	gdouble *y;
	gchar *lbl;

	gdouble par[4];

	struct crossax *ax;


	if (!data)
		return TRUE;

	ax = (struct crossax *) data;

	n = xyplot_get_selection_data(w, &x, &y, NULL);
	if (!n) {
		xyplot_drop_graph(w, ax->plt_ref_in);
		xyplot_drop_graph(w, ax->plt_ref_out);
		return TRUE;
	}

	gaussian_calc_param(par, x, y, n);

	ret = fit_gaussian(par, x, y, n);

	g_free(x);
	g_free(y);

	if (ret)
		return TRUE;


	lbl = g_strdup_printf("Fit Results:\n\n"
			      "<tt>"
			      "Peak shift:       <b>%5.2f°</b>\n"
			      "Height over base: <b>%6.2fK</b>\n"
			      "FWHM:             <b>%5.2f°</b>\n\n"
			      "</tt>",
			      par[2], par[0],
			      fabs(par[1]) * 2.0 * sqrt(log(2.0)));

	gtk_label_set_markup(ax->fitpar, lbl);
	g_free(lbl);

	/* plot a fixed 100 points for now */
	plot_gaussian(w, par, 100, ax);

	return TRUE;
}


/**
 * @brief enable/disable tracking of central position
 */

void obs_assist_on_cross_track(GtkWidget *w, ObsAssist *p)
{
	p->cfg->cross.track = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
}


/**
 * @brief get cosine-corrected azimuth
 **/

static gdouble obs_assistant_get_corr_az(ObsAssist *p)
{
	gdouble az;


	az = p->cfg->cross.az_cent;
	az = az + (p->cfg->cross.az_cur - az) * p->cfg->cross.az_cor;

	return az;
}


/**
 * @brief set the cross observation paramters according to the
 *        configured equatorial coordinates, number of points and step size
 */

static void obs_assist_set_obs_param(ObsAssist *p)
{
	gdouble az_min;
	gdouble az_max;
	gdouble el_min;
	gdouble el_max;
	gdouble az_cor;
	gdouble az_off;
	gdouble az_stp;
	gdouble el_off;
	gdouble el_stp;

	struct coord_horizontal hor;
	struct coord_equatorial	equ;


	/* if tracked, convert vom ra/de */
	if (p->cfg->cross.track) {

		equ.ra  = p->cfg->cross.ra_cent;
		equ.dec = p->cfg->cross.de_cent;

		hor = equatorial_to_horizontal(equ, p->cfg->lat, p->cfg->lon,
					       0.0);

		p->cfg->cross.az_cent = hor.az;
		p->cfg->cross.el_cent = hor.el;
	}


	az_cor = 1.0 / cos(RAD(p->cfg->cross.el_cent));

	if (p->cfg->cross.az_pt)
		az_off = p->cfg->cross.deg * 0.5 * (p->cfg->cross.az_pt - 1.0);
	else
		az_off = 0.0;

	az_min = p->cfg->cross.az_cent - az_off;
	az_max = p->cfg->cross.az_cent + az_off;

	if (p->cfg->cross.az_pt)
		az_stp = (az_max - az_min)  / (p->cfg->cross.az_pt - 1.0);
	else
		az_stp = 0.0;



	if (p->cfg->cross.el_pt)
		el_off = p->cfg->cross.deg * 0.5 * (p->cfg->cross.el_pt - 1.0);
	else
		el_off = 0.0;

	el_min = p->cfg->cross.el_cent - el_off;
	el_max = p->cfg->cross.el_cent + el_off;

	if (p->cfg->cross.el_pt)
		el_stp = (el_max - el_min)  / (p->cfg->cross.el_pt - 1.0);
	else
		el_stp = 0.0;


	p->cfg->cross.az_min = az_min;
	p->cfg->cross.az_max = az_max;
	p->cfg->cross.el_min = el_min;
	p->cfg->cross.el_max = el_max;
	p->cfg->cross.az_cor = az_cor;
	p->cfg->cross.az_off = az_off;
	p->cfg->cross.az_stp = az_stp;
	p->cfg->cross.el_off = el_off;
	p->cfg->cross.el_stp = el_stp;
}


/**
 * @brief update the Azimuth progress bar
 */

static void cross_update_pbar_az(ObsAssist *p)
{
	gdouble frac;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->cross.pbar_az);

	frac = (p->cfg->cross.az_cur - p->cfg->cross.az_min) /
	       (p->cfg->cross.az_max - p->cfg->cross.az_min);

	str = g_strdup_printf("Offset: %5.2f°",
			      (p->cfg->cross.az_cur - p->cfg->cross.az_cent) /
			       p->cfg->cross.az_cor);

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief update the Elevation progress bar
 */

static void cross_update_pbar_el(ObsAssist *p)
{
	gdouble frac;

	gchar *str;

	GtkProgressBar *pb;


	pb = GTK_PROGRESS_BAR(p->cfg->cross.pbar_el);

	frac = (p->cfg->cross.el_cur - p->cfg->cross.el_min) /
	       (p->cfg->cross.el_max - p->cfg->cross.el_min);

	str = g_strdup_printf("Offset: %5.2f°",
			      (p->cfg->cross.el_cur - p->cfg->cross.el_cent));

	gtk_progress_bar_set_fraction(pb, frac);
	gtk_progress_bar_set_show_text(pb, TRUE);
	gtk_progress_bar_set_text(pb, str);

	g_free(str);
}


/**
 * @brief clear and draw the Azimuth plot
 */

static void cross_draw_graph_az(ObsAssist *p)
{
	void *ref;

	gdouble *x;
	gdouble *y;


	/* update graph */
	xyplot_drop_all_graphs(p->cfg->cross.plt_az);
	x = g_memdup(p->cfg->cross.az.off->data,
		     p->cfg->cross.az.off->len * sizeof(gdouble));
	y = g_memdup(p->cfg->cross.az.amp->data,
		     p->cfg->cross.az.amp->len * sizeof(gdouble));

	ref = xyplot_add_graph(p->cfg->cross.plt_az, x, y, NULL,
			       p->cfg->cross.az.off->len,
			       g_strdup_printf("Azimuth Scan"));

	xyplot_set_graph_style(p->cfg->cross.plt_az, ref, CIRCLES);
	xyplot_redraw(p->cfg->cross.plt_az);
}


/**
 * @brief clear and draw the Elevation plot
 */

static void cross_draw_graph_el(ObsAssist *p)
{
	void *ref;

	gdouble *x;
	gdouble *y;


	/* update graph */
	xyplot_drop_all_graphs(p->cfg->cross.plt_el);
	x = g_memdup(p->cfg->cross.el.off->data,
		     p->cfg->cross.el.off->len * sizeof(gdouble));
	y = g_memdup(p->cfg->cross.el.amp->data,
		     p->cfg->cross.el.amp->len * sizeof(gdouble));

	ref = xyplot_add_graph(p->cfg->cross.plt_el, x, y, NULL,
			       p->cfg->cross.el.off->len,
			       g_strdup_printf("Elevation Scan"));

	xyplot_set_graph_style(p->cfg->cross.plt_el, ref, CIRCLES);
	xyplot_redraw(p->cfg->cross.plt_el);
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

static gboolean cross_in_position(ObsAssist *p, gdouble az, gdouble el)
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
 * @param az if TRUE: axis is Azimuth; if FALSE: axis is Elevation
 *
 * @brief returns TRUE if measurement was taken
 */

static gboolean cross_measure(ObsAssist *p, gboolean az)
{
	gsize i;

	gdouble avg = 0.0;
	gdouble offset;

	static guint sample;


	/* enable acquisition at current position */
	if (!p->cfg->acq_enabled) {
		cmd_spec_acq_enable(PKT_TRANS_ID_UNDEF);
		return FALSE;
	}

	/* has new spectral data arrived? */
	if (!p->cfg->spec.n)
		return FALSE;

	/* compute continuum flux */
	for (i = 0; i < p->cfg->spec.n; i++)
		avg += p->cfg->spec.y[i];

	avg = avg / (gdouble) p->cfg->spec.n;

	if (az) {
		offset = p->cfg->cross.az_cur - p->cfg->cross.az_cent;
		g_array_append_val(p->cfg->cross.az.off, offset);
		g_array_append_val(p->cfg->cross.az.amp, avg);
	} else {
		offset = p->cfg->cross.el_cur - p->cfg->cross.el_cent;
		g_array_append_val(p->cfg->cross.el.off, offset);
		g_array_append_val(p->cfg->cross.el.amp, avg);
	}

	if (sample < p->cfg->cross.samples) {
		sample++;
		return FALSE;
	}

	sample = 0;

	return TRUE;
}


/**
 * @brief scan the azimuth direction of the cross
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean cross_obs_az(ObsAssist *p)
{
	gdouble az;
	gdouble el;

	gdouble az_lim;


	/* is azimuth done?  */
	az_lim = p->cfg->cross.az_max + p->cfg->cross.az_stp;
	if (p->cfg->cross.az_cur >= az_lim)
		return FALSE;

	cross_update_pbar_az(p);

	az = obs_assistant_get_corr_az(p);
	el = p->cfg->cross.el_cent;

	if (!cross_in_position(p, az, el))
		return TRUE;

	if (!cross_measure(p, TRUE))
		return TRUE;

	obs_assist_clear_spec(p);

	cross_draw_graph_az(p);

	/* update azimuth */
	p->cfg->cross.az_cur += p->cfg->cross.az_stp;

	return TRUE;
}


/**
 * @brief scan the elevation direction of the cross
 *
 * @returns TRUE if observation is ongoing, FALSE if complete
 */

static gboolean cross_obs_el(ObsAssist *p)
{
	gdouble az;
	gdouble el;

	gdouble el_lim;


	/* is elevation done?  */
	el_lim = p->cfg->cross.el_max + p->cfg->cross.el_stp;
	if (p->cfg->cross.el_cur >= el_lim)
		return FALSE;

	cross_update_pbar_el(p);

	az = p->cfg->cross.az_cent;
	el = p->cfg->cross.el_cur;

	if (!cross_in_position(p, az, el))
		return TRUE;

	if (!cross_measure(p, FALSE))
		return TRUE;

	obs_assist_clear_spec(p);

	cross_draw_graph_el(p);

	/* update elevation */
	p->cfg->cross.el_cur += p->cfg->cross.el_stp;

	return TRUE;
}


/**
 * @brief scan both cross directions
 */

static gboolean cross_obs(void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	if (p->cfg->abort) {
		/* move back to center */
		cmd_moveto_azel(PKT_TRANS_ID_UNDEF,
				p->cfg->cross.az_cent,
				p->cfg->cross.el_cent);

		return G_SOURCE_REMOVE;
	}

	obs_assist_set_obs_param(p);

	if (cross_obs_az(p))
		return G_SOURCE_CONTINUE;

	if (cross_obs_el(p))
		return G_SOURCE_CONTINUE;

	/* move back to center */
	cmd_moveto_azel(PKT_TRANS_ID_UNDEF,
			p->cfg->cross.az_cent,
			p->cfg->cross.el_cent);

	xyplot_select_all_data(p->cfg->cross.plt_az);
	xyplot_select_all_data(p->cfg->cross.plt_el);

	/* done */
	g_array_free(p->cfg->cross.az.off, FALSE);
	g_array_free(p->cfg->cross.az.amp, FALSE);
	g_array_free(p->cfg->cross.el.off, FALSE);
	g_array_free(p->cfg->cross.el.amp, FALSE);


	return G_SOURCE_REMOVE;
}


/**
 * @brief start the cross observation
 */

static void on_assistant_apply(GtkWidget *as, ObsAssist *p)
{
	GtkWidget *w;

	GtkGrid *grid;


	p->cfg->cross.az.off = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->cross.az.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->cross.el.off = g_array_new(FALSE, FALSE, sizeof(gdouble));
	p->cfg->cross.el.amp = g_array_new(FALSE, FALSE, sizeof(gdouble));


	g_signal_emit_by_name(sig_get_instance(), "tracking", FALSE);

	gtk_container_foreach(GTK_CONTAINER(p),
			      (GtkCallback) gtk_widget_hide, NULL);

	grid = GTK_GRID(new_default_grid());

	p->cfg->cross.plt_az = xyplot_new();
	xyplot_set_xlabel(p->cfg->cross.plt_az, "Offset");
	xyplot_set_ylabel(p->cfg->cross.plt_az, "Amplitude");

	gtk_widget_set_hexpand(p->cfg->cross.plt_az, TRUE);
	gtk_widget_set_vexpand(p->cfg->cross.plt_az, TRUE);
	gtk_grid_attach(grid, p->cfg->cross.plt_az, 0, 0, 2, 1);

	p->cfg->cross.plt_el = xyplot_new();
	xyplot_set_xlabel(p->cfg->cross.plt_el, "Offset");
	xyplot_set_ylabel(p->cfg->cross.plt_el, "Amplitude");

	gtk_widget_set_hexpand(p->cfg->cross.plt_el, TRUE);
	gtk_widget_set_vexpand(p->cfg->cross.plt_el, TRUE);
	gtk_grid_attach(grid, p->cfg->cross.plt_el, 2, 0, 2, 1);


	w = gtk_label_new("Azimuth Scan");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);
	p->cfg->cross.pbar_az = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->cross.pbar_az, TRUE);
	gtk_grid_attach(grid, p->cfg->cross.pbar_az, 1, 1, 1, 1);

	w = gtk_label_new("Elevation Scan");
	gtk_grid_attach(grid, w, 2, 1, 1, 1);
	p->cfg->cross.pbar_el = gtk_progress_bar_new();
	gtk_widget_set_hexpand(p->cfg->cross.pbar_el, TRUE);
	gtk_grid_attach(grid, p->cfg->cross.pbar_el, 3, 1, 1, 1);


	w = gtk_label_new("");
	gtk_grid_attach(grid, w, 0, 2, 2, 1);
	p->cfg->cross.az.fitpar = GTK_LABEL(w);

	w = gtk_label_new("");
	gtk_grid_attach(grid, w, 2, 2, 2, 1);
	p->cfg->cross.el.fitpar = GTK_LABEL(w);


	w = gtk_button_new_with_label("Quit");
	gtk_widget_set_tooltip_text(w, "Quit observation");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_abort), p);

	/** XXX result label as parameter output */
	g_signal_connect(p->cfg->cross.plt_az,  "xyplot-fit-selection",
			 G_CALLBACK(cross_plt_fitbox_selected),
			 &p->cfg->cross.az);

	g_signal_connect(p->cfg->cross.plt_el,  "xyplot-fit-selection",
			 G_CALLBACK(cross_plt_fitbox_selected),
			 &p->cfg->cross.el);

	gtk_box_pack_start(GTK_BOX(p), GTK_WIDGET(grid), TRUE, TRUE, 0);
	gtk_widget_show_all(GTK_WIDGET(grid));

	/* the actual work is done asynchronously, .5 seconds calls
	 * per should be fine
	 */
	g_timeout_add(500, cross_obs, p);
}


/**
 * @brief set up the cross observation
 */

static void obs_assist_on_prepare_cross(GtkWidget *as, GtkWidget *pg,
					ObsAssist *p)
{
	gint cp;

	GtkWidget *w;
	GtkWidget *box;

	GtkAssistantPageType type;

	gchar *lbl;

	gdouble az_min;
	gdouble az_max;
	gdouble off;



	gdouble page_complete = TRUE;


	type = gtk_assistant_get_page_type(GTK_ASSISTANT(as), pg);
	if (type != GTK_ASSISTANT_PAGE_CONFIRM)
		return;


	/* set configuration */
	p->cfg->cross.az_pt   = gtk_spin_button_get_value(p->cfg->cross.sb_az);
	p->cfg->cross.el_pt   = gtk_spin_button_get_value(p->cfg->cross.sb_el);
	p->cfg->cross.deg     = gtk_spin_button_get_value(p->cfg->cross.sb_deg);
	p->cfg->cross.samples = gtk_spin_button_get_value(p->cfg->cross.sb_sa);

	p->cfg->cross.az_cent = p->cfg->az;
	p->cfg->cross.el_cent = p->cfg->el;
	p->cfg->cross.ra_cent = p->cfg->ra;
	p->cfg->cross.de_cent = p->cfg->de;


	obs_assist_set_obs_param(p);

	/* initial cross axis positions are at minimum */
	p->cfg->cross.az_cur = p->cfg->cross.az_min;
	p->cfg->cross.el_cur = p->cfg->cross.el_min;


	cp  = gtk_assistant_get_current_page(GTK_ASSISTANT(as));
      	box = gtk_assistant_get_nth_page(GTK_ASSISTANT(as), cp);

	gtk_container_foreach(GTK_CONTAINER(box),
			      (GtkCallback) gtk_widget_destroy, NULL);

	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

	off = p->cfg->cross.az_off * p->cfg->cross.az_cor;

	az_min = p->cfg->cross.az_cent - off;
	az_max = p->cfg->cross.az_cent + off;


	lbl = g_strdup_printf(
	      "This is your configuration:\n\n"
	      "<tt>"
	      "Scan points in Azimuth:    <b>%5.0f</b>\n"
	      "Scan points in Elevation:  <b>%5.0f</b>\n"
	      "Nominal step size:         <b>%5.2f°</b>\n"
	      "Azimuth step:              <b>%5.2f°</b>\n"
	      "Elevation step:            <b>%5.2f°</b>\n\n"
	      "Center Azimuth:            <b>%5.2f°</b>\n"
	      "Center Elevation:          <b>%5.2f°</b>\n"
	      "Scan range in Azimuth:     <b>%5.2f°</b> to <b>%5.2f°</b>\n"
	      "Scan range in Elevation:   <b>%5.2f°</b> to <b>%5.2f°</b>\n"
	      "Samples per position:      <b>%u</b>\n"
	      "Tracking:                  <b>%s</b>\n"
	      "</tt>",
	      p->cfg->cross.az_pt, p->cfg->cross.el_pt,
	      p->cfg->cross.deg,
	      p->cfg->cross.az_stp, p->cfg->cross.el_stp,
	      p->cfg->cross.az_cent, p->cfg->cross.el_cent,
	      az_min, az_max,
	      p->cfg->cross.el_min, p->cfg->cross.el_max,
	      p->cfg->cross.samples,
	      p->cfg->cross.track ? "ENABLED" : "DISABLED");

        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);
	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_set_halign(w, GTK_ALIGN_START);




	if (p->cfg->cross.el_max > p->cfg->el_max) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("upper", "elevation",
							  p->cfg->el_max);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (p->cfg->cross.el_min < p->cfg->el_min) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("lower", "elevation",
							  p->cfg->el_min);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (az_max > p->cfg->az_max) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("right", "azimuth",
							  p->cfg->az_max);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (az_min < p->cfg->az_min) {
		page_complete = FALSE;
		w = obs_assist_limits_exceeded_warning("left", "azimuth",
							  p->cfg->az_min);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}

	if (!page_complete) {
		w = gtk_check_button_new_with_label("I choose to ignore any "
						    "warnings.");
		g_signal_connect(G_OBJECT(w), "toggled",
				 G_CALLBACK(obs_assist_on_ignore_warning), as);
		gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	}



	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, page_complete);

	gtk_widget_show_all(box);
}


/**
 * @brief create info page
 */

static void obs_assist_cross_create_page_1(GtkAssistant *as)
{
	GtkWidget *w;
	GtkWidget *box;

	gchar *lbl;


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);

	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
        lbl = g_strdup_printf(
		"This observation mode will perform a scan in the shape of a "
		"cross around the current on-sky position of the telescope.\n\n"
		"<b>Note:</b> If enabled, the central position will be tracked "
		"at the sidereal rate. The resulting graphs will be in Azimuth "
		"and Elevation offsets from the centeral position."
		"Azimuth distance will be corrected for the cosine of the "
		"Elevation for actual angular distance.");
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

static void obs_assist_cross_create_page_2(GtkAssistant *as, ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;
	GtkSpinButton *sb;

	gdouble res;


	grid = GTK_GRID(new_default_grid());


	/** STEP **/
	w = gui_create_desclabel("Step Size",
				 "Specify the step size in degrees. This "
				 "setting will apply to both scan directions.");
	gtk_grid_attach(grid, w, 0, 0, 1, 1);

	/* determine some minimum step size */
	if (p->cfg->el_res < p->cfg->az_res)
		res = p->cfg->az_res;
	else
		res = p->cfg->el_res;

	/* set an arbitrary limit of 20 degrees for step size */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(res, 20., 0.1));
	gtk_spin_button_set_value(sb, 2.0);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 0, 1, 1);
	p->cfg->cross.sb_deg = sb;


	/** AZ **/
	w = gui_create_desclabel("Steps in Azimuth",
				 "Specify the number of steps in Azimuth. An "
				 "even number of steps will take samples only "
				 "left and right of the center");
	gtk_grid_attach(grid, w, 0, 1, 1, 1);

	/* set some arbitrary limits */
	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 101, 1));
	gtk_spin_button_set_value(sb, 11);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 1, 1, 1);
	p->cfg->cross.sb_az = sb;


	/** EL **/
	w = gui_create_desclabel("Steps in Elevation",
				 "Specify the number of steps in Elevation. An "
				 "even number of steps will take samples only "
				 "above and below of the center");
	gtk_grid_attach(grid, w, 0, 2, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 101, 1));
	gtk_spin_button_set_value(sb, 11);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 2, 1, 1);
	p->cfg->cross.sb_el = GTK_SPIN_BUTTON(sb);

	/** EL **/
	w = gui_create_desclabel("Samples per position",
				 "Specify the number of measurements taken at "
				 "each position.");
	gtk_grid_attach(grid, w, 0, 3, 1, 1);

	sb = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1, 101, 1));
	gtk_spin_button_set_value(sb, 1);
	gtk_spin_button_set_numeric(sb, TRUE);
	gtk_spin_button_set_snap_to_ticks(sb, TRUE);
	gtk_widget_set_valign(GTK_WIDGET(sb), GTK_ALIGN_CENTER);
	gtk_grid_attach(grid, GTK_WIDGET(sb), 1, 3, 1, 1);
	p->cfg->cross.sb_sa = GTK_SPIN_BUTTON(sb);


	w = gui_create_desclabel("Enable Tracking",
				 "If enabled, the current on-sky position is "
				 "tracked at sidereal rate.");
	gtk_grid_attach(grid, w, 0, 4, 1, 1);

	w = gtk_check_button_new_with_label("Track Sky");
	g_signal_connect(G_OBJECT(w), "toggled",
				 G_CALLBACK(obs_assist_on_cross_track), p);
	gtk_grid_attach(grid, w, 1, 4, 1, 1);




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

static void obs_assist_cross_create_page_3(GtkAssistant *as)
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

static void obs_assist_cross_setup_cb(GtkWidget *w, ObsAssist *p)
{
	GtkWidget *as;


	as = obs_assist_create_default(w);
	g_return_if_fail(as);

	p->cfg->abort = FALSE;
	bzero(&p->cfg->cross, sizeof(p->cfg->cross));

	/* info page */
	obs_assist_cross_create_page_1(GTK_ASSISTANT(as));
	/* settings page */
	obs_assist_cross_create_page_2(GTK_ASSISTANT(as), p);
	/** summary page **/
	obs_assist_cross_create_page_3(GTK_ASSISTANT(as));


	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(obs_assist_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "prepare",
 			 G_CALLBACK(obs_assist_on_prepare_cross), p);
	g_signal_connect(G_OBJECT(as), "apply",
			  G_CALLBACK(on_assistant_apply), p);


	gtk_widget_show(as);
}


/**
 * @brief create cross scan selection
 */

GtkWidget *obs_assist_cross_scan_new(ObsAssist *p)
{
	GtkWidget *w;
	GtkGrid *grid;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Cross Scan",
				 "Perform a scan around a source in azimuth "
				 "and elevation.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Cross Scan.");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);


	return GTK_WIDGET(grid);
}
