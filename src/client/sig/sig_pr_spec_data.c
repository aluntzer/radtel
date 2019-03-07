/**
 * @file    client/sig/sig_pr_spec_data.c
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
 * @brief emit pr-spec-data signal
 */

void sig_pr_spec_data(const struct spec_data *s)
{
	g_debug("Emit signal \"pr-spec-data\"");

	g_signal_emit_by_name(sig_get_instance(), "pr-spec-data", s);
}
