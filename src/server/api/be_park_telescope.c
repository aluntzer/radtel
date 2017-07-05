/**
 * @file    server/api/be_park_telescope.c
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


static void (*p_park_telescope)(void);


/**
 * @brief executes be_park_telescope on a backend
 */

void be_park_telescope(void)
{
	if (p_park_telescope)
		p_park_telescope();
	else
		g_message("BACKEND: function %s not available\n", __func__);
}


/**
 * @brief try to load the be_park_telescope symbol in a backend plugin
 */

int be_park_telescope_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_park_telescope", &func);
	if(!ret)
		return -1;
	
        g_message("BACKEND: found symbol %s", __func__);

	p_park_telescope = (typeof(p_park_telescope)) func;
}
