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
#include <string.h>

#include <protocol.h>
#include <signals.h>



void proc_pr_spec_data(struct packet *pkt)
{
	struct spec_data *s;


	g_message("Server sent spectral data");

	s = g_malloc(pkt->data_size);

	memcpy(s, pkt->data, pkt->data_size);

	sig_pr_spec_data(s);

	/* cleanup */
	g_free(s);
}
