/**
 * @file    client/sig/sig_pr_fail.c
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
#include <glib-object.h>
#include <signals.h>


/**
 * @brief emit pr-fail signal
 */

void sig_pr_fail(const uint16_t trans_id)
{
	g_debug("Emit signal \"pr-fail\"");

	g_signal_emit_by_name(sig_get_instance(), "pr-fail", trans_id);
}
