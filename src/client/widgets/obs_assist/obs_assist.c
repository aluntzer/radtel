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
//#include <obs_assist_internal.h>

#include <desclabel.h>
#include <signals.h>
#include <default_grid.h>
#include <cmd.h>
#include <math.h>


G_DEFINE_TYPE_WITH_PRIVATE(ObsAssist, obs_assist, GTK_TYPE_BOX)


/**
 * @brief handle position data
 */

static gboolean obs_assist_getpos_azel_cb(gpointer instance, struct getpos *pos,
					  ObsAssist *p)
{
	p->cfg->az = (gdouble) pos->az_arcsec / 3600.0;
	p->cfg->el = (gdouble) pos->el_arcsec / 3600.0;


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

	p->cfg->lat = c->lat_arcsec / 3600.0;
	p->cfg->lon = c->lon_arcsec / 3600.0;
}



static void on_assistant_close_cancel(GtkWidget *widget, gpointer data)
{
	GtkWidget *assistant = (GtkWidget *) data;

	gtk_widget_destroy(assistant);
	assistant = NULL;
}


static void
on_entry_changed (GtkWidget *widget, gpointer data)
{
  GtkAssistant *assistant = GTK_ASSISTANT (data);
  GtkWidget *current_page;
  gint page_number;
  const gchar *text;

  page_number = gtk_assistant_get_current_page (assistant);
  current_page = gtk_assistant_get_nth_page (assistant, page_number);
  text = gtk_entry_get_text (GTK_ENTRY (widget));

  if (text && *text)
    gtk_assistant_set_page_complete (assistant, current_page, TRUE);
  else
    gtk_assistant_set_page_complete (assistant, current_page, FALSE);
}


static void obs_assist_cross_setup_cb(GtkWidget *widget, ObsAssist *p)
{
	GtkWidget *box;
	GtkWidget *entry;

	GtkWidget *w;
	GtkWidget *as;
	GtkGrid *grid;

	gchar *title;

	gint width, height;
	gchar *lbl;


	as = g_object_new(GTK_TYPE_ASSISTANT, "use-header-bar", TRUE, NULL);

	gtk_window_get_size(GTK_WINDOW(gtk_widget_get_toplevel(widget)), &width, &height);
	/* XXX assert toplevel == type_window */
	gtk_window_set_transient_for(GTK_WINDOW(as),
				     GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	gtk_window_set_modal(GTK_WINDOW(as), TRUE);
	gtk_window_set_position(GTK_WINDOW(as), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_window_set_attached_to(GTK_WINDOW(as),
				   gtk_widget_get_toplevel(widget));

	gtk_window_set_default_size(GTK_WINDOW(as),
				    (2 * width) / 3, (2 * height) / 3);

	g_signal_connect(G_OBJECT(as), "cancel",
			 G_CALLBACK(on_assistant_close_cancel), as);
	g_signal_connect(G_OBJECT(as), "close",
			 G_CALLBACK(on_assistant_close_cancel), as);




	/* info page */
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	w  = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(w), TRUE);

        lbl = g_strdup_printf(
		"This observation mode will perform a scan in the shape of a "
		"cross around the current on-sky position of the telescope.\n\n"
		"<b>Note:</b> The central position will be tracked at the "
		"sidereal rate. The resulting graphs will be in Azimuth and "
		"Elevation offsets from the centeral position."
		"Azimuth distance will be corrected for the cosine of the "
		"Elevation for actual angular distance.");
        gtk_label_set_markup(GTK_LABEL(w), lbl);
	g_free(lbl);

	gtk_box_pack_start(GTK_BOX(box), w, FALSE, FALSE, 0);
	gtk_widget_show_all(box);

	gtk_assistant_append_page(GTK_ASSISTANT(as), box);
	gtk_assistant_set_page_complete(GTK_ASSISTANT(as), box, TRUE);
	gtk_assistant_set_page_title(GTK_ASSISTANT(as), box, "Info");
	gtk_assistant_set_page_type(GTK_ASSISTANT(as), box,
				    GTK_ASSISTANT_PAGE_INTRO);



	/* settings page */
	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Step Size",
				 "Specify the the step size in degrees. This "
				 "setting will apply to both scan directions.");
	gtk_grid_attach(grid, w, 0, 0, 1, 1);

	entry = gtk_entry_new();
	gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_NUMBER);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);

	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_halign(entry, GTK_ALIGN_END);
	gtk_grid_attach(grid, entry, 1, 0, 1, 1);

	gtk_widget_show_all(GTK_WIDGET(grid));
	g_signal_connect(G_OBJECT(entry), "changed",
			  G_CALLBACK(on_entry_changed), as);

	gtk_assistant_append_page(GTK_ASSISTANT(as), GTK_WIDGET(grid));
	gtk_assistant_set_page_title(GTK_ASSISTANT(as), GTK_WIDGET(grid), "Setup");
	gtk_assistant_set_page_type(GTK_ASSISTANT(as), GTK_WIDGET(grid),
				    GTK_ASSISTANT_PAGE_CONTENT);
	gtk_widget_show(as);
}


/**
 * @brief create cross scan selection
 */

GtkWidget *ob_assist_cross_scan_new(ObsAssist *p)
{
	GtkGrid *grid;
	GtkWidget *w;


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


/**
 * @brief create galactic plane scan selection
 */

GtkWidget *ob_assist_gal_line_scan_new(ObsAssist *p)
{
	GtkGrid *grid;
	GtkWidget *w;


	grid = GTK_GRID(new_default_grid());

	w = gui_create_desclabel("Galactic Line Scan",
				 "Scan along galactic longitude.");

	gtk_grid_attach(GTK_GRID(grid), w, 0, 0, 1, 1);

	w = gtk_button_new_with_label("Start");
	gtk_widget_set_tooltip_text(w, "Start Galactic Line Scan");

	gtk_widget_set_hexpand(w, TRUE);
	gtk_widget_set_halign(w, GTK_ALIGN_END);
	gtk_grid_attach(grid, w, 1, 0, 1, 1);
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);


	return GTK_WIDGET(grid);
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
	g_signal_connect(G_OBJECT(w), "clicked",
			 G_CALLBACK(obs_assist_cross_setup_cb), p);


	return GTK_WIDGET(grid);
}


static void gui_create_obs_assist_controls(ObsAssist *p)
{
	GtkWidget *w;

	w = ob_assist_cross_scan_new(p);
	gtk_box_pack_start(GTK_BOX(p), w, FALSE, FALSE, 0);
	w = ob_assist_gal_line_scan_new(p);
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
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_OBS_ASSIST(p));

	p->cfg = obs_assist_get_instance_private(p);

	gtk_orientable_set_orientation(GTK_ORIENTABLE(p),
				       GTK_ORIENTATION_VERTICAL);

	gtk_box_set_spacing(GTK_BOX(p), 0);
	gui_create_obs_assist_controls(p);


	p->cfg->id_cap = g_signal_connect(sig_get_instance(), "pr-capabilities",
					  G_CALLBACK(obs_assist_handle_pr_capabilities),
					  (void *) p);

	p->cfg->id_pos = g_signal_connect(sig_get_instance(), "pr-getpos-azel",
					  (GCallback) obs_assist_getpos_azel_cb,
					  (gpointer) p);

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
