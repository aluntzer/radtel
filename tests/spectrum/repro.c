/**
 * @file    repro.c
 * @brief   deterministic scenarios for the spectrum widget fit-selection path
 *
 * usage: repro <name>   |   repro --hex <hexbytes>
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"


/* --- op encoders, mirroring harness.h --- */

#define OP_PUSH		0x01
#define OP_PUSH_VOID	0x02
#define OP_PUSH_SHORT	0x03
#define OP_PER		0x04
#define OP_AVG		0x05
#define OP_SEL		0x06
#define OP_FIT		0x07
#define OP_REDRAW	0x08
#define OP_STYLE	0x09
#define OP_CLEAR_SEL	0x0a
#define OP_SEL_ALL	0x0b
#define OP_VREST	0x0c
#define OP_COLOUR	0x0d
#define OP_DROP_ALL	0x0e
#define OP_ZOOM		0x0f
#define OP_SCROLL	0x10
#define OP_POPUP	0x11
#define OP_CLICK	0x12
#define OP_RESET	0x13
#define OP_PROBE_NAN	0x14
#define OP_POPDOWN	0x15


struct buf {
	unsigned char b[65536];
	size_t n;
};


static void put(struct buf *b, unsigned v)
{
	if (b->n < sizeof(b->b))
		b->b[b->n++] = (unsigned char) (v & 0xff);
}


static void push_ncode(struct buf *b, unsigned nc)
{
	put(b, OP_PUSH);
	put(b, nc);
	put(b, 0);	/* pattern, patched below */
	put(b, 0);
	put(b, 0);
	put(b, 0);
	put(b, 0);
}


static void sel(struct buf *b, int x0, int y0, int x1, int y1)
{
	put(b, OP_SEL);
	put(b, x0); put(b, x0 >> 8);
	put(b, y0); put(b, y0 >> 8);
	put(b, x1); put(b, x1 >> 8);
	put(b, y1); put(b, y1 >> 8);
}


static void set_pattern(struct buf *b, size_t at, unsigned pattern)
{
	b->b[at + 1] = (unsigned char) pattern;
}


static int run(struct buf *b, const char *name)
{
	printf("=== %s (%zu bytes)\n", name, b->n);
	fflush(stdout);

	harness_run(b->b, b->n);

	printf("--- %s: survived\n", name);
	fflush(stdout);

	return 0;
}


/* run a program and require the widget to have asked for exactly want fits */
static int run_fits(struct buf *b, const char *name, guint want)
{
	guint got;

	run(b, name);

	got = harness_fit_count();

	if (got != want) {
		printf("--- %s: FAIL, %u fit requests, expected %u\n",
		       name, got, want);
		fflush(stdout);

		return 1;
	}

	printf("--- %s: %u fit requests, as expected\n", name, got);
	fflush(stdout);

	return 0;
}


/* run a program and require the plot to hold at most want graphs; a refit
 * replaces its curves, so the count must not creep upwards
 */
static int run_graphs(struct buf *b, const char *name, guint want)
{
	guint got, pend;

	run(b, name);

	got  = harness_graph_count();
	pend = harness_graph_pending();

	if (got > want) {
		printf("--- %s: FAIL, %u graphs live (%u awaiting cleanup), "
		       "at most %u expected\n", name, got, pend, want);
		fflush(stdout);

		return 1;
	}

	printf("--- %s: %u graphs live, %u awaiting cleanup, at most %u\n",
	       name, got, pend, want);
	fflush(stdout);

	return 0;
}


/* dropping a graph reference that is no longer in the plot is what happens
 * when a fit curve reference is used again after the curve was dropped and
 * the reference was left dangling; the xyplot answers that with a
 * g_warning(), so count them over the run
 */

static guint stale_drops;

static void stale_drop_warn(const gchar *log_domain, GLogLevelFlags log_level,
			    const gchar *message, gpointer user_data)
{
	(void) log_domain;
	(void) log_level;
	(void) user_data;

	if (strstr(message, "graph reference not found"))
		stale_drops++;
}


static int run_no_stale(struct buf *b, const char *name, guint want)
{
	int rc;

	/* xyplot.c defines no G_LOG_DOMAIN, so its g_warning() arrives
	 * under a NULL domain rather than under "GLib"
	 */
	g_log_set_handler(NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL,
			  stale_drop_warn, NULL);

	stale_drops = 0;

	rc = run_graphs(b, name, want);

	g_log_set_handler(NULL, G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL,
			  g_log_default_handler, NULL);

	if (stale_drops) {
		printf("--- %s: FAIL, %u drops of a stale graph reference, "
		       "none expected\n", name, stale_drops);
		fflush(stdout);

		return 1;
	}

	printf("--- %s: no stale graph reference dropped\n", name);
	fflush(stdout);

	return rc;
}


/* run a program and require the plot geometry to have stayed finite */
static int run_finite(struct buf *b, const char *name)
{
	run(b, name);

	if (!harness_axes_finite()) {
		printf("--- %s: FAIL, plot geometry is not finite\n", name);
		fflush(stdout);

		return 1;
	}

	printf("--- %s: plot geometry finite\n", name);
	fflush(stdout);

	return 0;
}


int main(int argc, char **argv)
{
	struct buf b;
	const char *name;
	size_t at;
	int i;
	int rc = 0;

	if (harness_init() < 0) {
		fprintf(stderr, "harness init failed (no display?)\n");
		return 77;
	}

	if ((argc > 2) && !strcmp(argv[1], "--hex")) {
		size_t len = strlen(argv[2]);
		size_t k;

		memset(&b, 0, sizeof(b));
		for (k = 0; k * 2 < len; k++) {
			char t[3] = { argv[2][k * 2], argv[2][k * 2 + 1], 0 };
			b.b[b.n++] = (unsigned char) strtoul(t, NULL, 16);
		}
		return run(&b, "hex");
	}

	name = (argc > 1) ? argv[1] : "all";

	if (!strcmp(name, "all") || !strcmp(name, "nan-graph")) {
		/* isolate the render primitive: a graph whose every point is
		 * NaN, drawn with the NaN-aware line style
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PROBE_NAN);
		put(&b, 200); put(&b, 0);
		put(&b, 0);   put(&b, 0);
		put(&b, 200); put(&b, 0);
		put(&b, OP_REDRAW);
		run(&b, "nan-graph");
	}

	if (!strcmp(name, "all") || !strcmp(name, "nan-graph-head")) {
		/* only the leading points are NaN: a valid point exists */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PROBE_NAN);
		put(&b, 200); put(&b, 0);
		put(&b, 0);   put(&b, 0);
		put(&b, 199); put(&b, 0);
		put(&b, OP_REDRAW);
		run(&b, "nan-graph-head");
	}

	if (!strcmp(name, "all") || !strcmp(name, "nan-graph-tail")) {
		/* only the trailing points are NaN: also has a valid point */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PROBE_NAN);
		put(&b, 200); put(&b, 0);
		put(&b, 1);   put(&b, 0);
		put(&b, 200); put(&b, 0);
		put(&b, OP_REDRAW);
		run(&b, "nan-graph-tail");
	}

	if (!strcmp(name, "all") || !strcmp(name, "nan-graph-2")) {
		/* a two-point all-NaN graph: the smallest graph the style
		 * will still draw
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PROBE_NAN);
		put(&b, 2); put(&b, 0);
		put(&b, 0); put(&b, 0);
		put(&b, 2); put(&b, 0);
		put(&b, OP_REDRAW);
		run(&b, "nan-graph-2");
	}

	if (!strcmp(name, "all") || !strcmp(name, "nan-graph-1")) {
		/* a single all-NaN point: below the style's minimum length */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PROBE_NAN);
		put(&b, 1); put(&b, 0);
		put(&b, 0); put(&b, 0);
		put(&b, 1); put(&b, 0);
		put(&b, OP_REDRAW);
		run(&b, "nan-graph-1");
	}

	if (!strcmp(name, "all") || !strcmp(name, "clear-plot")) {
		/* the plot's "Clear Plot" menu item, then the next spectrum
		 * packet: the spectrum side still holds the freed graph
		 * references
		 */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);
		b.b[at + 2] = 10;

		put(&b, OP_DROP_ALL);

		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);

		run(&b, "clear-plot");
	}

	if (!strcmp(name, "all") || !strcmp(name, "clear-plot-one")) {
		/* the same, but with a single persistence graph in the list,
		 * so the stale element is also the head of the list
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PER);
		put(&b, 1); put(&b, 0);

		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);

		put(&b, OP_DROP_ALL);

		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);

		run(&b, "clear-plot-one");
	}

	if (!strcmp(name, "all") || !strcmp(name, "clear-plot-mid")) {
		/* the same, but with three graphs, so the stale element is in
		 * the middle of the list -- the "one step back" branch
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_PER);
		put(&b, 3); put(&b, 0);

		for (i = 0; i < 3; i++) {
			at = b.n;
			push_ncode(&b, 40);
			set_pattern(&b, at, 1);
		}

		put(&b, OP_DROP_ALL);

		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);

		run(&b, "clear-plot-mid");
	}

	if (!strcmp(name, "all") || !strcmp(name, "flat-selection")) {
		/* a plain, plausible spectrum, then a small selection over the
		 * peak: the case the users say crashes immediately
		 */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 30);
		set_pattern(&b, at, 1);
		b.b[at + 2] = 8;	/* amplitude */
		sel(&b, 200, 150, 600, 450);
		put(&b, OP_REDRAW);
		run(&b, "flat-selection");
	}

	if (!strcmp(name, "all") || !strcmp(name, "zero-band")) {
		/* every selected bin is exactly 0 mK */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 30);
		set_pattern(&b, at, 2);
		sel(&b, 100, 100, 700, 500);
		put(&b, OP_REDRAW);
		run(&b, "zero-band");
	}

	if (!strcmp(name, "all") || !strcmp(name, "few-points")) {
		/* a very small number of bins, so the selection can catch only
		 * a handful of points
		 */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 0);
		set_pattern(&b, at, 6);
		b.b[at + 2] = 4;
		sel(&b, 300, 200, 320, 260);
		put(&b, OP_FIT);
		put(&b, OP_REDRAW);
		run(&b, "few-points");
	}

	if (!strcmp(name, "all") || !strcmp(name, "select-all")) {
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);
		b.b[at + 2] = 10;
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		run(&b, "select-all");
	}

	if (!strcmp(name, "all") || !strcmp(name, "refit-loop")) {
		/* selection stays active while data keeps arriving: the fit
		 * re-runs on every packet and its own output is inside the
		 * selection
		 */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 20);
		set_pattern(&b, at, 1);
		b.b[at + 2] = 6;
		sel(&b, 150, 120, 650, 470);
		for (i = 0; i < 40; i++) {
			at = b.n;
			push_ncode(&b, 20);
			set_pattern(&b, at, (unsigned) (i % 9));
			b.b[at + 2] = (unsigned char) (i + 1);
		}
		put(&b, OP_REDRAW);
		run(&b, "refit-loop");
	}

	if (!strcmp(name, "all") || !strcmp(name, "short-packet")) {
		memset(&b, 0, sizeof(b));
		put(&b, OP_PUSH_SHORT);
		put(&b, 200);
		put(&b, 0);
		put(&b, 5);
		put(&b, 0);
		put(&b, 0);
		put(&b, 0);
		run(&b, "short-packet");
	}

	if (!strcmp(name, "all") || !strcmp(name, "style-sweep")) {
		/* every draw style over a large point cloud */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 250);
		set_pattern(&b, at, 1);
		b.b[at + 2] = 20;
		sel(&b, 50, 50, 800, 550);
		for (i = 0; i < 9; i++) {
			put(&b, OP_STYLE);
			put(&b, (unsigned) i);
		}
		put(&b, OP_REDRAW);
		run(&b, "style-sweep");
	}

	if (!strcmp(name, "all") || !strcmp(name, "menu-drop")) {
		/* graphs dropped while the graph menu holds references */
		memset(&b, 0, sizeof(b));
		at = b.n;
		push_ncode(&b, 40);
		set_pattern(&b, at, 1);
		b.b[at + 2] = 10;
		sel(&b, 200, 150, 600, 450);
		put(&b, OP_POPUP);
		for (i = 0; i < 8; i++) {
			at = b.n;
			push_ncode(&b, 40);
			set_pattern(&b, at, 1);
		}
		put(&b, OP_REDRAW);
		run(&b, "menu-drop");
	}

	if (!strcmp(name, "all") || !strcmp(name, "persistence-churn")) {
		/* data and average spin buttons moved around while data lands */
		memset(&b, 0, sizeof(b));
		for (i = 0; i < 30; i++) {
			at = b.n;
			push_ncode(&b, 60);
			set_pattern(&b, at, (unsigned) (i % 10));

			put(&b, OP_PER);
			put(&b, (unsigned) (i * 37) % 1001);
			put(&b, (unsigned) ((i * 37) % 1001) >> 8);

			put(&b, OP_AVG);
			put(&b, (unsigned) (i * 53) % 1001);
			put(&b, (unsigned) ((i * 53) % 1001) >> 8);
		}
		put(&b, OP_REDRAW);
		run(&b, "persistence-churn");
	}

	if (!strcmp(name, "all") || !strcmp(name, "degenerate-fit")) {
		/* a zero-width box must not ask for a fit; once the scale is
		 * valid again the same gesture must ask for exactly one
		 */
		memset(&b, 0, sizeof(b));
		for (i = 0; i < 2; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}

		put(&b, OP_SEL);
		put(&b, 300);
		put(&b, 300 >> 8);
		put(&b, 300);
		put(&b, 300 >> 8);
		put(&b, 300);
		put(&b, 300 >> 8);
		put(&b, 400);
		put(&b, 400 >> 8);

		put(&b, OP_REDRAW);

		put(&b, OP_SEL);
		put(&b, 200);
		put(&b, 200 >> 8);
		put(&b, 200);
		put(&b, 200 >> 8);
		put(&b, 500);
		put(&b, 500 >> 8);
		put(&b, 400);
		put(&b, 400 >> 8);

		rc |= run_fits(&b, "degenerate-fit", 1);
	}

	if (!strcmp(name, "all") || !strcmp(name, "stale-scale")) {
		/* gestures issued with no redraw in between: the pixel-to-data
		 * scale is whatever the last draw left behind, and the drag must
		 * still leave finite axes and a finite selection
		 */
		memset(&b, 0, sizeof(b));
		for (i = 0; i < 4; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}

		put(&b, OP_ZOOM);
		put(&b, 200);
		put(&b, 200 >> 8);
		put(&b, 200);
		put(&b, 200 >> 8);
		put(&b, 500);
		put(&b, 500 >> 8);
		put(&b, 400);
		put(&b, 400 >> 8);

		put(&b, OP_SEL);
		put(&b, 200);
		put(&b, 200 >> 8);
		put(&b, 200);
		put(&b, 200 >> 8);
		put(&b, 500);
		put(&b, 500 >> 8);
		put(&b, 400);
		put(&b, 400 >> 8);

		put(&b, OP_FIT);
		put(&b, OP_REDRAW);
		rc |= run_finite(&b, "stale-scale");
	}

	if (!strcmp(name, "all") || !strcmp(name, "refit-once")) {
		/* one gesture, then a run of arriving packets: the fit must be
		 * refreshed as the data arrives, not only on the gesture
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_SEL_ALL);
		for (i = 0; i < 25; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
			b.b[at + 2] = (unsigned char) (i + 1);
		}
		put(&b, OP_REDRAW);
		rc |= run_fits(&b, "refit-once", 21);
	}

	if (!strcmp(name, "all") || !strcmp(name, "refit-on-packet")) {
		/* a gesture, then arriving packets, then a second gesture: the
		 * packets in between refresh the fit as well
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_SEL_ALL);
		for (i = 0; i < 10; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		rc |= run_fits(&b, "refit-on-packet", 22);
	}

	if (!strcmp(name, "all") || !strcmp(name, "fit-graph-leak")) {
		/* a fit, then the graph context menu, then a long run of
		 * refits: dropping a graph while the menu is up defers its
		 * free, and the fit curves must not pile up regardless
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		put(&b, OP_POPUP);
		for (i = 0; i < 20; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}
		put(&b, OP_REDRAW);
		rc |= run_graphs(&b, "fit-graph-leak", 40);
	}

	if (!strcmp(name, "all") || !strcmp(name, "fit-menu-refit")) {
		/* the same, but the selection is dropped and remade in between,
		 * which is where a stale curve reference can go wrong
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		put(&b, OP_CLEAR_SEL);
		for (i = 0; i < 10; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		rc |= run_graphs(&b, "fit-menu-refit", 40);
	}

	if (!strcmp(name, "all") || !strcmp(name, "fit-ref-stale")) {
		/* make a fit so both curve references are live, then move the
		 * band off the data: the selection stays active but holds no
		 * points, so the fit handler drops both curves. the references
		 * must be cleared there, or the next fit drops freed memory
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		for (i = 0; i < 6; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
			sel(&b, 30000, 30000, 31000, 31000);
			put(&b, OP_REDRAW);
		}
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		for (i = 0; i < 6; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
			sel(&b, 30000, 30000, 31000, 31000);
			put(&b, OP_REDRAW);
		}
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		rc |= run_no_stale(&b, "fit-ref-stale", 40);
	}

	if (!strcmp(name, "all") || !strcmp(name, "fit-drop-all-ref")) {
		/* make a fit so both curve references are live, then drop every
		 * graph the way the plot's "Clear Plot" item does: the plot
		 * frees the fit curves without the fit having asked for it, so
		 * the stored references must be cleared there or the next fit
		 * drops freed memory
		 */
		memset(&b, 0, sizeof(b));

		for (i = 0; i < 6; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}

		/* a fit over the data, so both curve references are live */
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);

		put(&b, OP_DROP_ALL);

		at = b.n;
		push_ncode(&b, 30);
		set_pattern(&b, at, 1);

		/* re-arm the band: the next graph to arrive asks for a fit,
		 * and that fit is the one that drops the stale references
		 */
		put(&b, OP_SEL_ALL);

		at = b.n;
		push_ncode(&b, 30);
		set_pattern(&b, at, 2);

		put(&b, OP_REDRAW);
		put(&b, OP_REDRAW);

		rc |= run_no_stale(&b, "fit-drop-all-ref", 40);
	}

	if (!strcmp(name, "all") || !strcmp(name, "fit-menu-drop-all")) {
		/* the same, but with the graph context menu up: the drop must
		 * be deferred rather than free the graphs the open menu still
		 * points at, and the references must be cleared once the
		 * deferred free runs
		 */
		memset(&b, 0, sizeof(b));

		for (i = 0; i < 6; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i % 9));
		}

		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);

		put(&b, OP_POPUP);
		put(&b, OP_DROP_ALL);
		put(&b, OP_POPDOWN);

		at = b.n;
		push_ncode(&b, 30);
		set_pattern(&b, at, 1);

		put(&b, OP_SEL_ALL);

		for (i = 0; i < 4; i++) {
			at = b.n;
			push_ncode(&b, 30);
			set_pattern(&b, at, (unsigned) (i + 1));
			put(&b, OP_REDRAW);
		}

		rc |= run_no_stale(&b, "fit-menu-drop-all", 40);
	}

	if (!strcmp(name, "all") || !strcmp(name, "fit-deferred")) {
		/* the fit request raised by a new graph must not be delivered
		 * from inside xyplot_add_graph(), otherwise the re-entrant fit
		 * handler paints the plot while that graph still wears the
		 * add_graph defaults
		 */
		memset(&b, 0, sizeof(b));
		put(&b, OP_SEL_ALL);
		put(&b, OP_REDRAW);
		rc |= run(&b, "fit-deferred");

		if (harness_fit_deferred()) {
			printf("--- fit-deferred: PASS, the fit request waits "
			       "for the main context\n");
			fflush(stdout);
		} else {
			printf("--- fit-deferred: FAIL, the fit request was "
			       "delivered inside xyplot_add_graph()\n");
			fflush(stdout);
			rc |= 1;
		}
	}

	return rc;
}
