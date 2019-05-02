/**
 * @file    server/proc/proc_pr_message.c
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



void proc_pr_message(struct packet *pkt, gpointer ref)
{
	struct message *c;



	c = (struct message *) pkt->data;


	if (strlen(c->message) != c->len)
		return;

	net_server_broadcast_message((const gchar *) c->message, ref);
}
