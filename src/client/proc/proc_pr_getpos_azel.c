/**
 * @file    client/proc/proc_pr_getpos_azel.c
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



void proc_pr_getpos_azel(struct packet *pkt)
{
	struct getpos *pos;


	g_message("Server sent spectral data");

	pos = g_malloc(pkt->data_size);

	memcpy(pos, pkt->data, pkt->data_size);

	sig_pr_getpos_azel(pos);

	/* cleanup */
	g_free(pos);
}
