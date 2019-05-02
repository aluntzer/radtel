/**
 * @file    server/include/net.h
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

#ifndef _SERVER_INCLUDE_NET_H_
#define _SERVER_INCLUDE_NET_H_

#include <net_common.h>
#include <protocol.h>


int net_server(void);
void net_server_reassign_control(gpointer ref);
void net_server_broadcast_message(const gchar *msg, gpointer ref);
void net_server_set_nickname(const gchar *nick, gpointer ref);


#endif /* _SERVER_INCLUDE_NET_H_ */

