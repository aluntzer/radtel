/**
 * @file    include/net_common.h
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

#ifndef _INCLUDE_NET_COMMON_H_
#define _INCLUDE_NET_COMMON_H_

#include <protocol.h>

#include <glib.h>

gint net_send(const char *pkt, gsize nbytes);
gint net_send_single(gpointer ref, const char *pkt, gsize nbytes);

#endif /* _INCLUDE_NET_COMMON_H_ */

