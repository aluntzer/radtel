/**
 * @file    server/proc/proc_pr_control.c
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

#include <ack.h>
#include <backend.h>
#include <cfg.h>
#include <net.h>



void proc_pr_control(struct packet *pkt, gpointer ref)
{
	gchar *digest;

	struct control *c;



	c = (struct control *) pkt->data;


	if (strlen(c->digest) != c->len)
		return;

	digest = g_compute_hmac_for_string(G_CHECKSUM_SHA256,
					   (guint8*)"radtel", 6,
					   "thisishardcoed", 13);



	if (!strcmp(digest, c->digest)) {
		g_message("Client telescope control reassigned");
		net_server_reassign_control(ref);
	} else {
		g_message("Client telescope control NOT reassigned, digest "
			  "mismatch %s %s", digest, c->digest);
	}

	g_free(digest);
}
