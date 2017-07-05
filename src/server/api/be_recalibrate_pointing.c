/**
 * @file    server/api/be_recalibrate_pointing.c
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


static void (*p_recalibrate_pointing)(void);


/**
 * @brief executes be_recalibrate_pointing on a backend
 */

void be_recalibrate_pointing(void)
{
	if (p_recalibrate_pointing)
		p_recalibrate_pointing();
	else
		g_message("BACKEND: function %s not available\n", __func__);
}


/**
 * @brief try to load the be_recalibrate_pointing symbol in a backend plugin
 */

int be_recalibrate_pointing_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_recalibrate_pointing", &func);
	if(!ret)
		return -1;
	
        g_message("BACKEND: found symbol %s", __func__);

	p_recalibrate_pointing = (typeof(p_recalibrate_pointing)) func;
}
