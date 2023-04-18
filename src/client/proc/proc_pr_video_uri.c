/**
 * @file    client/proc/proc_pr_video_uri.c
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

#include <protocol.h>
#include <signals.h>



void proc_pr_video_uri(struct packet *pkt)
{
	const struct video_uri *c;


	g_debug("Server sent video_uri");

	c = (const struct video_uri *) pkt->data;


	if (strlen(c->video_uri) != c->len)
		return;

	sig_pr_video_uri(c->video_uri);
}
