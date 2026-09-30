/**
 * @file    harness_stubs.c
 * @brief   link-time stubs so the client widget sources can be driven without
 *	    a network transport
 */

#include <glib.h>

#include <net_common.h>


/* the widget only ever calls these to ship a command to the server; the
 * harness has no transport, so drop them
 */

gint net_send(const char *pkt, gsize nbytes)
{
	(void) pkt;
	(void) nbytes;

	return 0;
}


gint net_send_single(gpointer ref, const char *pkt, gsize nbytes)
{
	(void) ref;
	(void) pkt;
	(void) nbytes;

	return 0;
}
