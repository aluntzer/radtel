/**
 * @file    server/api/be_hot_load_enable.c
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

#include <backend.h>


static int (*p_be_hot_load_enable)(gboolean mode);


/**
 * @brief executes HOT_LOAD enable/disable command on a backend
 *
 * @returns -1 on failure, 0 on success
 */

int be_hot_load_enable(gboolean mode)
{
	if (p_be_hot_load_enable)
		return p_be_hot_load_enable(mode);

	g_message("BACKEND: function %s not available\n", __func__);

	return -1;
}


/**
 * @brief try to load the be_hot_load_enable symbol in a backend plugin
 */

int be_hot_load_enable_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_hot_load_enable", &func);
	if(!ret)
		return -1;

        g_message("BACKEND: found symbol %s", __func__);

	p_be_hot_load_enable = (typeof(p_be_hot_load_enable)) func;
}
