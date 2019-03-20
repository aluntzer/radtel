/**
 * @file    widgets/obs_assist/obs_assist.c
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
#include <cmd.h>
#include <math.h>


G_DEFINE_TYPE_WITH_PRIVATE(ObsAssist, obs_assist, GTK_TYPE_BOX)




void obs_assist_clear_spec(ObsAssist *p)
{
	g_free(p->cfg->spec.x);
	g_free(p->cfg->spec.y);

	p->cfg->spec.x = NULL;
	p->cfg->spec.y = NULL;
	p->cfg->spec.n = 0;
}

/**
 * @brief handle spectral data
 *
 * XXX we currently run observations on .5s timeouts; with the SRT spectrometer,
 *     this is fine, but for other backends, we might want to do partial
 *     data accumulation in this function (or accumulation/averaging on the
 *     server), so we don't waste observation data.
 */

static void obs_assist_handle_pr_spec_data(gpointer instance,
					   const struct spec_data *s,
					   gpointer data)
{

	uint64_t i;
	uint64_t f;

	gdouble *frq;
	gdouble *amp;

	ObsAssist *p;

	p = OBS_ASSIST(data);

	obs_assist_clear_spec(p);

	frq = g_malloc(s->n * sizeof(gdouble));
	amp = g_malloc(s->n * sizeof(gdouble));

	for (i = 0, f = s->freq_min_hz; i < s->n; i++, f += s->freq_inc_hz) {
		frq[i] = (gdouble) f * 1e-6;		/* Hz to Mhz */
		amp[i] = (gdouble) s->spec[i] * 0.001;	/* mK to K */
	}


	p->cfg->spec.x = frq;
	p->cfg->spec.y = amp;
	p->cfg->spec.n = s->n;
}




/**
 * @brief handle position data
 */

static gboolean obs_assist_getpos_azel_cb(gpointer instance, struct getpos *pos,
					  gpointer data)
{
	struct coord_horizontal hor;
	struct coord_equatorial	equ;
	struct coord_galactic gal;

	ObsAssist *p;


	p = OBS_ASSIST(data);

	hor.az = (gdouble) pos->az_arcsec / 3600.0;
	hor.el = (gdouble) pos->el_arcsec / 3600.0;

	equ = horizontal_to_equatorial(hor, p->cfg->lat, p->cfg->lon, 0.0);
	gal = horizontal_to_galactic(hor, p->cfg->lat, p->cfg->lon);

	p->cfg->az = hor.az;
	p->cfg->el = hor.el;

	p->cfg->ra = equ.ra;
	p->cfg->de = equ.dec;

	p->cfg->glat = gal.lat;
	p->cfg->glon = gal.lon;

	return TRUE;
}


/**
 * @brief handle capabilities data
 */

static void obs_assist_handle_pr_capabilities(gpointer instance,
					      const struct capabilities *c,
					      gpointer data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	p->cfg->lat = (gdouble) c->lat_arcsec / 3600.0;
	p->cfg->lon = (gdouble) c->lon_arcsec / 3600.0;

	p->cfg->az_min = (gdouble) c->az_min_arcsec / 3600.0;
	p->cfg->az_max = (gdouble) c->az_max_arcsec / 3600.0;
	p->cfg->az_res = (gdouble) c->az_res_arcsec / 3600.0;

	p->cfg->el_min = (gdouble) c->el_min_arcsec / 3600.0;
	p->cfg->el_max = (gdouble) c->el_max_arcsec / 3600.0;
	p->cfg->el_res = (gdouble) c->el_res_arcsec / 3600.0;
}


/**
 * @brief handle spec-acq-enaable
 */

static void obs_assist_handle_spec_acq_enable(gpointer instance, gpointer data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	p->cfg->acq_enabled = TRUE;
}


/**
 * @brief handle spec-acq-disable
 */

static void obs_assist_handle_spec_acq_disable(gpointer instance, gpointer data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	p->cfg->acq_enabled = FALSE;
}


/**
 * @brief handle status-move
 */

static void obs_assist_handle_pr_status_move(gpointer instance,
					     const struct status *s,
					     gpointer data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	p->cfg->moving = s->busy;
}



/**
 * @brief create hardware axis limit exceeded warning label
 */

GtkWidget *obs_assist_limits_exceeded_warning(const gchar *direction,
						 const gchar *axis,
						 const double limit_deg)
{
	GtkWidget *w;

	gchar *lbl;


	w = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);
	lbl = g_strdup_printf(
	      "<span foreground=\"red\" size=\"large\">WARNING:</span> "
	      "Your configuration exceeds the %s limit of the %s "
	      "drive: <tt>%5.2f°</tt>", direction, axis, limit_deg);
	gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);

	gtk_widget_set_halign(w, GTK_ALIGN_START);

	return w;
}


void obs_assist_on_ignore_warning(GtkWidget *w, gpointer data)
{
	GtkWidget *cp;
	GtkAssistant *as;

	gint pn;


       	as = GTK_ASSISTANT(data);

	pn = gtk_assistant_get_current_page(as);
	cp = gtk_assistant_get_nth_page(as, pn);

	gtk_assistant_set_page_complete(as, cp, TRUE);
}



void obs_assist_close_cancel(GtkWidget *widget, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(data));
}


/**
 * @brief hide the observation procedure selectors from show_all() calls
 */

void obs_assist_hide_procedure_selectors(ObsAssist *p)
{
	GList *l;


	p->cfg->hidden = gtk_container_get_children(GTK_CONTAINER(p));

	for(l = p->cfg->hidden; l; l = l->next) {
		gtk_widget_set_no_show_all(GTK_WIDGET(l->data), TRUE);
		gtk_widget_hide(GTK_WIDGET(l->data));
	}
}


/**
 * @brief unhide the observation procedure selectors from show_all() calls
 */

void obs_assist_unhide_procedure_selectors(ObsAssist *p)
{
	GList *l;


	p->cfg->hidden = gtk_container_get_children(GTK_CONTAINER(p));

	for(l = p->cfg->hidden; l; l = l->next)
		gtk_widget_set_no_show_all(GTK_WIDGET(l->data), FALSE);

	g_list_free(p->cfg->hidden);
}


/**
 * @brief observation procedure abort handler
 */

void obs_assist_abort(GtkWidget *w, gpointer data)
{
	ObsAssist *p;


	p = OBS_ASSIST(data);

	p->cfg->abort = TRUE;

	gtk_widget_destroy(gtk_widget_get_parent(w));

	obs_assist_unhide_procedure_selectors(p);

	/* and show all the obs procedure selectors again */
	gtk_widget_show_all(GTK_WIDGET(p));

}


/**
 * @brief create beam switching selection
 */

GtkWidget *ob_assist_beam_switching_new(ObsAssist *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Beam Switching",
				 "Perform a beam switching observation");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start beam switching");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
#if 0
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);
#endif

	return GTK_WIDGET(grid);
}


GtkWidget *obs_assist_create_default(GtkWidget *w)
{
	GtkWidget *as;
	GtkWidget *top;

	gint width;
	gint height;


	top = gtk_widget_get_toplevel(w);
	if (!GTK_IS_WINDOW(GTK_WINDOW(top))) {
		g_warning("%s: toplevel widget is not a window", __func__);
		return NULL;
	}

	as = g_object_new(GTK_TYPE_ASSISTANT, "use-header-bar", TRUE, NULL);


	gtk_window_get_size(GTK_WINDOW(top), &width, &height);

	gtk_window_set_transient_for(GTK_WINDOW(as),
				     GTK_WINDOW(top));
	gtk_window_set_modal(GTK_WINDOW(as), TRUE);
	gtk_window_set_position(GTK_WINDOW(as), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_window_set_attached_to(GTK_WINDOW(as), top);

	/* make the default size 2/3 of the toplevel window */
	gtk_window_set_default_size(GTK_WINDOW(as),
				    (2 * width) / 3, (2 * height) / 3);

	return as;
}


static void gui_create_obs_assist_controls(ObsAssist *p)
{
	GtkWidget *w;

	w = obs_assist_cross_scan_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
	w = obs_assist_gal_plane_scan_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
	w = obs_assist_azel_scan_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
	w = ob_assist_beam_switching_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
}


/**
 * @brief destroy signal handler
 */

static gboolean obs_assist_destroy(GtkWidget *w, void *data)
{
	ObsAssist *p;


	p = OBS_ASSIST(w);

	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_pos);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_cap);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_aen);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_adi);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_spd);
	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_mov);

	return TRUE;
}


/**
 * @brief initialise the ObsAssist class
 */

static void obs_assist_class_init(ObsAssistClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the ObsAssist widget
 */

static void obs_assist_init(ObsAssist *p)
{
	gpointer sig;

	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_OBS_ASSIST(p));

	p->cfg = obs_assist_get_instance_private(p);

	p->cfg->spec.x = NULL;
	p->cfg->spec.y = NULL;
	p->cfg->spec.n = 0;
	p->cfg->moving = FALSE;
	p->cfg->abort  = TRUE;
	p->cfg->hidden = NULL;

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_obs_assist_controls(p);

	sig = sig_get_instance();

	p->cfg->id_cap = g_signal_connect(sig, "pr-capabilities",
				G_CALLBACK(obs_assist_handle_pr_capabilities),
				(gpointer) p);

	p->cfg->id_pos = g_signal_connect(sig, "pr-getpos-azel",
				(GCallback) obs_assist_getpos_azel_cb,
				(gpointer) p);

	p->cfg->id_aen = g_signal_connect(sig, "pr-spec-acq-enable",
				G_CALLBACK(obs_assist_handle_spec_acq_enable),
				(gpointer) p);

	p->cfg->id_adi = g_signal_connect(sig, "pr-spec-acq-disable",
				  G_CALLBACK(obs_assist_handle_spec_acq_disable),
				  (gpointer) p);

	p->cfg->id_spd = g_signal_connect(sig_get_instance(), "pr-spec-data",
				G_CALLBACK(obs_assist_handle_pr_spec_data),
				(gpointer) p);

	p->cfg->id_mov = g_signal_connect(sig_get_instance(), "pr-status-move",
				 G_CALLBACK(obs_assist_handle_pr_status_move),
				 (void *) p);


	g_signal_connect(p, "destroy", G_CALLBACK(obs_assist_destroy), NULL);
}


/**
 * @brief create a new ObsAssist widget
 */

GtkWidget *obs_assist_new(void)
{
	ObsAssist *obs_assist;


	obs_assist = g_object_new(TYPE_OBS_ASSIST, NULL);

	return GTK_WIDGET(obs_assist);
}
