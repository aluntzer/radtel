/**
 * @file    widgets/video/video.c
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
 * @brief show a video stream
 *
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

#include <sswdnd.h>

#include <vid_cfg.h>
#include <cmd.h>
#include <signals.h>

G_DEFINE_TYPE_WITH_PRIVATE(Video, video, GTK_TYPE_DRAWING_AREA)



/**
 * @brief write text with center at x/y coordinate and a given rotation
 *
 * @param cr the cairo context to draw on
 * @param x the x coordinate
 * @param y the y coordinate
 * @param buf a pointer to the text buffer
 * @param rot a text rotation in radians
 */

static void video_write_text_centered(cairo_t *cr,
				      const double x, const double y,
				      const char *buf, const double rot)
{
	cairo_text_extents_t te;


	cairo_save(cr);

	cairo_text_extents(cr, buf, &te);

	/* translate origin to center of text location */
	cairo_translate(cr, x, y);

	cairo_rotate(cr, rot);

	/* translate origin so text will be centered */
	cairo_translate(cr, -te.width * 0.5 , te.height * 0.5);

	/* start new path at origin */
	cairo_move_to(cr, 0.0, 0.0);

	cairo_show_text(cr, buf);

	cairo_stroke(cr);

	cairo_restore(cr);
}


static void video_error_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
	GError *err;
	gchar *debug_info;

	Video *p;


	p = VIDEO(data);


	gst_message_parse_error(msg, &err, &debug_info);
	g_printerr("Error received from %s: %s\n", GST_OBJECT_NAME (msg->src),
						   err->message);

	g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");

	g_clear_error(&err);
	g_free(debug_info);

	gst_element_set_state(p->cfg->playbin, GST_STATE_READY);
}


static void video_eos_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
	Video *p;


	p = VIDEO(data);

	gst_element_set_state(p->cfg->playbin, GST_STATE_READY);
}



static void video_state_changed_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
	GstState old, new, pending;

	Video *p;


	p = VIDEO(data);


	gst_message_parse_state_changed(msg, &old, &new, &pending);

	if (GST_MESSAGE_SRC(msg) == GST_OBJECT(p->cfg->playbin))
		p->cfg->state = new;
}


/**
 * @brief handle target position data
 */

static void video_handle_pr_video_uri(gpointer instance,
				      const gchar *uri,
				      gpointer data)
{
	Video *p;


	p = VIDEO(data);


	gst_element_set_state(p->cfg->playbin, GST_STATE_READY);

	if (p->cfg->uri)
		g_free(p->cfg->uri);

	p->cfg->uri = g_strdup(uri);

	if (p->cfg->uri) {
		g_object_set(p->cfg->playbin, "uri", p->cfg->uri, NULL);

		if (gtk_widget_is_visible(GTK_WIDGET(&p->parent)))
		    gst_element_set_state(p->cfg->playbin, GST_STATE_READY);
	}
}



/**
 * @brief clear drawing area when the stream is stopped or not available
 */

static gboolean video_draw_cb(GtkWidget *w, cairo_t *cr, gpointer data)
{
	GtkAllocation allocation;

	Video *p;


	p = VIDEO(data);

	if (p->cfg->state >= GST_STATE_PAUSED)
		return FALSE;



	gtk_widget_get_allocation(GTK_WIDGET(p), &allocation);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);
	cairo_fill(cr);

	cairo_set_source_rgb (cr, 1, 1, 1);
	video_write_text_centered(cr,
				  0.5 * allocation.width,
				  0.5 * allocation.height,
				   "NO STREAM AVAILABLE", 0.0);

	return FALSE;
}


static void video_realize_cb(GtkWidget *w, gpointer data)
{
	Video *p;
	GdkWindow *win;
	guintptr handle;


	p = VIDEO(data);

       	win = gtk_widget_get_window(w);

	printf("mmmmeeeeeh!\n");
	if (!gdk_window_ensure_native(win)) {
		g_error("VIDEO: not a native window!");
		return;
	}

#if defined (GDK_WINDOWING_WIN32)
	handle = (guintptr) GDK_WINDOW_HWND(win);
#elif defined (GDK_WINDOWING_QUARTZ)
	handle = gdk_quartz_window_get_nsview(win);
#elif defined (GDK_WINDOWING_X11)
	handle = GDK_WINDOW_XID(win);
#endif


	gst_element_set_state(p->cfg->playbin, GST_STATE_READY);

	gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(p->cfg->playbin), handle);
}


/**
 * @brief destroy signal handler
 */

static gboolean video_destroy(GtkWidget *w, gpointer data)
{
	Video *p;

	p = VIDEO(data);


	g_signal_handler_disconnect(sig_get_instance(), p->cfg->id_uri);

	printf("stop!\n");

	 if (GST_STATE(p->cfg->playbin) == GST_STATE_PLAYING) {
		gst_element_set_state(p->cfg->playbin, GST_STATE_READY);
	}


	return TRUE;
}

/**
 * @brief check if video surface is actually exposed
 */

static gboolean video_visible(gpointer data)
{
	GtkWidget *ch;
	GtkWidget *sw;
	Video *p;

	p = VIDEO(data);


	if (!gtk_widget_get_realized(GTK_WIDGET(&p->parent)))
		return G_SOURCE_CONTINUE;

	if (!p->cfg->playbin)
		return G_SOURCE_CONTINUE;

	if (!p->cfg->uri)
		return G_SOURCE_CONTINUE;

	/* apparently there is no reliable way to check whether a
	 * widget is ACTUALLY visible in a GTK context (I'm not talking about
	 * compositing WMs), but thankfully the drag and drop stack switcher
	 * can help us out here. The parent we're interested is like the
	 * universe: turtles - all the way down. We grab it and see if it is
	 * actually part of our SSWDnD which is a stack switcher and hence
	 * contains a GTK stack which we can check against the currently
	 * visible child (weird, but they actually have that status...)
	 * If we're not visible, we pause the video stream and resume otherwise
	 * to save some $$$ (aka bandwidth).
	 */

	ch = gtk_widget_get_parent(gtk_widget_get_parent(GTK_WIDGET(&p->parent)));
	sw = gtk_widget_get_parent(ch);

	if (!GTK_IS_STACK(sw))
	    return G_SOURCE_CONTINUE;

	if (gtk_stack_get_visible_child(GTK_STACK(sw)) == ch) {



		if (GST_STATE(p->cfg->playbin) != GST_STATE_PLAYING) {
		printf("play!\n");
			gst_element_set_state(p->cfg->playbin, GST_STATE_PLAYING);
			/* Start playing */
			if (gst_element_set_state(p->cfg->playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
				g_printerr ("Unable to set the pipeline to the playing state.\n");
				gst_object_unref(p->cfg->playbin);
			}
		}


	} else if (GST_STATE(p->cfg->playbin) == GST_STATE_PLAYING) {
		printf("STOP!\n");
		gst_element_set_state(p->cfg->playbin, GST_STATE_READY);
	}

	return G_SOURCE_CONTINUE;
}

/**
 * @brief initialise the Video class
 */

static void video_class_init(VideoClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the plot parameters
 */

static void video_init(Video *p)
{
	GstBus *bus;
	gpointer sig;


	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_VIDEO(p));

	sig = sig_get_instance();

	p->cfg = video_get_instance_private(p);

	memset(p->cfg, 0, sizeof(struct _VideoConfig));


	/* connect the relevant signals of the DrawingArea */
	g_signal_connect(G_OBJECT(&p->parent), "destroy",
			 G_CALLBACK(video_destroy), (gpointer) p);

	g_signal_connect(G_OBJECT(&p->parent), "realize",
			 G_CALLBACK(video_realize_cb), (gpointer) p);

	g_signal_connect(G_OBJECT(&p->parent), "draw",
			 G_CALLBACK(video_draw_cb), (gpointer) p);


#if 0
	p->cfg->id_uri = g_signal_connect(sig, "pr-video-uri",
			  G_CALLBACK(video_handle_pr_video_uri),
			  (gpointer) p);
#else
	p->cfg->uri = g_strdup("rtsp://radio:Telescope@radvis.astro.univie.ac.at:553/Streaming/channels/103/profile?token=media_profile1&SessionTimeout=600000");
#endif

	p->cfg->playbin = gst_element_factory_make("playbin", "playbin");
	if (!p->cfg->playbin) {
		g_message ("VIDEO: Error: could not create playbin.\n");
		return;
	}
	g_object_set(p->cfg->playbin, "uri", p->cfg->uri, NULL);

	bus = gst_element_get_bus (p->cfg->playbin);
	gst_bus_add_signal_watch (bus);
	g_signal_connect(G_OBJECT(bus), "message::error",
			 G_CALLBACK(video_error_cb), (gpointer) p);
	g_signal_connect(G_OBJECT(bus), "message::eos",
			 G_CALLBACK(video_eos_cb), (gpointer) p);
	g_signal_connect(G_OBJECT(bus), "message::state-changed",
			 G_CALLBACK(video_state_changed_cb), (gpointer) p);
	gst_object_unref(bus);



	g_timeout_add_seconds (1, (GSourceFunc) video_visible, (gpointer) p);
}


/**
 * @brief create a new VIDEO widget
 */

GtkWidget *video_new(void)
{
	Video *video;

	video = g_object_new(TYPE_VIDEO, NULL);

	return GTK_WIDGET(video);
}
