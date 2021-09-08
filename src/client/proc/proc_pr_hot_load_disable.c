/**
 * @file    client/proc/proc_pr_hot_load_disable.c
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


void proc_pr_hot_load_disable(void)
{
	g_debug("Server sent HOT_LOAD_DISABLE");
	sig_pr_hot_load_disable();
}
