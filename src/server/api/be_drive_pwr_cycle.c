/**
 * @file    server/api/be_drive_pwr_cycle.c
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


static void (*p_drive_pwr_cycle)(void);


/**
 * @brief executes be_drive_pwr_cycle on a backend
 */

void be_drive_pwr_cycle(void)
{
	if (p_drive_pwr_cycle)
		p_drive_pwr_cycle();
	else
		g_message("BACKEND: function %s not available\n", __func__);
}


/**
 * @brief try to load the be_drive_pwr_cycle symbol in a backend plugin
 */

int be_drive_pwr_cycle_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_drive_pwr_cycle", &func);
	if(!ret)
		return -1;
	
        g_message("BACKEND: found symbol %s", __func__);

	p_drive_pwr_cycle = (typeof(p_drive_pwr_cycle)) func;
}
