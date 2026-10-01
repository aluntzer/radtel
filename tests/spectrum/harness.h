/**
 * @file    harness.h
 * @brief   shared driver for the spectrum widget fit-selection harness
 *
 * The byte stream is interpreted as a short program of operations against a
 * live, realized Spectrum widget.  The widget is built exactly as
 * gui_create_spectrum_controls() builds it, data arrives through the same
 * "pr-spec-data" signal the network layer uses, and the fit is triggered by
 * synthesizing the real Ctrl+drag gesture and button release.
 */

#ifndef _HARNESS_H_
#define _HARNESS_H_

#include <gtk/gtk.h>

/* total xy-points allowed to be live in the plot at any one time */
#define HARNESS_MAX_POINTS	1000000

/* operations */
enum harness_op {
	HARNESS_OP_NOP		= 0x00,
	HARNESS_OP_PUSH		= 0x01,	/* deliver a spec_data packet */
	HARNESS_OP_PUSH_VOID	= 0x02,	/* deliver an empty packet */
	HARNESS_OP_PUSH_SHORT	= 0x03,	/* deliver a truncated packet */
	HARNESS_OP_PER		= 0x04,	/* data-persistence spin button */
	HARNESS_OP_AVG		= 0x05,	/* average-length spin button */
	HARNESS_OP_SEL		= 0x06,	/* ctrl+drag selection + release */
	HARNESS_OP_FIT		= 0x07,	/* emit xyplot-fit-selection */
	HARNESS_OP_REDRAW	= 0x08,
	HARNESS_OP_STYLE	= 0x09,
	HARNESS_OP_CLEAR_SEL	= 0x0a,
	HARNESS_OP_SEL_ALL	= 0x0b,	/* xyplot_select_all_data() */
	HARNESS_OP_VREST	= 0x0c,
	HARNESS_OP_COLOUR	= 0x0d,
	HARNESS_OP_DROP_ALL	= 0x0e,
	HARNESS_OP_ZOOM		= 0x0f,	/* plain drag: zoom, no selection */
	HARNESS_OP_SCROLL	= 0x10,
	HARNESS_OP_POPUP	= 0x11,	/* open the graph context menu */
	HARNESS_OP_CLICK	= 0x12,	/* mod1+click: retune acq */
	HARNESS_OP_RESET	= 0x13,	/* autorange (key "a") */
	HARNESS_OP_PROBE_NAN	= 0x14,	/* all-NaN graph, NaN-line style */
	HARNESS_OP_POPDOWN	= 0x15,	/* dismiss the graph context menu */
	HARNESS_OP_MAX
};

int harness_init(void);
void harness_run(const guint8 *data, size_t size);
gsize harness_points_live(void);

/* number of "xyplot-fit-selection" emissions seen since the last reset */
guint harness_fit_count(void);

/* TRUE when the plot axes, the pixel-to-data scale and the current
 * selection are all finite; a drag must never leave them otherwise
 */
gboolean harness_axes_finite(void);

/* graphs currently held by the plot, and graphs dropped while the graph
 * context menu was up and therefore awaiting deferred cleanup
 */
guint harness_graph_count(void);
guint harness_graph_pending(void);

/* push one spectrum packet and check the fit request it triggers arrives
 * only once the main context runs, never from inside xyplot_add_graph()
 */
gboolean harness_fit_deferred(void);

/* clear the plot without tearing down the widget */
void harness_reset(void);

/* add a graph with NaN in [nan_from, nan_to) drawn with the NaN-aware
 * line style, to exercise the render primitive on its own
 */
void harness_add_nan_graph(size_t n, size_t nan_from, size_t nan_to);

#endif /* _HARNESS_H_ */
