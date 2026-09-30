/**
 * @file    client/proc/proc_pr_spec_data.c
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
 */

#include <glib.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <protocol.h>
#include <signals.h>



void proc_pr_spec_data(struct packet *pkt)
{
	struct spec_data *s;

	gsize hdr, avail, n;


	g_debug("Server sent spectral data");

	hdr   = offsetof(struct spec_data, spec);
	avail = pkt->data_size;

	/* the payload lives at offset 10 of a packed packet, so it is never
	 * suitably aligned for struct spec_data; read it through a copy
	 */
	if (avail < hdr) {
		g_warning("PR_SPEC_DATA: payload of %" G_GSIZE_FORMAT
			  " bytes is shorter than the header", avail);
		return;
	}

	s = g_malloc(avail);

	memcpy(s, pkt->data, avail);

	/* the sender sizes the payload as sizeof(struct spec_data) + 4 * n,
	 * so the sample count must fit what actually arrived, otherwise the
	 * consumer reads past the copy
	 */
	n = (avail - hdr) / sizeof(uint32_t);

	if (s->n > n) {
		g_warning("PR_SPEC_DATA: n is %" PRIu32 " but the payload "
			  "carries at most %" G_GSIZE_FORMAT " samples, "
			  "dropping", s->n, n);

		g_free(s);

		return;
	}

	sig_pr_spec_data(s);

	/* cleanup */
	g_free(s);
}
