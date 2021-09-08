/**
 * @file    server/api/be_get_capabilities_spec.c
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


static int (*p_be_get_capabilities_spec)(struct capabilities *c);


/**
 * @brief gets spectrometer capabilities from a backend
 *
 * @returns -1 on failure, 0 on success
 */

int be_get_capabilities_spec(struct capabilities *c)
{
	if (p_be_get_capabilities_spec)
		return p_be_get_capabilities_spec(c);

	g_message("BACKEND: function %s not available\n", __func__);

	return -1;
}


/**
 * @brief try to load the be_be_get_capabilities_spec symbol in a backend plugin
 */

int be_get_capabilities_spec_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_get_capabilities_spec", &func);
	if(!ret)
		return -1;

        g_message("BACKEND: found symbol %s", __func__);

	p_be_get_capabilities_spec = (typeof(p_be_get_capabilities_spec)) func;
}
