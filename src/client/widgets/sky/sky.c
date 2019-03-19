/**
 * @file    widgets/sky/sky.c
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
 * @brief create a projection of the sky
 *
 * @todo clean up, in partiular use of data types
 */


#include <cairo.h>
#include <gtk/gtk.h>

#include <sky.h>
#include <sky_cfg.h>
#include <milky_way.h>
#include <coordinates.h>
#include <cmd.h>
#include <signals.h>

#include <string.h>
#include <math.h>
#include <float.h>


#define SKY_OBJ_SIZE   5.0
#define SKY_SUN_SIZE   7.0
#define SKY_MOON_SIZE  7.0

#define ARRAY_SIZE(x)	(sizeof(x)/sizeof(x[0]))

G_DEFINE_TYPE_WITH_PRIVATE(Sky, sky, GTK_TYPE_DRAWING_AREA)


static void sky_plot(GtkWidget *w);


/**
 * @brief handle tracking signal and turn of internal object selection
 */

static void sky_handle_tracking(gpointer instance, gboolean state,
				gdouble az, gdouble el,
				Sky *p)
{
	struct sky_obj *obj;
	GList *l;


	if (state)
		return;

	/* deselect all */
	p->cfg->sel = NULL;

	for (l = p->cfg->obj; l; l = l->next) {
		obj = l->data;
		obj->selected = FALSE;
	}
}


/**
 * @brief handle target position data
 */

static void sky_handle_pr_moveto_azel(gpointer instance,
				      const gdouble az, const gdouble el,
				      gpointer data)
{
	Sky *p;


	p = SKY(data);
	p->cfg->tgt.az = az;
	p->cfg->tgt.el = el;

	sky_plot(GTK_WIDGET(p));
}


/**
 * @brief handle current position data
 */

static void sky_handle_pr_getpos_azel(gpointer instance, struct getpos *pos,
				      gpointer data)
{
	Sky *p;


	p = SKY(data);

	p->cfg->pos.az = (gdouble) pos->az_arcsec / 3600.0;
	p->cfg->pos.el = (gdouble) pos->el_arcsec / 3600.0;

	sky_plot(GTK_WIDGET(p));
}


/**
 * @brief handle capabilities data
 */

static void sky_handle_pr_capabilities(gpointer instance,
				       const struct capabilities *c,
				       gpointer data)
{
	Sky *p;


	p = SKY(data);


	p->cfg->lat = (gdouble) c->lat_arcsec / 3600.0;
	p->cfg->lon = (gdouble) c->lon_arcsec / 3600.0;

	p->cfg->az_res = (gdouble) c->az_res_arcsec / 3600.0;
	p->cfg->el_res = (gdouble) c->el_res_arcsec / 3600.0;

	p->cfg->lim[0].az = (gdouble) c->az_min_arcsec / 3600.0;
	p->cfg->lim[0].el = (gdouble) c->el_min_arcsec / 3600.0;

	p->cfg->lim[1].az = (gdouble) c->az_max_arcsec / 3600.0;
	p->cfg->lim[1].el = (gdouble) c->el_max_arcsec / 3600.0;

	if (p->cfg->local_hor)
		g_free(p->cfg->local_hor);

	p->cfg->n_local_hor = c->n_hor;
	p->cfg->local_hor = g_memdup(c->hor,
				     c->n_hor * sizeof(struct local_horizon));

	sky_plot(GTK_WIDGET(p));
}


/**
 * @brief update the tracked position
 *
 * @note this is useful for things which move relative to the
 *	 celestial background, e.g. the moon or artificial satellites
 */

static void sky_update_tracked_pos(Sky *p)
{
	gdouble d_az;
	gdouble d_el;

	gdouble az_tol = 2.0 * p->cfg->az_res;
	gdouble el_tol = 2.0 * p->cfg->el_res;


	d_az = fabs(p->cfg->sel->hor.az - p->cfg->tgt.az);
	d_el = fabs(p->cfg->sel->hor.el - p->cfg->tgt.el);


	if ((d_az < az_tol) && (d_el < el_tol))
		return;


	g_signal_emit_by_name(sig_get_instance(), "tracking", TRUE,
			      p->cfg->sel->hor.az, p->cfg->sel->hor.el);
}


/**
 * @brief update horizon system coordinates
 */

static gboolean sky_update_coord_hor(gpointer data)
{
	Sky *p;
	struct sky_obj *obj;
	GList *l;


	p = SKY(data);

	l = p->cfg->obj;

	while (l) {
		obj = l->data;

		/* not exactly safe, but whatever */
		if (!strncmp("Sun", obj->name, 3))
			obj->eq = sun_ra_dec(p->cfg->time_off);

		if (!strncmp("Moon", obj->name, 4))
			obj->eq = moon_ra_dec(p->cfg->lat, p->cfg->lon,
					      p->cfg->time_off);

		obj->hor = equatorial_to_horizontal(obj->eq,
						    p->cfg->lat, p->cfg->lon,
						    p->cfg->time_off);
		l = l->next;
	}

	if (p->cfg->sel)
		sky_update_tracked_pos(p);


	sky_plot(GTK_WIDGET(p));

	return G_SOURCE_CONTINUE;
}


/**
 * @brief add an object to our list of objects
 */

static void sky_append_object(Sky *p, gchar *name, struct coord_equatorial eq,
			      gdouble radius)
{
	struct sky_obj *obj;


	obj = g_malloc0(sizeof(struct sky_obj));

	obj->name   = g_strdup(name);
	obj->eq     = eq;
	obj->radius = radius;

	p->cfg->obj = g_list_append(p->cfg->obj, obj);
}


/**
 * @brief add a new sky object for equatorial coordinates
 */

static void sky_add_object_eq(Sky *p, GKeyFile *kf, gchar *group)
{
	gsize len;
	gdouble *c;

	GError *error = NULL;

	struct coord_equatorial eq;


	c = g_key_file_get_double_list(kf, group, "eq", &len, &error);
	if (error) {
		g_warning(error->message);
		g_error_free(error);
	}


	switch (len) {
	case 2:
		eq.ra  = c[0];
		eq.dec = c[1];
		break;
	case 6:
		eq.ra  = (c[0] + c[1] / 60.0 + c[2] / 3600.0);
		eq.dec = (c[3] + c[4] / 60.0 + c[5] / 3600.0);
		break;
	default:
		goto cleanup;
	}

	sky_append_object(p, group, eq, SKY_OBJ_SIZE);

cleanup:
	g_free(c);
}


/**
 * @brief add a new sky object for galactic coordinates
 */

static void sky_add_object_gal(Sky *p, GKeyFile *kf, gchar *group)
{
	gsize len;
	gdouble *c;

	GError *error = NULL;

	struct coord_galactic ga;


	c = g_key_file_get_double_list(kf, group, "ga", &len, &error);
	if (error) {
		g_warning(error->message);
		g_error_free(error);
	}

	switch (len) {
	case 2:
		ga.lon = c[0];
		ga.lat = c[1];
		break;
	case 6:
		ga.lon = (c[0] + c[1] / 60.0 + c[2] / 3600.0);
		ga.lat = (c[3] + c[4] / 60.0 + c[5] / 3600.0);
		break;
	default:
		goto cleanup;
		break;
	}

	sky_append_object(p, group, galactic_to_equatorial(ga), SKY_OBJ_SIZE);

cleanup:
	g_free(c);
}


/**
 * @brief add a new sky object from the config file
 */

static void sky_add_object(Sky *p, GKeyFile *kf, gchar *group)
{
	if (g_key_file_has_key(kf, group, "ga",  NULL))
		sky_add_object_gal(p, kf, group);

	if (g_key_file_has_key(kf, group, "eq",  NULL))
		sky_add_object_eq(p, kf, group);
}



/**
 * @brief load configuration keys
 */

static void sky_load_keys(Sky *p, GKeyFile *kf)
{
	gchar **g;
	gchar **groups;



	/* all coordinates are separated by space */
	g_key_file_set_list_separator(kf, ' ');

	groups = g_key_file_get_groups(kf, NULL);
	g = groups;

	while ((*g)) {
		sky_add_object(p, kf, (*g));
		g++;
	}


	g_strfreev(groups);
}


/**
 * @brief load the configuration file
 */

static int sky_load_config(Sky *p)
{
	gboolean ret;

	GKeyFile *kf;
	GKeyFileFlags flags;

	GError *error = NULL;



	kf = g_key_file_new();
	flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;


	ret = g_key_file_load_from_file(kf, "config/sky_objects.cfg",
					flags, &error);

	if (!ret) {
		g_error("error loading config file %s", error->message);
		g_clear_error(&error);
		return -1;
	}

	sky_load_keys(p, kf);

	g_key_file_free(kf);

	return 0;
}


/**
 * @brief calculate single precision canvas coordinates from horizontal in a
 *	  polar projection
 * @param hor horizontal system coordinates of type struct coord_horizontal
 * @param xc the central x coordinate
 * @param yc the central y coordinate
 * @param r the pixel radius of the confining circle
 * @param[out] x the output x coordinate
 * @param[out] x the output x coordinate
 *
 * @note inputs are allowed as double precision on purpose
 */

static void sky_horizontal_to_canvas_f(struct coord_horizontal hor,
					  const double xc, const double yc,
					  const double r,
					  float *x, float *y)
{
	const float scale = (float) 1.0 / 90.0 * r;
	const float z     = (float) r - scale * hor.el;


	hor.az = 180.0 - hor.az;

	(*x) = xc - z * sinf((float) RAD(hor.az));
	(*y) = yc + z * cosf((float) RAD(hor.az));
}

/**
 * @brief write text with center at x/y coordinate
 *
 * @param cr the cairo context to draw on
 * @param x the x coordinate
 * @param y the y coordinate
 * @param buf a pointer to the text buffer
 */

static void sky_write_text_centered(cairo_t *cr,
			       const double x, const double y, const char *buf)
{
	cairo_text_extents_t te;


	cairo_save(cr);

	cairo_text_extents(cr, buf, &te);

	cairo_move_to(cr, x - te.width * 0.5 , y - te.height * 0.5);

	cairo_show_text(cr, buf);

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief write text with rotation
 *
 * @param cr the cairo context to draw on
 * @param x the x coordinate
 * @param y the y coordinate
 * @param buf a pointer to the text buffer
 * @param rot a text rotation
 */

static void sky_write_text(cairo_t *cr,
			       const double x, const double y,
			       const char *buf, const double rot)
{
	cairo_save(cr);

	cairo_move_to(cr, x, y);

	cairo_rotate(cr, rot);
	cairo_show_text(cr, buf);

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw a circle
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r circle radius
 */

static void sky_draw_circle(cairo_t *cr,
				const double x, const double y, const double r)
{
	cairo_save(cr);

	cairo_arc(cr, x, y, r, 0.0, 2.0 * M_PI);
	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw a filled circle
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r circle radius
 */

static void sky_draw_circle_filled(cairo_t *cr,
				       const double x, const double y,
				       const double r)
{
	cairo_save(cr);

	cairo_arc(cr, x, y, r, 0.0, 2.0 * M_PI);

	cairo_fill(cr);
	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw the telescope pointing boundary
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r circle radius
 * @param lim a 2-element array of limiting coordiates of
 *	  struct coord_horizontal
 *
 * @note coordinate order is {"left", "lower"} - {"right", "upper}
 */

static void sky_draw_pointing_limits(cairo_t *cr,
				     const double x, const double y,
				     const double r,
				     const struct coord_horizontal lim[2])
{
	const double scale = 1.0 / 90.0 * r;


	cairo_save(cr);

	cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);

	cairo_arc_negative(cr, x, y,
			   (90.0 - lim[0].el) * scale,
			   RAD(270.0 - lim[0].az), RAD(270.0f - lim[1].az));
	cairo_arc(cr, x, y,
		   (90.0f - lim[1].el) * scale,
		   RAD((270.0f - lim[1].az)), RAD(270.0f - lim[0].az));


	cairo_close_path(cr);
	cairo_stroke_preserve(cr);

	/* shade inaccessible area */
	cairo_arc(cr, x, y, r, 0.0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.1); /* shade color */

	cairo_fill(cr);

	cairo_restore(cr);
}


/**
 * @brief draw the telescope's local horizon profile
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r circle radius
 * @param hor an array of struct local_horizon
 * @param n  the number of elements in hor
 *
 * @note coordinate order is {"left", "lower"} - {"right", "upper}
 */

static void sky_draw_local_horizon(cairo_t *cr,
				   const double xc, const double yc,
				   const double r,
				   struct local_horizon *loc,
				   size_t n)
{
	guint32 i;

	float x, y, x0, y0;

	double j, k, steps;

	struct coord_horizontal hor;


	if (!n)
		return;


	cairo_save(cr);

	cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);



	hor.az = loc[0].az;
	hor.el = loc[0].el;
	sky_horizontal_to_canvas_f(hor, xc, yc, r, &x, &y);


	cairo_move_to(cr, (double) x, (double) y);
	x0 = x;
	y0 = y;

	for (i = 0; i < n; i++) {

		if ((i + 1) == n) {
			steps = trunc(360.0f - loc[i].az);
			k     = (gdouble) (loc[0].el - loc[i].el);
		} else {
			steps = (gdouble) (loc[i + 1].az - loc[i].az);
			k     = (gdouble) (loc[i + 1].el - loc[i].el);
		}

		k /= steps;

		hor.az = loc[0].az;
		hor.el = loc[0].el;


		/* linear interpolation; increment is arbitrary (4 deg is
		 * sufficient)
		 */
		for (j = 0.0; j <= steps; j += 4.0) {
			hor.az = (gdouble) loc[i].az + j;
			hor.el = (gdouble) loc[i].el + j * k;

			sky_horizontal_to_canvas_f(hor, xc, yc, r, &x, &y);

			cairo_rel_line_to(cr, x - x0, y - y0);

			x0 = x;
			y0 = y;
		}
	}

	cairo_close_path(cr);
	cairo_stroke_preserve(cr);

	/* shade "true" horizon inaccessible area */
	cairo_arc(cr, xc, yc, r, 0.0, 2.0 * M_PI);
	cairo_set_source_rgba(cr, 0.0, 0.6, 0.0, 0.1); /* shade color */

	cairo_fill(cr);

	cairo_restore(cr);
}




/**
 * @brief draw lines between points in an array withing a polar projection of
 *        the sky
 * @param cr the cairo context to draw on
 * @param points an array of points of struct coord_galactic
 * @param n the number of points in the array
 * @param stepsize the array step size
 * @param r the pixel radius of the confining circle
 * @param x the central x coordinate
 * @param y the central y coordinate
 *
 * @note the maximum length of a line segment is limited to r/2
 * @note the cairo line parameters must be set by the caller
 */

static void sky_draw_array_gal(cairo_t *cr,
			       const struct coord_galactic *points,
			       const size_t n, const size_t stepsize,
			       const double xc, const double yc,
			       const double r,
			       const double lat, const double lon,
			       const double hour_angle_shift)
{
	size_t i;

	float x, y, x0, y0;

	float delta;

	gboolean line_start = FALSE;

	struct coord_horizontal hor;

	/* implausible line lenght; more or less arbitray
	 * length comparison is non-normalised
	 */
	const float delta_len_max = (r / 2.0) * (r / 2.0);


	cairo_save(cr);


	for (i = 0; i < n; i+= stepsize) {

		hor = galactic_to_horizontal(points[i],
					     lat, lon, hour_angle_shift);

		if (hor.el < 0.0f || hor.el > 90.0f)
			continue;

		sky_horizontal_to_canvas_f(hor, xc, yc, r, &x, &y);

		cairo_move_to(cr, (double) x, (double) y);

		if (line_start) {

			delta = (x - x0) * (x - x0) + (y - y0) * (y - y0);

			if (delta > delta_len_max) {
				line_start = FALSE;
				continue;
			}

			cairo_line_to(cr, (double) x0, (double) y0);
		}

		line_start = TRUE;

		x0 = x;
		y0 = y;
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw lines between points in an array withing a polar projection of
 *        the sky
 * @param cr the cairo context to draw on
 * @param points an array of points of struct coord_equatorial
 * @param n the number of points in the array
 * @param stepsize the array step size
 * @param r the pixel radius of the confining circle
 * @param x the central x coordinate
 * @param y the central y coordinate
 *
 * @note the maximum length of a line segment is limited to r/2
 * @note the cairo line parameters must be set by the caller
 */

static void sky_draw_array_eq(cairo_t *cr,
			       const struct coord_equatorial *points,
			       const size_t n, const size_t stepsize,
			       const double xc, const double yc,
			       const double r,
			       const double lat, const double lon,
			       const double hour_angle_shift)
{
	size_t i;

	float x, y, x0, y0;

	float delta;

	gboolean line_start = FALSE;

	struct coord_horizontal hor;

	/* implausible line lenght; more or less arbitray
	 * length comparison is non-normalised
	 */
	const float delta_len_max = (r / 2.0) * (r / 2.0);


	cairo_save(cr);


	for (i = 0; i < n; i+= stepsize) {

		hor = equatorial_to_horizontal(points[i],
					     lat, lon, hour_angle_shift);

		if (hor.el < 0.0f || hor.el > 90.0f)
			continue;

		sky_horizontal_to_canvas_f(hor, xc, yc, r, &x, &y);

		cairo_move_to(cr, (double) x, (double) y);

		if (line_start) {

			delta = (x - x0) * (x - x0) + (y - y0) * (y - y0);

			if (delta > delta_len_max) {
				line_start = FALSE;
				continue;
			}

			cairo_line_to(cr, (double) x0, (double) y0);
		}

		line_start = TRUE;

		x0 = x;
		y0 = y;
	}


	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 *
 * @brief generate the galactic plane in equatorial coordinates
 *
 * @note for reference only
 */

void sky_gen_gal_plane_equatorial(void)
{
	int i;

	float xg, yg;
	float xr, yr, zr;

	struct coord_equatorial eq;


	for (i = 0; i < 360; i++) {

		/* point along the unit circle */
		xg = cosf(RAD(i));
		yg = sinf(RAD(i));

		/* rotate relative to sky equator */
		xr = xg * sinf(RAD(27.1f));
		yr = yg;
		zr = -xg * cosf(RAD(27.1f));

		/* project to sky and convert to horizontal system */
		eq.ra  = atan2f(yr, xr) + (12.0f + 51.4f / 60.0f) * M_PI / 12.0f;
		eq.dec = atan2f(zr, sqrtf(xr * xr + yr * yr));

		eq.ra  = DEG_TO_HOUR(DEG(eq.ra));

		if (eq.ra > 24.0)
			eq.ra -= 24.0;

		eq.dec = DEG(eq.dec);

		printf("{%9.3f, %9.3f},\n ", eq.ra, eq.dec);

		if ((i + 1) % 3 == 0)
			printf("\n");
	}
}


/**
 * @brief draw catalog objects
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 */

static void sky_draw_cat_objects(Sky *p, cairo_t *cr)
{
	struct sky_obj *obj;

	GList *l;


	cairo_save(cr);

	l = p->cfg->obj;

	while (l) {
		obj = l->data;

		/* default */
		cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);

		/* yellow Sun */
		if (!strncmp("Sun", obj->name, 3))
			cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);

		/* red selection */
		if (obj->selected)
			cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);


		if (obj->hor.el > 0.0) {

			sky_horizontal_to_canvas_f(obj->hor, p->cfg->xc, p->cfg->yc,
						   p->cfg->r,
						   &obj->x, &obj->y);

			sky_draw_circle_filled(cr, obj->x, obj->y, obj->radius);

			sky_write_text(cr,
				       obj->x + obj->radius + 5.0, obj->y,
				       obj->name, 0.0);

		}

		l = l->next;
	}


	cairo_restore(cr);
}


/**
 * @brief draw current pointing and target pointing
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 */

static void sky_draw_pointing(Sky *p, cairo_t *cr)
{
	float x, y;

	cairo_save(cr);



	sky_horizontal_to_canvas_f(p->cfg->pos,
				   p->cfg->xc, p->cfg->yc, p->cfg->r,
				   &x, &y);

	cairo_set_source_rgba(cr, 0.64, 0.73, 0.24, 0.2);
	sky_draw_circle_filled(cr, x, y, 8.);

	cairo_set_source_rgba(cr, 0.64, 0.73, 0.24, 1.0);
	sky_draw_circle(cr, x, y, 8.);

	sky_write_text(cr, x + 15., y + 5., "POS", 0.0);


	/* do not draw when tgt el is > -90.0 */
	if (p->cfg->tgt.el > -90.) {

		sky_horizontal_to_canvas_f(p->cfg->tgt,
					   p->cfg->xc, p->cfg->yc, p->cfg->r,
					   &x, &y);

		cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
		cairo_rectangle(cr, x - 10.0, y - 10.0, 20.0, 20.0);
		cairo_stroke(cr);

		sky_write_text(cr, x - 35., y + 5., "TGT", 0.0);
	}



	cairo_restore(cr);
}


/**
 * @brief draw the galactic plane
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 */

static void sky_draw_galactic_plane(cairo_t *cr,
				    const double xc, const double yc,
				    const double r,
				    const double lat, const double lon,
				    const double hour_angle_shift)
{
	const size_t step = 5;

	const double dashes[] = {10.0, 10.0};


	cairo_save(cr);

	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);

	cairo_set_source_rgb(cr, 0.2, 0.5, 1.0);
	cairo_set_line_width(cr, 2.0);

	sky_draw_array_eq(cr, gal_plane_eq, ARRAY_SIZE(gal_plane_eq), step,
			      xc, yc, r, lat, lon, hour_angle_shift);

	cairo_set_dash(cr, NULL, 0 , 0.0);

	cairo_restore(cr);
}


/**
 * @brief draw an outline of the milky way
 * @param cr the cairo context to draw on
 * @param r the pixel radius of the confining circle
 * @param x the central x coordinate
 * @param y the central y coordinate
 */

static void sky_draw_milkyway(cairo_t *cr,
				  const double xc, const double yc,
				  const double r,
				  const double lat, const double lon,
				  const double hour_angle_shift)
{
	cairo_save(cr);

	cairo_set_source_rgb(cr, 0.2, 0.5, 1.0);
	cairo_set_line_width(cr, 1.0);

	sky_draw_array_gal(cr, milky_way_1_gal, ARRAY_SIZE(milky_way_1_gal),
			       1, xc, yc, r, lat, lon, hour_angle_shift);

	sky_draw_array_gal(cr, milky_way_2_gal, ARRAY_SIZE(milky_way_2_gal),
			       1, xc, yc, r, lat, lon, hour_angle_shift);

	cairo_stroke(cr);

	cairo_restore(cr);
}


/**
 * @brief draw radial ticks
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r0 the inner tick radius
 * @param r1 the outer tick radius
 * @param nticks the number of ticks
 *
 */

static void sky_draw_grid_radial_ticks(cairo_t *cr,
					   const double x, const double y,
					   const double r0, const double r1,
					   const unsigned int nticks)
{
	int i;

	double angle;
	double cos_rot;
	double sin_rot;


	cairo_save(cr);

	for (i = 0; i < nticks; i++) {

		angle = ((double) i) * 2.0 * M_PI / ((double) nticks);

		cos_rot = cos(angle);
		sin_rot = sin(angle);

		cairo_line_to(cr, x + r0 * cos_rot,
				  y + r0 * sin_rot);

		cairo_line_to(cr, x + r1 * cos_rot,
				  y + r1 * sin_rot);

		cairo_stroke(cr);
	}

	cairo_restore(cr);
}


/**
 * @brief draw labels to the radial ticks
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r0 the inner tick radius
 * @param r1 the outer tick radius
 * @param nticks the number of ticks
 *
 * @todo some optimisation could not hurt
 */

static void sky_draw_grid_radial_ticks_labels(cairo_t *cr,
					   const double x, const double y,
					   const double r0, const double r1,
					   const unsigned int nticks)
{
	int i;
	int deg;
	int deg_inc;
	int label_rot;

	double r;
	double r_corr;
	double angle;
	double angle_corr;
	double cos_rot;
	double sin_rot;

	char buf[32];

	cairo_text_extents_t te;


	cairo_save(cr);

	/* angle correction for label centering and one-character offset
	 * along radial line
	 */
	snprintf(buf, ARRAY_SIZE(buf), "%g", 0);
	cairo_text_extents(cr, buf, &te);

	r_corr     =  te.height;
	angle_corr = (0.5 * te.height / r1);


	deg_inc = 360 / nticks;

	for (i = 0; i < nticks; i++) {

		deg = (180 + deg_inc * i) % 360;

		angle = ((double) i) * 2.0 * M_PI / ((double) nticks);

		if (deg > 180) {
			cos_rot = cos(angle - angle_corr);
			sin_rot = sin(angle - angle_corr);
		} else {
			cos_rot = cos(angle + angle_corr);
			sin_rot = sin(angle + angle_corr);
		}

		label_rot = 360 - (90 + deg_inc * i) % 360;

		snprintf(buf, ARRAY_SIZE(buf), "%g", (double) deg);
		cairo_text_extents(cr, buf, &te);

		r = r1 + r_corr;

		if (deg > 180)
			label_rot -= 180;
		else
			r += te.width;


		sky_write_text(cr,
				   x + r * sin_rot,
				   y + r * cos_rot,
				   buf,
				   ((double) label_rot) * M_PI / 180.0);

		cairo_stroke(cr);
	}

	cairo_restore(cr);
}


/**
 * @brief draw radial ticks and labels
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r0 the inner tick radius
 * @param r1 the outer tick radius
 * @param nticks the number of ticks
 */

static void sky_draw_grid_radial(cairo_t *cr,
				     const double x, const double y,
				     const double r0, const double r1,
				     const unsigned int nticks)
{
	cairo_save(cr);

	sky_draw_grid_radial_ticks(cr, x, y, r0, r1, nticks);

	sky_draw_grid_radial_ticks_labels(cr, x, y, r0, r1, nticks);

	cairo_restore(cr);
}


/**
 * @brief draw an angular grid
 *
 * @param cr the cairo context to draw on
 * @param x the central x coordinate
 * @param y the central y coordinate
 * @param r the outer radius of the grid
 *
 * @note the radius does not include the ticks labels
 */

static void sky_draw_grid_angular(cairo_t *cr,
				      const double x, const double y,
				      const double r)
{
	char buf[32];

	const double dashes[] = {2.0, 2.0};


	cairo_save(cr);


	/* draw colour */
	cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);

	/* four circles representing 90 * 1/4  = 22.5 degree steps */
	sky_draw_circle(cr, x, y, r);
	sky_draw_circle(cr, x, y, r * 3./4.);
	sky_draw_circle(cr, x, y, r * 2./4.);
	sky_draw_circle(cr, x, y, r * 1./4.);

	/* grid labels */
	snprintf(buf, ARRAY_SIZE(buf), "%g", 90.0 * 1.0 / 4.0);
	sky_write_text_centered(cr, x, y + r * 3.0 / 4.0, buf);

	snprintf(buf, ARRAY_SIZE(buf), "%g", 90.0 * 2.0 / 4.0);
	sky_write_text_centered(cr, x, y + r * 2.0 / 4.0, buf);

	snprintf(buf, ARRAY_SIZE(buf), "%g", 90.0 * 3.0 / 4.0);
	sky_write_text_centered(cr, x, y + r * 1.0 /4.0, buf);

	/* draw a a dashed radial grid with labels */
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0.0);
	sky_draw_grid_radial(cr, x, y, r * 1.0 / 30.0, r, 12);
	cairo_set_dash(cr, NULL, 0 , 0.0);

	/* draw a white zenith marker */
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	sky_draw_circle_filled(cr, x, y, r * 1.0 / 100.0);

	cairo_restore(cr);
}


/**
 * @brief create a pango layout of a buffer
 *
 * @param cr the cairo context
 * @param buf a text buffer
 * @param len the text length
 *
 * @return a PangoLayout
 */

static PangoLayout *sky_create_layout(cairo_t *cr,
					  const char *buf,
					  const size_t len)
{
	PangoLayout *layout;

	layout = pango_cairo_create_layout(cr);
	pango_layout_set_markup(layout, buf, len);

	return layout;
}


/**
 * @brief render and release a PangoLayout
 *
 * @param cr a cairo context
 * @param layout a pango layout
 * @param x a x coordinate
 * @param y a y coordinate
 */

static void sky_render_layout(cairo_t *cr, PangoLayout *layout,
				  const int x, const int y)
{
	cairo_save(cr);
	cairo_move_to(cr, x, y);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	cairo_restore(cr);
}


/**
 * @brief convert plot-center relativ x/y coordinates to horizontal azel
 *
 * @param p the Sky widget
 * @param x the x coordinate relative to the plot center
 * @param y the y coordinate relative to the plot center
 *
 * @returns a struct coord_horizontal
 */

struct coord_horizontal sky_xy_rel_to_horizontal(Sky *p, double x, double y)
{
	double r, phi;

	struct coord_horizontal hor;


	r = sqrt(x * x + y * y);

	phi = acos(x / r) * 180.0 / M_PI ;

	if(y < 0.0)
		phi *= -1.0;

	phi += 270.;	/* rotate to our system */

	/* avoid nan by adding a some small number */
	phi = fmod(phi, 360.0) + 1e-20;

	/* set to 0.0 if phi is really small */
	if (phi < 1e-10 && phi > -1e-10)
		phi = 0.0;

	/* correct scale */
	r = 90.0 - r / p->cfg->r * 90.0;

	hor.az = phi;
	hor.el = r;

	return hor;
}


/**
 * @brief create a coordinate info text layout
 *
 * @param cr the cairo context
 * @param x the x coordinate relative to the plot center
 * @param y the y coordinate relative to the plot center
 *
 * @return a PangoLayout
 */

static PangoLayout *sky_coord_info_layout(cairo_t *cr, Sky *p,
					  double x, double y)
{

	struct coord_horizontal hor;
	struct coord_equatorial eq;
	struct coord_galactic   gal;

	char buf[256];


	hor = sky_xy_rel_to_horizontal(p, x, y);

	eq  = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon,
				       p->cfg->time_off);
	gal = equatorial_to_galactic(eq);

	snprintf(buf, ARRAY_SIZE(buf),
		 "<span foreground='#7AAA7E'"
		 "	background='#000000'"
		 "	font_desc='Sans Bold 12'>"
		 "<tt>"
		 "AZ   %+7.2f°\n"
		 "EL   %+7.2f°\n"
		 "RA   %+7.2f°\n"
		 "DE   %+7.2f°\n"
		 "GLAT %+7.2f°\n"
		 "GLON %+7.2f°\n"
		 "VLSR %+7.2f kms<sup>-1</sup> "
		 "</tt>"
		 "</span>",
		 hor.az, hor.el, eq.ra, eq.dec, gal.lat, gal.lon,
		 vlsr(eq, 0.0));

	return sky_create_layout(cr, buf, strlen(buf));
}


/**
 * @brief create a time info text layout
 *
 * @param cr the cairo context
 *
 * @return a PangoLayout
 */

static PangoLayout *sky_time_info_layout(cairo_t *cr, double time_offset)
{
	double h, m;

	char buf[256];


	h = trunc(time_offset);
	m = (time_offset - h) * 60.0;

	snprintf(buf, ARRAY_SIZE(buf),
		 "<span foreground='#7AAA7E'"
		 "	background='#000000'"
		 "	font_desc='Sans Bold 12'>"
		 "<tt>"
		 "TIME %+03.0fh %+06.2fm"
		 "</tt>"
		 "</span>",
		 h, m);

	return sky_create_layout(cr, buf, strlen(buf));
}

#if 0
/**
 * @brief create reset button text layout
 *
 * @param cr the cairo context
 *
 * @return a PangoLayout
 */

static PangoLayout *sky_reset_button_layout(cairo_t *cr)
{
	char buf[256];


	snprintf(buf, ARRAY_SIZE(buf),
		 "<span foreground='#FF0000'"
		 "	background='#000000'"
		 "	font_desc='Sans Bold 12'>"
		 "<tt>"
		 "RST"
		 "</tt>"
		 "</span>");

	return sky_create_layout(cr, buf, strlen(buf));
}


/**
 * @brief draw a rectangle around the reset button
 *
 * @param cr the cairo context
 * @param x the x pixel coordinate of the top left corner
 * @param y the y pixel coordinate of the top left corner
 * @param w the width in pixels
 * @param h the height in pixels
 */

static void sky_reset_button_rectangle(cairo_t *cr,
					   const double x, const double y,
					   const double w, const double h)
{

	cairo_save(cr);

	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 2.0);

	cairo_rectangle(cr, x, y, w, h);

	cairo_stroke(cr);

	cairo_restore(cr);
}
#endif


/**
 * @brief render info text
 */

static void sky_draw_mouse_coord(cairo_t *cr, Sky *p)
{
	gint coord_width;
	gint text_height;

	PangoLayout *layout;


	if (!p->cfg->mptr.inside)
		return;


	layout = sky_coord_info_layout(cr, p, p->cfg->mptr.x, p->cfg->mptr.y);

	pango_layout_get_pixel_size(layout, &coord_width, &text_height);

	/* insert 1 line heigth of padding from top (we have 7 lines) */
	sky_render_layout(cr, layout,
			      (p->cfg->width - coord_width),
			      text_height / 7);
}


/**
 * @brief render time offset/reset
 *
 */

static void sky_draw_time_rst(cairo_t *cr, Sky *p)
{
	gint w;
	gint w_off;
	gint text_height;
	gint x0, y0;
	gint wh, ww;
	gint h;


	PangoLayout *layout;

	GtkWidget *b;
	GtkWidget *off;



	if (p->cfg->time_off == 0.0)
		return;

#if 0
	layout = sky_time_info_layout(cr, p->cfg->time_off);

	pango_layout_get_pixel_size(layout, &w, &text_height);

	w_off = w * 1.1;

	/* keep same left align as above */
	sky_render_layout(cr, layout,
			  (p->cfg->width  - w_off),
			  (p->cfg->height - text_height));


	layout = sky_reset_button_layout(cr);

	pango_layout_get_pixel_size(layout, &w, &text_height);
	sky_render_layout(cr, layout,
			  (p->cfg->width  - w_off * 1.1) - w, /* ugh... */
			  (p->cfg->height - text_height));

	sky_reset_button_rectangle(cr,
				   p->cfg->width  - w_off * 1.1 - w * 1.1,
				   p->cfg->height - text_height,
				   w * 1.1,
				   text_height);

	/*** POSSIBLE ISSUE ***/
	p->cfg->rst.x0 = (int) (p->cfg->width - w_off * 1.1 - w * 1.1);
	p->cfg->rst.x1 = (int) (p->cfg->rst.x0 + w * 1.1);

	p->cfg->rst.y0 = (int) (p->cfg->height - text_height);
	p->cfg->rst.y1 = (int) (p->cfg->rst.y0 + text_height);

#else
	/* render offscreen button to cairo context */
	off = gtk_offscreen_window_new();
	b = gtk_button_new_with_label("Reset");
	gtk_container_add(GTK_CONTAINER(off), b);
	gtk_widget_show_all(off);

	wh = gtk_widget_get_allocated_height(b);
	ww = gtk_widget_get_allocated_width(b);


	layout = sky_time_info_layout(cr, p->cfg->time_off);

	pango_layout_get_pixel_size(layout, &w, &text_height);

	w_off = w * 1.1;

	h = (text_height + wh);

	/* keep same left align as above */
	sky_render_layout(cr, layout,
			  (p->cfg->width  - w_off),
			  (p->cfg->height - h / 2));


	x0 = p->cfg->width - w_off + w - ww ;
	y0 = p->cfg->height - h - wh / 2;

	cairo_save(cr);
	cairo_translate(cr, x0, y0);

	gtk_widget_draw(b, cr);
	cairo_restore(cr);

	p->cfg->rst.x0 = x0;
	p->cfg->rst.x1 = x0 + ww;

	p->cfg->rst.y0 = y0;
	p->cfg->rst.y1 = y0 + wh;

	gtk_widget_destroy(off);
#endif
}



/**
 * @brief draw the background
 *
 * @param cr the cairo context to draw on
 */

static void sky_draw_bg(cairo_t *cr)
{
	cairo_save(cr);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_paint(cr);

	cairo_set_source_rgba(cr,1.0, 1.0, 1.0, 1.0);
	cairo_stroke(cr);

	cairo_restore(cr);
}



/**
 * @brief draws the plot
 *
 * @note the plot is rendered onto it's own surface, so we don't need
 *	 to redraw it every time we want to add or update an overlay
 */

static void sky_plot(GtkWidget *w)
{
	double min;

	Sky *p;
	cairo_t *cr;


	p = SKY(w);

	cr = cairo_create(p->cfg->plot);


	p->cfg->width  = gtk_widget_get_allocated_width(w);
	p->cfg->height = gtk_widget_get_allocated_height(w);

	min = (gint) MIN(p->cfg->width, p->cfg->height);

	p->cfg->xc = min / 2;
	p->cfg->yc = p->cfg->height / 2;

	cairo_set_line_width(cr, 0.5);
	p->cfg->r = ((double) min) / 2.2;

	sky_draw_bg(cr);

	sky_draw_grid_angular(cr, p->cfg->xc, p->cfg->yc, p->cfg->r);


	sky_draw_milkyway(cr, p->cfg->xc, p->cfg->yc, p->cfg->r,
			  p->cfg->lat, p->cfg->lon, p->cfg->time_off);

	sky_draw_galactic_plane(cr, p->cfg->xc, p->cfg->yc, p->cfg->r,
				p->cfg->lat, p->cfg->lon, p->cfg->time_off);

	sky_draw_cat_objects(p, cr);

	sky_draw_pointing_limits(cr, p->cfg->xc, p->cfg->yc, p->cfg->r,
				 p->cfg->lim);

	sky_draw_local_horizon(cr, p->cfg->xc, p->cfg->yc, p->cfg->r,
			       p->cfg->local_hor, p->cfg->n_local_hor);

	sky_draw_pointing(p, cr);

	sky_draw_time_rst(cr, p);

	sky_draw_mouse_coord(cr, p);

	cairo_destroy(cr);

	/* duplicate to render surface */
	cr = cairo_create(p->cfg->render);
	cairo_set_source_surface(cr, p->cfg->plot, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);


	gtk_widget_queue_draw(w);
}


/**
 * @brief the draw signal callback
 *
 * @note draws the current render surface, which is typically the plot surface
 *	 and optionally some overlay
 */

static gboolean sky_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	Sky *p;

	p = SKY(widget);

	cairo_set_source_surface(cr, p->cfg->render, 0.0, 0.0);

	cairo_paint(cr);


	return TRUE;
}


/**
 * @brief handle mouse cursor enter/leave events
 */

static gboolean sky_pointer_crossing_cb(GtkWidget *widget,
					GdkEventCrossing *event)
{
	GdkWindow  *window;
	GdkDisplay *display;
	GdkCursor  *cursor;


	display = gtk_widget_get_display(widget);
	window  = gtk_widget_get_window(widget);


	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		cursor = gdk_cursor_new_from_name(display, "cell");
		break;
	case GDK_LEAVE_NOTIFY:
	default:
		cursor = gdk_cursor_new_from_name(display, "default");
		break;
	}


	gdk_window_set_cursor(window, cursor);

	g_object_unref (cursor);

	return TRUE;
}





/**
 * @brief check if the RST button was clicked and re-render plot
 */

static void sky_button_reset_time(GtkWidget *widget, GdkEventButton *event)
{
	Sky *p;


	p = SKY(widget);

	/* click within RST area? */
	if (p->cfg->rst.x0 >= event->x)
		return;

	if (p->cfg->rst.x1 <= event->x)
		return;

	if (p->cfg->rst.y0 >= event->y)
		return;

	if (p->cfg->rst.y1 <= event->y)
		return;


	/* reset time offset */
	p->cfg->time_off = 0.0;
	sky_update_coord_hor((gpointer) p);
	sky_plot(widget);
}


/**
 * @brief sky object selection/deselection
 */

static void sky_selection(GtkWidget *widget, GdkEventButton *event)
{
	gdouble px, py;

	Sky *p;

	struct sky_obj *obj;
	GList *l;



	p = SKY(widget);

	px = event->x - p->cfg->xc;
	py = p->cfg->yc - event->y;

	/* inside sky plot? */
	if (px * px + py * py > p->cfg->r * p->cfg->r)
		return;


	/* if something was selected, it was tracked. signal disable */
	if (p->cfg->sel)
		g_signal_emit_by_name(sig_get_instance(), "tracking", FALSE,
				      p->cfg->sel->hor.az, p->cfg->sel->hor.el);
	/* deselect all */

	p->cfg->sel = NULL;

	for (l = p->cfg->obj; l; l = l->next) {
		obj = l->data;
		obj->selected = FALSE;
	}

	/* just deselect if ctrl was not held down */
	if (!(event->state & GDK_CONTROL_MASK))
		return;

	/* select at most one */
	for (l = p->cfg->obj; l; l = l->next) {

		obj = l->data;

		if (obj->x - obj->radius > event->x)
			continue;

		if (obj->x + obj->radius < event->x)
			continue;

		if (obj->y + obj->radius < event->y)
			continue;

		if (obj->y - obj->radius > event->y)
			continue;

		g_debug("Selected object: %s, RA: %g DEC: %g",
			  obj->name, obj->eq.ra, obj->eq.dec);

		obj->selected = TRUE;
		p->cfg->sel = obj;
		break;
	}

	sky_plot(widget);
}

/**
 * @brief button press events
 */

static gboolean sky_button_press_cb(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	gdouble px, py;

	struct coord_horizontal hor;

	Sky *p;




	p = SKY(widget);

	if (event->type != GDK_BUTTON_PRESS)
		goto exit;

	if (event->button == 1) {

		sky_selection(widget, event);

		if (event->state & GDK_CONTROL_MASK) {


			/* get plot center reference */
			px = event->x - p->cfg->xc;
			py = p->cfg->yc - event->y;

			if ((px * px + py * py) > p->cfg->r * p->cfg->r)
				return TRUE;

			if (p->cfg->sel) {
				sky_update_tracked_pos(p);
				return TRUE;
			}

			hor = sky_xy_rel_to_horizontal(p, px, py);

			/* always disable tracking, but update anyway */
			g_signal_emit_by_name(sig_get_instance(),
					      "tracking", FALSE,
					      hor.az, hor.el);

			/* ignore click if outside of axis range, the external
			 * tracker will take care of selected objects
			 */

			if (hor.az < p->cfg->lim[0].az)
				return TRUE;

			if (hor.el < p->cfg->lim[0].el)
				return TRUE;

			if (hor.az > p->cfg->lim[1].az)
				return TRUE;

			if (hor.el > p->cfg->lim[1].el)
				return TRUE;


			cmd_moveto_azel(PKT_TRANS_ID_UNDEF, hor.az, hor.el);

			return TRUE;
		}

		sky_button_reset_time(widget, event);
	}

	/* save x coordinates where button 3 was depressed */
	if (event->button == 3)
		p->cfg->mb3_x = event->x;

exit:
	return TRUE;
}



/**
 * @brief handle mouse cursor motion events
 */

static gboolean sky_motion_notify_event_cb(GtkWidget *widget,
					   GdkEventMotion *event, gpointer data)
{
	gdouble px, py;

	Sky *p;
	cairo_t *cr;


	p = SKY(widget);


	if (!event->is_hint)
		goto exit;

	if (!event->device)
		goto exit;

	if (event->state & GDK_BUTTON3_MASK) {
		p->cfg->time_off -= 10.0 * (p->cfg->mb3_x - event->x) / p->cfg->r;
		p->cfg->mb3_x = event->x;

		/* redraw with new time */
		sky_update_coord_hor((gpointer) p);
		sky_plot(widget);
	}


	cr = cairo_create(p->cfg->render);

	/* paint plot surface to render surface */
	cairo_set_source_surface(cr, p->cfg->plot, 0, 0);
	cairo_paint(cr);
	cairo_set_source_surface(cr, p->cfg->render, 0, 0);



	/* get plot center reference */
	px = event->x - p->cfg->xc;
	py = p->cfg->yc - event->y;

	if (px * px + py * py > p->cfg->r * p->cfg->r) {
		p->cfg->mptr.inside = FALSE;
		sky_plot(GTK_WIDGET(p));
		goto cleanup;
	}


	/* get data range reference */
	p->cfg->mptr.x = px;
	p->cfg->mptr.y = py;
	p->cfg->mptr.inside = TRUE;


	sky_draw_mouse_coord(cr, p);

cleanup:
	cairo_destroy(cr);


	/* _draw_area() may leave artefacts if the pointer is moved to fast */
	gtk_widget_queue_draw(widget);

exit:
	return TRUE;
}


/**
 * @brief create a new surface on configure event
 */

static gboolean sky_configure_event_cb(GtkWidget *w,
				       GdkEventConfigure *event,
				       gpointer data)
{
	guint width, height;

	Sky *p;

	GdkWindow *win;


	win = gtk_widget_get_window(w);

	if (!win)
		goto exit;


	p = SKY(w);

	if (p->cfg->render)
	    cairo_surface_destroy(p->cfg->render);


	width  = gtk_widget_get_allocated_width(w);
	height = gtk_widget_get_allocated_height(w);

	p->cfg->plot = gdk_window_create_similar_surface(win,
							 CAIRO_CONTENT_COLOR,
							 width, height);

	p->cfg->render = gdk_window_create_similar_surface(win,
							   CAIRO_CONTENT_COLOR,
							   width, height);

	sky_update_coord_hor((gpointer) p);
	sky_plot(w);

exit:
	return TRUE;
}


/**
 * @brief destroy signal handler
 */

static gboolean sky_destroy(GtkWidget *w, void *data)
{
	Sky *p;

	p = SKY(data);


	g_source_remove(p->cfg->id_to);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cap);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_pos);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_tgt);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_trk);

	return TRUE;
}


/**
 * @brief initialise the Sky class
 */

static void sky_class_init(SkyClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the plot parameters
 */

static void sky_init(Sky *p)
{
	gpointer sig;


	struct coord_equatorial eq = {0.0, 0.0};


	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_SKY(p));

	sig = sig_get_instance();

	p->cfg = sky_get_instance_private(p);

	bzero(p->cfg, sizeof(struct _SkyConfig));

	p->cfg->tgt.el = -90.0; /* do not draw target position on init */

	sky_load_config(p);

	/* add sun/moon objects, their coordinates are updated automatically */
	sky_append_object(p, "Sun",  eq, SKY_SUN_SIZE);
	sky_append_object(p, "Moon", eq, SKY_MOON_SIZE);


	/* connect the relevant signals of the DrawingArea */
	g_signal_connect(G_OBJECT(&p->parent), "configure-event",
			 G_CALLBACK(sky_configure_event_cb), NULL);

	g_signal_connect(G_OBJECT(&p->parent), "draw",
			  G_CALLBACK(sky_draw_cb), NULL);

	g_signal_connect(G_OBJECT(&p->parent), "motion-notify-event",
			 G_CALLBACK(sky_motion_notify_event_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "button-press-event",
			  G_CALLBACK(sky_button_press_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "enter-notify-event",
			  G_CALLBACK(sky_pointer_crossing_cb), NULL);

	g_signal_connect (G_OBJECT(&p->parent), "leave-notify-event",
			  G_CALLBACK(sky_pointer_crossing_cb), NULL);

	g_signal_connect(G_OBJECT(&p->parent), "destroy",
			 G_CALLBACK(sky_destroy), (gpointer) p);

#if 0

	g_signal_connect (G_OBJECT(&p->parent), "button-press-event",
			  G_CALLBACK (sky_button_press_cb), NULL);
#endif

	p->cfg->id_cap = g_signal_connect(sig, "pr-capabilities",
			  G_CALLBACK(sky_handle_pr_capabilities),
			  (gpointer) p);

	p->cfg->id_pos = g_signal_connect(sig, "pr-getpos-azel",
			  G_CALLBACK(sky_handle_pr_getpos_azel),
			  (gpointer) p);

	p->cfg->id_tgt = g_signal_connect(sig, "pr-moveto-azel",
			  G_CALLBACK(sky_handle_pr_moveto_azel),
			  (gpointer) p);

	p->cfg->id_trk = g_signal_connect(sig_get_instance(), "tracking",
			  G_CALLBACK(sky_handle_tracking),
			  (gpointer) p);


	/* update coordinates every second */
	p->cfg->id_to = g_timeout_add_seconds(1, sky_update_coord_hor,
					      (gpointer) p);


	gtk_widget_set_events(GTK_WIDGET(&p->parent), GDK_EXPOSURE_MASK
			       | GDK_LEAVE_NOTIFY_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_POINTER_MOTION_MASK
			       | GDK_POINTER_MOTION_HINT_MASK
			       | GDK_ENTER_NOTIFY_MASK
			       | GDK_LEAVE_NOTIFY_MASK);
}


/**
 * @brief create a new SKY widget
 */

GtkWidget *sky_new(void)
{
	Sky *sky;

	sky = g_object_new(TYPE_SKY, NULL);

	return GTK_WIDGET(sky);
}
