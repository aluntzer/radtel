/**
 * @file    server/api/be_drive_pwr_status.c
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


static gboolean (*p_drive_pwr_status)(void);


/**
 * @brief executes be_drive_pwr_status on a backend
 */

gboolean be_drive_pwr_status(void)
{
	if (p_drive_pwr_status)
		return p_drive_pwr_status();
	else
		g_message("BACKEND: function %s not available\n", __func__);

	return TRUE;	/* assume default == on */
}


/**
 * @brief try to load the be_drive_pwr_status symbol in a backend plugin
 */

int be_drive_pwr_status_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_drive_pwr_status", &func);
	if(!ret)
		return -1;
	
        g_message("BACKEND: found symbol %s", __func__);

	p_drive_pwr_status = (typeof(p_drive_pwr_status)) func;
}
