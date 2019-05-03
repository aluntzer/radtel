/**
 * @file    widgets/sswdnd/sswdnd.c
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
 * @brief a widget enhancing the GTK stack switcher with DnD dock-redock
 *
 * @note this is still a bit problematic, but works (all known errors caught)
 */


#include <sswdnd.h>
#include <sswdnd_cfg.h>
#include <signals.h>

#if (G_ENCODE_VERSION(2,58)) > GLIB_VERSION_CUR_STABLE
#include <fuck_you_ubuntu.h>
#endif

G_DEFINE_TYPE(SSWDnD, sswdnd, GTK_TYPE_STACK_SWITCHER)

static gpointer *sswdnd_sig;
static grefcount sswdnd_ref;



static void sswdnd_enable_dnd_on_last(GtkWidget *p);
static gboolean sswdnd_drag_failed(GtkWidget *w, GdkDragContext *c,
				   GtkDragResult res, gpointer data);
static void sswdnd_catch(gpointer instance, GtkWidget *ostack, gpointer data);



static gpointer *sswdnd_sig_get_instance(void)
{
	return sswdnd_sig;
}

static void sswdnd_disperse_widgets(GtkWidget *w, gpointer data)
{
	GtkWidget *stack;

	GList *l, *elem;


	stack = GTK_WIDGET(gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER(data)));
	l = gtk_container_get_children(GTK_CONTAINER(stack));

	for(elem = l; elem; elem = elem->next)
		sswdnd_drag_failed(NULL, NULL, 0, elem->data);

	g_list_free(l);
}


static void sswdnd_collect_widgets(GtkWidget *w, gpointer data)
{
	g_signal_emit_by_name(sswdnd_sig_get_instance(), "collect", data);

}

static void sswdnd_collect(gpointer instance, GtkWidget *stacksw, gpointer data)
{
	GList *l = NULL;


	if (GTK_IS_STACK(data))
		l = gtk_container_get_children(GTK_CONTAINER(data));

	if (l) {
		g_list_free(l);

		if (data == stacksw)
			return;

		sswdnd_catch(instance, data, stacksw);
		return;
	}

	/*  stack is empty, not a stack or stale memory in still registered
	 *  callback, drop
	 */
	g_signal_handlers_disconnect_matched(instance,
					     G_SIGNAL_MATCH_DATA,
					     0, 0, NULL,
					     sswdnd_collect, data);
#if 0
	g_object_unref(data);
#endif
}


/**
 * @brief create a new stack and add it to the stack switcher
 */

static GtkWidget *sswdnd_new_stack(SSWDnD *p)
{
	GtkWidget *w;


	w = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(w),
				      GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

	gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(p),
				     GTK_STACK(w));

	return w;
}


/**
 * @brief drag-begin signal handler rendering a nice image of the item we are
 *	  dragging
 */

static void sswdnd_drag_begin(GtkWidget *w, GdkDragContext *ctx, gpointer data)

{
	gint x;
	gint y;

	GtkWidget *aw;
	GtkAllocation a;
	GtkStyleContext *s;

	cairo_t *cr;
	cairo_surface_t *cs;


	/* we actually want the ancestor of the widget */
	aw = gtk_widget_get_ancestor(w, GTK_TYPE_BUTTON);

	gtk_widget_get_allocation(aw, &a);
	cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, a.width, a.height);
	cr = cairo_create(cs);
	s  = gtk_widget_get_style_context(aw);

	gtk_style_context_add_class(s, "drag-icon");
	gtk_widget_draw(aw, cr);
	gtk_style_context_remove_class(s, "drag-icon");

	gtk_widget_translate_coordinates(w, aw, 0, 0, &x, &y);
	cairo_surface_set_device_offset(cs, -x, -y);
	gtk_drag_set_icon_surface(ctx, cs);

	cairo_destroy (cr);
	cairo_surface_destroy(cs);
}


/**
 * @brief drag-data-get signal handler (emitted by receiver of drag)
 */

static void sswdnd_drag_data_get(GtkWidget *w, GdkDragContext *ctx,
				 GtkSelectionData *sel_data,
				 guint info, guint time, gpointer data)
{
	gtk_selection_data_set(sel_data,
			       gdk_atom_intern_static_string("SSWDND_WIDGET"),
			       32,
			       (const guchar *) &data,
			       sizeof(gpointer));
}


/**
 * @brief drag-data-received handler , signalled when drag-data-get is complete
 */

static void sswdnd_drag_data_received(GtkWidget *w, GdkDragContext *ctx,
				      gint x, gint y,
				      GtkSelectionData *sel_data,
				      guint info, guint time, gpointer data)
{
	gint pick_pos;
	gint drop_pos;

	GtkWidget *ostack;
	GtkWidget *nstack;

	GtkWidget *button;
	GtkWidget *drop_cont;

	gchar *lbl = NULL;


	button = GTK_WIDGET((* ((gpointer *) gtk_selection_data_get_data(sel_data))));
	ostack = gtk_widget_get_parent(GTK_WIDGET(button));
	nstack = GTK_WIDGET(data);

	drop_cont = gtk_widget_get_parent(GTK_WIDGET(w));


	gtk_container_child_get(GTK_CONTAINER(ostack), GTK_WIDGET(button),
				"title", &lbl, NULL);

	/* get pickup position in case of same-stack DnD */
	gtk_container_child_get(GTK_CONTAINER(ostack), GTK_WIDGET(button),
				"position", &pick_pos, NULL);

	/* add reference, so gtk does not deallocate when detached */
	g_object_ref(button);
	gtk_container_remove(GTK_CONTAINER(ostack), button);

	gtk_container_child_get(GTK_CONTAINER(drop_cont), GTK_WIDGET(w),
				"position", &drop_pos, NULL);
	gtk_stack_add_named(GTK_STACK(nstack), GTK_WIDGET(button), lbl);

	gtk_container_child_set(GTK_CONTAINER(nstack), GTK_WIDGET(button),
				"title", lbl, NULL);

	g_object_unref(button);

	/* re-enable dnd */
	sswdnd_enable_dnd_on_last(drop_cont);

	/* enforce somewhat consistent behaviour */
	if (ostack == nstack) {
		if (pick_pos <= drop_pos)
			drop_pos++;
	}

	/* interestingly, setting the position will move the other
	 * children out of the way, so we'll do that instead of manually
	 * moving them around
	 */
	gtk_container_child_set(GTK_CONTAINER(nstack), GTK_WIDGET(button),
				"position", drop_pos, NULL);

	if (!gtk_container_get_children(GTK_CONTAINER(ostack)))
		gtk_widget_destroy(gtk_widget_get_toplevel(ostack));
}


/**
 * @brief drag-failed handler; we treat these as new-window drops
 */

static gboolean sswdnd_drag_failed(GtkWidget *w, GdkDragContext *c,
				   GtkDragResult res, gpointer data)
{
	gchar *lbl = NULL;

	GtkWidget *win = NULL;
	GtkWidget *parent;
	GtkWidget *sswdnd;


	parent = gtk_widget_get_parent(GTK_WIDGET(data));

	gtk_container_child_get(GTK_CONTAINER(parent), GTK_WIDGET(data),
				"title", &lbl, NULL);

	/* make sure we keep this around */
	g_object_ref(GTK_WIDGET(data));

	/* remove the widget */
	gtk_container_remove(GTK_CONTAINER(parent), GTK_WIDGET(data));

	/* create another one of us and attach */
	sswdnd = sswdnd_new();
	sswdnd_add_named(sswdnd, GTK_WIDGET(data), lbl);

	g_object_unref(GTK_WIDGET(data));


	g_signal_emit_by_name(parent, "sswdnd-create-window", &win, sswdnd);

	if (!win) {
		g_warning("Could not add new window, widget dangling!");
		return TRUE;
	}

	/* we created a new window and moved there */
	g_ref_count_inc(&sswdnd_ref);

	/* mop up if we spilled an empty window */
	if (!gtk_container_get_children(GTK_CONTAINER(parent))) {

		GtkWidget *top = gtk_widget_get_toplevel(parent);

		if (GTK_IS_WINDOW(top))
			gtk_window_close(GTK_WINDOW(top));
	}

	return TRUE;
}


/**
 * @brief enable dnd on the last item added to a stack switcher
 */

static void sswdnd_enable_dnd_on_last(GtkWidget *p)
{
	GtkWidget *stack;

	GtkWidget *button;
	GtkWidget *child;

	GList *l;


	l = gtk_container_get_children(GTK_CONTAINER(p));
	button = GTK_WIDGET(g_list_last(l)->data);
	g_list_free(l);

	stack = GTK_WIDGET(gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER(p)));
	l = gtk_container_get_children(GTK_CONTAINER(stack));
	child = GTK_WIDGET(g_list_last(l)->data);
	g_list_free(l);


	gtk_drag_source_set(button, GDK_BUTTON1_MASK,
			    sswdnd_entries, 1, GDK_ACTION_MOVE);

	gtk_drag_dest_set(button, GTK_DEST_DEFAULT_ALL,
			  sswdnd_entries, 1, GDK_ACTION_MOVE);


	g_signal_connect(button, "drag-begin",
			 G_CALLBACK(sswdnd_drag_begin), NULL);

	g_signal_connect(button, "drag-failed",
			 G_CALLBACK(sswdnd_drag_failed), child);

	g_signal_connect(button, "drag-data-get",
			 G_CALLBACK(sswdnd_drag_data_get), child);

	g_signal_connect(button, "drag-data-received",
			 G_CALLBACK(sswdnd_drag_data_received), stack);
}


/**
 * @brief catches stacks which were dropped need rescue
 */

static void sswdnd_catch(gpointer instance, GtkWidget *ostack, gpointer data)
{

	GtkWidget *tgt;
	GtkWidget *btn;

	GtkWidget *nstack;

	GList *l, *elem;

	gchar *lbl = NULL;


	/* some other ssw might have given us a new home already */
	if (!GTK_IS_STACK_SWITCHER(data))
		return;


	if (!GTK_IS_STACK(ostack))  {
		g_warning("Argument is not a stack!");
		return;
	}


	nstack = GTK_WIDGET(gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER((data))));

	if (!GTK_IS_STACK(nstack)) {
		g_warning("Not a stack!");
		return;
	}

	if (ostack == nstack)
		return;


	/* locate one of the DnD target buttons */
	l = gtk_container_get_children(GTK_CONTAINER(nstack));
	if (!l) /* hmmm...happens on "cascade" ... ordering issue?*/
		return;

	tgt = GTK_WIDGET(g_list_last(l)->data);

	g_list_free(l);

	if (!tgt) {
		g_warning("No target in new stack!");
		return;
	}


	/* source */
	l = gtk_container_get_children(GTK_CONTAINER(ostack));

	for(elem = l; elem; elem = elem->next) {

		btn = GTK_WIDGET(elem->data);

		gtk_container_child_get(GTK_CONTAINER(ostack), GTK_WIDGET(btn),
					"title", &lbl, NULL);
		g_object_ref(btn);
		gtk_container_remove(GTK_CONTAINER(ostack), btn);
		gtk_stack_add_named(GTK_STACK(nstack), GTK_WIDGET(btn), lbl);
		gtk_container_child_set(GTK_CONTAINER(nstack), GTK_WIDGET(btn),
					"title", lbl, NULL);
		sswdnd_enable_dnd_on_last(GTK_WIDGET(data));
		g_object_unref(btn);
	}


	if (!gtk_container_get_children(GTK_CONTAINER(ostack))) {

		GtkWidget *top = gtk_widget_get_toplevel(ostack);

		if (GTK_IS_WINDOW(top))
			gtk_widget_destroy(top);
	}


	g_list_free(l);

	return;
}



static gboolean sswdnd_rescue_me(GtkWidget *w, gpointer data)
{
	/* we were disconnected from the parent window */
	g_ref_count_dec(&sswdnd_ref);

	g_signal_emit_by_name(sswdnd_sig_get_instance(), "rescue-me", w);


	if (g_ref_count_dec(&sswdnd_ref))
		sig_shutdown();
	else
		g_ref_count_inc(&sswdnd_ref);

	return TRUE;
}

/*
 * @brief add a new child to the stack switcher
 * @note for simplicity, the name will be used for the "title" property as well
 */

void sswdnd_add_named(GtkWidget *p, GtkWidget *w, const gchar *name)
{
	GtkWidget *stack;


	g_return_if_fail(IS_SSWDND(p));

	stack = GTK_WIDGET(gtk_stack_switcher_get_stack(GTK_STACK_SWITCHER(p)));

	if (!stack) {
		stack = sswdnd_new_stack(SSWDND(p));

		g_signal_connect(sswdnd_sig_get_instance(), "rescue-me",
				 G_CALLBACK(sswdnd_catch), p);

		g_signal_connect(stack, "destroy",
				 G_CALLBACK(sswdnd_rescue_me), p);

		g_signal_connect(sswdnd_sig_get_instance(), "collect",
				 G_CALLBACK(sswdnd_collect), stack);
	}

	gtk_stack_add_named(GTK_STACK(stack), w, name);
	gtk_container_child_set(GTK_CONTAINER(stack), w, "title", name, NULL);

	sswdnd_enable_dnd_on_last(p);
}


/**
 * @brief add cascade and collect buttons to headerbar
 */

void sswdnd_add_header_buttons(GtkWidget *sswdnd, GtkWidget *header)
{
	GtkWidget *btn;


	btn = gtk_button_new_from_icon_name("view-grid-symbolic",
					    GTK_ICON_SIZE_BUTTON);

	g_signal_connect(G_OBJECT(btn), "clicked",
			 G_CALLBACK(sswdnd_disperse_widgets),
			 GTK_CONTAINER(sswdnd));

	gtk_widget_set_tooltip_text(btn, "Disperse");
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header), btn);

	btn = gtk_button_new_from_icon_name("view-restore-symbolic",
					    GTK_ICON_SIZE_BUTTON);

	g_signal_connect(G_OBJECT(btn), "clicked",
			 G_CALLBACK(sswdnd_collect_widgets), GTK_CONTAINER(sswdnd));

	gtk_widget_set_tooltip_text(btn,"Collect");
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header), btn);
}


/**
 * @brief initialise the SSWDnD class
 */

static void sswdnd_class_init(SSWDnDClass *klass)
{
	__attribute__((unused))
	GtkWidgetClass *widget_class;


	widget_class = GTK_WIDGET_CLASS(klass);

	/* not exactly "proper"; maybe redo later when I revisit the widget */
	sswdnd_sig = g_object_new(G_TYPE_OBJECT, NULL);

	g_signal_new("rescue-me",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_LAST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);

	g_signal_new("collect",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_LAST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);

	g_signal_new("sswdnd-create-window",
		     GTK_TYPE_STACK, G_SIGNAL_RUN_LAST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	/* override widget methods go here if needed */
}


/**
 * @brief initialise the SSWDnD widget
 */

static void sswdnd_init(SSWDnD *p)
{
	g_return_if_fail(p != NULL);
	g_return_if_fail(IS_SSWDND(p));

	if (!sswdnd_ref) {
		g_ref_count_init(&sswdnd_ref);
		g_ref_count_inc(&sswdnd_ref);
	}
}


/**
 * @brief create a new SSWDnD widget
 */

GtkWidget *sswdnd_new(void)
{
	SSWDnD *sswdnd;

	sswdnd = g_object_new(TYPE_SSWDND, NULL);

	return GTK_WIDGET(sswdnd);
}
