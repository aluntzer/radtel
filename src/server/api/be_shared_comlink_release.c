/**
 * @file    server/api/be_shared_comlink_release.c
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


static void (*p_shared_comlink_release)(void);


/**
 * @brief executes be_shared_comlink_release on a backend
 */

void be_shared_comlink_release(void)
{
	if (p_shared_comlink_release)
		p_shared_comlink_release();
	else
		g_message("BACKEND: function %s not available\n", __func__);
}


/**
 * @brief try to load the be_shared_comlink_release symbol in a backend plugin
 */

int be_shared_comlink_release_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_shared_comlink_release", &func);
	if(!ret)
		return -1;
	
        g_message("BACKEND: found symbol %s", __func__);

	p_shared_comlink_release = (typeof(p_shared_comlink_release)) func;
}
