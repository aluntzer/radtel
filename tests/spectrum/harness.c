/**
 * @file    harness.c
 * @brief   driver that runs a byte program against the real Spectrum widget
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include <spectrum.h>
#include <spectrum_cfg.h>
#include <xyplot.h>
#include <signals.h>
#include <cmd.h>
#include <protocol.h>
#include <pkt_proc.h>

#include "harness.h"


/* the widget under test */
static GtkWidget *win;
static GtkWidget *spec;
static GtkWidget *plot;

/* the two spin buttons, located in the sidebar grid */
static GtkWidget *sb_per;
static GtkWidget *sb_avg;

/* live-point accounting */
static gsize pts_live;
static gsize pts_last;

/* how often the widget has asked for a fit since the last reset */
static guint fit_count;

gsize harness_points_live(void)
{
	return pts_live;
}


guint harness_fit_count(void)
{
	return fit_count;
}


gboolean harness_axes_finite(void)
{
	XYPlot *p = XYPLOT(plot);

	if (!isfinite(p->xmin) || !isfinite(p->xmax) ||
	    !isfinite(p->ymin) || !isfinite(p->ymax))
		return FALSE;

	if (!isfinite(p->x_ax.len) || !isfinite(p->y_ax.len))
		return FALSE;

	if (!(p->scale_x > 0.0) || !(p->scale_y > 0.0))
		return FALSE;

	if (!isfinite(p->scale_x) || !isfinite(p->scale_y))
		return FALSE;

	if (!p->sel.active)
		return TRUE;

	if (!isfinite(p->sel.xmin) || !isfinite(p->sel.xmax) ||
	    !isfinite(p->sel.ymin) || !isfinite(p->sel.ymax))
		return FALSE;

	return TRUE;
}


guint harness_graph_count(void)
{
	XYPlot *p = XYPLOT(plot);

	return g_list_length(p->graphs);
}


guint harness_graph_pending(void)
{
	XYPlot *p = XYPLOT(plot);

	return g_list_length(p->graphs_cleanup);
}


static void count_fit(GtkWidget *w, gpointer data)
{
	(void) w;
	(void) data;

	fit_count++;
}


/* ------------------------------------------------------------------ */
/* byte reader								*/
/* ------------------------------------------------------------------ */

struct rd {
	const guint8 *p;
	size_t n;
	size_t i;
};


static guint8 rd_u8(struct rd *r)
{
	if (r->i + 1 > r->n)
		return 0;
	return r->p[r->i++];
}


static guint16 rd_u16(struct rd *r)
{
	guint16 v;

	v  = (guint16) rd_u8(r);
	v |= (guint16) rd_u8(r) << 8;

	return v;
}


static gint16 rd_i16(struct rd *r)
{
	return (gint16) rd_u16(r);
}


static gdouble rd_unit(struct rd *r)
{
	return (gdouble) rd_u8(r) / 255.0;
}


/* ------------------------------------------------------------------ */
/* event synthesis							*/
/* ------------------------------------------------------------------ */

static GdkDevice *pointer_device(void)
{
	GdkDisplay *dpy;

	dpy = gtk_widget_get_display(plot);
	if (!dpy)
		return NULL;

	return gdk_seat_get_pointer(gdk_display_get_default_seat(dpy));
}


static void send_button(guint type, guint button, guint state,
			gdouble x, gdouble y)
{
	GdkEvent *ev;

	ev = gdk_event_new((GdkEventType) type);
	ev->button.device = pointer_device();
	ev->button.window = g_object_ref(gtk_widget_get_window(plot));
	ev->button.send_event = TRUE;
	ev->button.time = GDK_CURRENT_TIME;
	ev->button.x = x;
	ev->button.y = y;
	ev->button.x_root = x;
	ev->button.y_root = y;
	ev->button.state = state;
	ev->button.button = button;

	gtk_main_do_event(ev);

	gdk_event_free(ev);
}


static void send_motion(guint state, gdouble x, gdouble y)
{
	GdkEvent *ev;

	ev = gdk_event_new(GDK_MOTION_NOTIFY);
	ev->motion.device = pointer_device();
	ev->motion.window = g_object_ref(gtk_widget_get_window(plot));
	ev->motion.send_event = TRUE;
	ev->motion.time = GDK_CURRENT_TIME;
	ev->motion.x = x;
	ev->motion.y = y;
	ev->motion.x_root = x;
	ev->motion.y_root = y;
	ev->motion.state = state;
	ev->motion.is_hint = TRUE;

	gtk_main_do_event(ev);

	gdk_event_free(ev);
}


/* the gesture the users describe: press, ctrl+drag, ctrl+release */
static void drag(gdouble x0, gdouble y0, gdouble x1, gdouble y1, gboolean ctrl)
{
	guint msk;

	send_button(GDK_BUTTON_PRESS, 1, 0, x0, y0);

	msk = GDK_BUTTON1_MASK;
	if (ctrl)
		msk |= GDK_CONTROL_MASK;

	send_motion(msk, x1, y1);
	send_motion(msk, x1, y1);

	send_button(GDK_BUTTON_RELEASE, 1, msk, x1, y1);
}


static void scroll(gint direction, gdouble x, gdouble y)
{
	GdkEvent *ev;

	ev = gdk_event_new(GDK_SCROLL);
	ev->scroll.device = pointer_device();
	ev->scroll.window = g_object_ref(gtk_widget_get_window(plot));
	ev->scroll.send_event = TRUE;
	ev->scroll.time = GDK_CURRENT_TIME;
	ev->scroll.x = x;
	ev->scroll.y = y;
	ev->scroll.state = 0;
	ev->scroll.direction = direction;
	ev->scroll.delta_x = 0;
	ev->scroll.delta_y = 0;

	gtk_main_do_event(ev);

	gdk_event_free(ev);
}


/* the key handler maps 'a' to autorange and 'u' to clearing the selection */
static void send_key(guint keyval)
{
	GdkEvent *ev;

	ev = gdk_event_new(GDK_KEY_PRESS);
	ev->key.window = g_object_ref(gtk_widget_get_window(plot));
	ev->key.send_event = TRUE;
	ev->key.time = GDK_CURRENT_TIME;
	ev->key.keyval = keyval;
	ev->key.length = 0;
	ev->key.string = NULL;
	ev->key.hardware_keycode = 0;
	ev->key.state = 0;

	gtk_main_do_event(ev);

	gdk_event_free(ev);
}


/* ------------------------------------------------------------------ */
/* data synthesis							*/
/* ------------------------------------------------------------------ */

/* amplitude shapes; the wire format is uint32 mK, so all of these stay
 * non-negative -- as in the real client
 */
static uint32_t synth_amp(guint8 pattern, guint8 k, gdouble amp,
			  gdouble w0, gdouble dx, guint seed)
{
	gdouble t, v;

	seed = seed * 1103515245u + 12345u;

	switch (pattern) {
	case 0:		/* flat noise */
		t = ((gdouble) ((seed >> 16) & 0xffff) / 65535.0) * amp;
		break;
	case 1:		/* gaussian on noise */
		t = amp * exp(-0.5 * pow(((gdouble) k * dx - w0) / (0.2 * w0
							 + 1e-9), 2.0));
		t += ((gdouble) ((seed >> 16) & 0xff) / 255.0) * amp * 0.05;
		break;
	case 2:		/* exact zeros: fully masked bins */
		t = 0.0;
		break;
	case 3:		/* gaussian riding on zero: peak in a zeroed band */
		t = amp * exp(-0.5 * pow(((gdouble) k * dx - w0) / (0.2 * w0
							 + 1e-9), 2.0));
		break;
	case 4:		/* single spike */
		t = (k == 1) ? amp : 0.0;
		break;
	case 5:		/* monotonic ramp */
		t = amp * (gdouble) k / 64.0;
		break;
	case 6:		/* one distinct value everywhere but one bin */
		t = (k == ((guint) seed % 64) + 1) ? amp : 0.0;
		break;
	case 7:		/* huge */
		t = amp * 1e9;
		break;
	case 8:		/* denormal-ish */
		t = amp * 1e-300;
		break;
	default:	/* bit-pattern noise, spans the whole uint32 range */
		t = (gdouble) (seed & 0xffff);
		break;
	}

	if (t < 0.0)
		t = 0.0;

	if (t > 4.2e9)
		t = 4.2e9;

	return (uint32_t) t;
}


static void op_push(struct rd *r, gboolean short_packet)
{
	guint8 ncode, pattern, acode, fcode, icode, salt;
	struct spec_data *s;
	struct packet *pkt;
	gsize n, alloc, i, hdr, payload;
	gdouble amp, w0, dx;
	uint64_t fmin, finc;
	gboolean refuse;

	ncode   = rd_u8(r);
	pattern = rd_u8(r);
	acode   = rd_u8(r);
	fcode   = rd_u8(r);
	icode   = rd_u8(r);
	salt    = rd_u8(r);

	n = 1 + (gsize) ((ncode * 7919u) % 1500);

	/* keep the live point count inside the budget */
	refuse = ((pts_live / (pts_last ? pts_last : 1) + 2) * (n + 200) >
		 HARNESS_MAX_POINTS);
	if (refuse)
		return;

	amp = 1.0 + (gdouble) acode * 40.0;

	fmin = 100000000ull + (uint64_t) fcode * 1000000ull;
	finc = 1000000ull + (uint64_t) icode * 200000ull;

	dx  = (gdouble) finc * 1e-6;
	w0  = 0.3 + (gdouble) ((salt >> 4) & 0x0f) * 0.05;

	alloc = n;
	if (short_packet && (n > 2))
		alloc = n / 2;

	/* build the payload through a real packet, so the receive path
	 * validates it exactly as it would coming off the wire; the server
	 * sizes it as sizeof(struct spec_data) + 4 * count
	 */
	hdr     = sizeof(struct spec_data);
	payload = hdr + alloc * sizeof(uint32_t);

	pkt = g_malloc0(sizeof(struct packet) + payload);
	pkt->service   = PR_SPEC_DATA;
	pkt->trans_id  = 0;
	pkt->data_size = (uint32_t) payload;

	s = g_malloc(payload);

	s->freq_min_hz = fmin;
	s->freq_max_hz = fmin + finc * (uint64_t) n;
	s->freq_inc_hz = finc;
	s->n = (uint32_t) n;

	for (i = 0; i < alloc; i++)
		s->spec[i] = synth_amp(pattern, (guint) i, amp, w0, dx,
				       (guint) (i * 31 + salt));

	memcpy(pkt->data, s, payload);

	g_free(s);

	proc_pr_spec_data(pkt);

	g_free(pkt);

	pts_last = n;
	pts_live += n;
}


/* ------------------------------------------------------------------ */
/* setup								*/
/* ------------------------------------------------------------------ */

static void pump(void)
{
	gint i;

	for (i = 0; i < 400; i++) {
		g_main_context_iteration(NULL, FALSE);

		if (gtk_widget_get_allocated_width(plot) > 0 &&
		    gtk_widget_get_allocated_height(plot) > 0 &&
		    XYPLOT(plot)->render && XYPLOT(plot)->plot)
			return;
	}
}


/* run pending main context work so that the fit-selection request deferred
 * by xyplot_add_graph() is actually delivered
 */

static void settle(void)
{
	gint i;

	for (i = 0; i < 16; i++)
		g_main_context_iteration(NULL, FALSE);
}


/* GTK 3 has no device field on GdkEventKey, so every synthesised key press
 * draws this one warning; drop just that message and let every other Gdk
 * diagnostic through
 */
static void drop_nodev_warning(const gchar *log_domain, GLogLevelFlags level,
			       const gchar *message, gpointer ref)
{
	(void) log_domain;
	(void) level;
	(void) ref;

	if (!strstr(message, "not holding a GdkDevice"))
		g_log_default_handler("Gdk", level, message, NULL);
}


int harness_init(void)
{
	GtkWidget *side;

	if (!gtk_init_check(NULL, NULL))
		return -1;

	g_log_set_handler("Gdk", G_LOG_LEVEL_WARNING, drop_nodev_warning, NULL);

	sig_init();

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), 900, 600);

	spec = spectrum_new();
	gtk_container_add(GTK_CONTAINER(win), spec);

	gtk_widget_show_all(win);

	/* the plot is the first child of the spectrum box */
	plot = gtk_container_get_children(GTK_CONTAINER(spec)) ?
		((GList *) gtk_container_get_children(GTK_CONTAINER(spec)))->data :
		NULL;
	if (!plot)
		return -1;

	g_signal_connect(plot, "xyplot-fit-selection", G_CALLBACK(count_fit), NULL);

	pump();

	if (!XYPLOT(plot)->render || !XYPLOT(plot)->plot) {
		g_printerr("harness: plot surfaces not realised\n");
		return -1;
	}

	/* the sidebar grid holds the two spin buttons at fixed positions */
	side = ((GList *) gtk_container_get_children(GTK_CONTAINER(spec)))
		->next->data;
	sb_per = gtk_grid_get_child_at(GTK_GRID(side), 0, 3);
	sb_avg = gtk_grid_get_child_at(GTK_GRID(side), 0, 8);

	/* a position update, as the server would send it */
	{
		struct getpos pos = { .az_arcsec = 0, .el_arcsec = 3600000 };
		g_signal_emit_by_name(sig_get_instance(), "pr-getpos-azel",
				     &pos, NULL);
	}

	return 0;
}


/* ------------------------------------------------------------------ */
/* direct probes							*/
/* ------------------------------------------------------------------ */

void harness_reset(void)
{
	XYPlot *p = XYPLOT(plot);

	/* the graph context menu is popped up with a pointer grab. left up,
	 * it swallows every later gesture and keeps every later graph drop
	 * deferred into graphs_cleanup, so the next scenario would run
	 * against a wedged widget
	 */
	if (GTK_IS_WIDGET(p->menu)) {
		gtk_menu_popdown(GTK_MENU(p->menu));
		gtk_widget_hide(p->menu);
	}

	/* clear the fit through the widget's own path before anything is
	 * freed: with an empty selection the fit handler drops both curves
	 * and forgets the references. dropping them here instead would leave
	 * the spectrum holding references to graphs this reset is about to
	 * free, and the next scenario would then drop freed memory
	 */
	p->sel.active = FALSE;
	p->sel.xmin = 0.0;
	p->sel.xmax = 0.0;
	p->sel.ymin = 0.0;
	p->sel.ymax = 0.0;

	g_signal_emit_by_name(plot, "xyplot-fit-selection");

	settle();

	/* drive the widget's own reset paths first, so its graph references
	 * stay consistent; a bare xyplot_drop_all_graphs() would leave the
	 * spectrum side holding stale references, which is a bug in itself
	 * and would mask everything else (see HARNESS_OP_DROP_ALL)
	 */
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb_per), 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb_avg), 0);

	xyplot_drop_all_graphs(plot);

	gtk_widget_grab_focus(plot);
	send_key(GDK_KEY_a);

	/* a rubber band maps its pixels through the scale and plot origin
	 * the last render left behind, so a scenario that ran before this
	 * one would otherwise hand the next one a degenerate window and the
	 * gesture guard would reject a perfectly good box
	 */
	p->xmin = 0.0;
	p->xmax = 1.0;
	p->xlen = 1.0;
	p->ymin = 0.0;
	p->ymax = 1.0;
	p->ylen = 1.0;
	p->cmin = 0.0;
	p->cmax = 1.0;
	p->clen = 1.0;

	xyplot_redraw(plot);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb_per), 10);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb_avg), 10);

	pts_live = 0;
	pts_last = 0;
	fit_count = 0;
}


void harness_add_nan_graph(size_t n, size_t nan_from, size_t nan_to)
{
	gdouble *x, *y;
	void *ref;
	size_t i;

	x = g_malloc(n * sizeof(gdouble));
	y = g_malloc(n * sizeof(gdouble));

	for (i = 0; i < n; i++) {
		x[i] = 1420.0 + (gdouble) i * 0.5;
		y[i] = 1.0 + 0.5 * sin((gdouble) i * 0.1);
	}

	for (i = nan_from; (i < nan_to) && (i < n); i++) {
		x[i] = NAN;
		y[i] = NAN;
	}

	ref = xyplot_add_graph(plot, x, y, NULL, n, g_strdup_printf("PROBE"));
	xyplot_set_graph_style(plot, ref, NAN_LINES);
}


/* ------------------------------------------------------------------ */
/* the interpreter							*/
/* ------------------------------------------------------------------ */

static void op_colour(struct rd *r)
{
	GdkRGBA c;

	c.red   = rd_unit(r);
	c.green = rd_unit(r);
	c.blue  = rd_unit(r);
	c.alpha = rd_unit(r);

	xyplot_set_graph_rgba(plot, SPECTRUM(spec)->cfg->r_avg, c);
	xyplot_redraw(plot);
}


/* xyplot_add_graph() must not deliver the fit-selection request from inside
 * the call: the re-entrant fit handler refreshes the plot, and the graph
 * just added still carries the xyplot_add_graph defaults at that point, so
 * it would be painted as a yellow histeps curve that no later refresh
 * removes. the request must still be delivered, only later.
 */

gboolean harness_fit_deferred(void)
{
	guint8 buf[6];
	guint before, during, after;
	struct rd r;

	memset(buf, 0, sizeof(buf));

	r.p = buf;
	r.n = sizeof(buf);
	r.i = 0;

	before = fit_count;

	op_push(&r, FALSE);

	during = fit_count;

	settle();

	after = fit_count;

	/* must not have been delivered from inside the call ... */
	if (during != before)
		return FALSE;

	/* ... but must still arrive once the main context runs */
	return after > during;
}


void harness_run(const guint8 *data, size_t size)
{
	struct rd r = { data, size, 0 };
	XYPlot *p;
	guint op;
	guint guard = 0;

	/* start from a clean plot but keep the widget alive */
	harness_reset();
	p = XYPLOT(plot);
	p->ind_x.lbl = NULL;
	p->ind_y.lbl = NULL;

	while ((r.i < r.n) && (guard++ < 512)) {
		op = rd_u8(&r);

		switch (op) {
		case HARNESS_OP_NOP:
			break;
		case HARNESS_OP_PUSH:
			op_push(&r, FALSE);
			break;
		case HARNESS_OP_PUSH_VOID: {
			struct spec_data *s;
			struct packet *pkt;

			gsize hdr, payload;

			hdr     = sizeof(struct spec_data);
			payload = hdr;

			pkt = g_malloc0(sizeof(struct packet) + payload);
			pkt->service   = PR_SPEC_DATA;
			pkt->data_size = (uint32_t) payload;

			s = g_malloc0(payload);
			s->n = 0;

			memcpy(pkt->data, s, payload);

			g_free(s);

			proc_pr_spec_data(pkt);

			g_free(pkt);
			break;
		}
		case HARNESS_OP_PUSH_SHORT:
			op_push(&r, TRUE);
			break;
		case HARNESS_OP_PER:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb_per),
						  rd_u16(&r) % 1001);
			break;
		case HARNESS_OP_AVG:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(sb_avg),
						  rd_u16(&r) % 1001);
			break;
		case HARNESS_OP_SEL: {
			gdouble x0, y0, x1, y1;

			x0 = rd_i16(&r);
			y0 = rd_i16(&r);
			x1 = rd_i16(&r);
			y1 = rd_i16(&r);

			drag(x0, y0, x1, y1, TRUE);
			break;
		}
		case HARNESS_OP_FIT:
			g_signal_emit_by_name(plot, "xyplot-fit-selection");
			break;
		case HARNESS_OP_REDRAW:
			xyplot_redraw(plot);
			break;
		case HARNESS_OP_STYLE: {
			GList *elem;
			enum xyplot_graph_style s;

			switch (rd_u8(&r) % 9) {
			case 0: s = STAIRS; break;
			case 1: s = CIRCLES; break;
			case 2: s = LINES; break;
			case 3: s = NAN_LINES; break;
			case 4: s = CURVES; break;
			case 5: s = DASHES; break;
			case 6: s = SQUARES; break;
			case 7: s = IMPULSES; break;
			default: s = MARIO; break;
			}

			SPECTRUM(spec)->cfg->s_per = s;
			for (elem = SPECTRUM(spec)->cfg->per; elem;
			     elem = elem->next)
				xyplot_set_graph_style(plot, elem->data, s);
			xyplot_redraw(plot);
			break;
		}
		case HARNESS_OP_CLEAR_SEL:
			send_key(GDK_KEY_u);
			break;
		case HARNESS_OP_SEL_ALL:
			xyplot_select_all_data(plot);
			break;
		case HARNESS_OP_VREST:
			SPECTRUM(spec)->cfg->freq_ref_mhz =
				(gdouble) rd_u16(&r);
			xyplot_redraw(plot);
			break;
		case HARNESS_OP_COLOUR:
			op_colour(&r);
			break;
		case HARNESS_OP_DROP_ALL:
			xyplot_drop_all_graphs(plot);
			pts_live = 0;
			break;
		case HARNESS_OP_ZOOM: {
			gdouble x0, y0, x1, y1;

			x0 = rd_i16(&r);
			y0 = rd_i16(&r);
			x1 = rd_i16(&r);
			y1 = rd_i16(&r);

			drag(x0, y0, x1, y1, FALSE);
			break;
		}
		case HARNESS_OP_SCROLL: {
			gdouble x, y;

			x = rd_i16(&r);
			y = rd_i16(&r);

			scroll(rd_u8(&r) % 4, x, y);
			break;
		}
		case HARNESS_OP_POPUP:
			send_button(GDK_BUTTON_PRESS, 3, 0, 10.0, 10.0);
			break;
		case HARNESS_OP_POPDOWN: {
			XYPlot *p = XYPLOT(plot);

			/* dismiss the graph context menu so a graph that a drop
			 * deferred while the menu was up can finally be freed
			 */
			if (GTK_IS_WIDGET(p->menu)) {
				gtk_menu_popdown(GTK_MENU(p->menu));
				gtk_widget_hide(p->menu);
			}

			settle();
			break;
		}
		case HARNESS_OP_CLICK: {
			gdouble x, y;

			x = rd_i16(&r);
			y = rd_i16(&r);

			send_button(GDK_BUTTON_PRESS, 1, GDK_MOD1_MASK, x, y);
			break;
		}
		case HARNESS_OP_RESET:
			send_key(GDK_KEY_a);
			break;
		case HARNESS_OP_PROBE_NAN: {
			guint16 nn, nf, nt;

			nn = rd_u16(&r);
			nf = rd_u16(&r);
			nt = rd_u16(&r);

			harness_add_nan_graph(nn, nf, nt);
			pts_live += nn;
			break;
		}
		default:
			/* unknown opcode: resynchronise */
			break;
		}

		settle();
	}
}
