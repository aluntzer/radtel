/**
 * @file    server/api/be_shared_comlink_read.c
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


static gchar *(*p_shared_comlink_read)(gsize *nbytes);


/**
 * @brief executes be_shared_comlink_read on a backend
 *
 * @param[out] nbytes the number of bytes read
 *
 * @returns the result of the read
 *
 * @note this function may block
 *
 * @note g_free() the returned buffer to deallocate
 */

gchar *be_shared_comlink_read(gsize *nbytes)
{
	gchar *buf = NULL;

	if (p_shared_comlink_read)
		buf = p_shared_comlink_read(nbytes);
	else
		g_message("BACKEND: function %s not available\n", __func__);

	return buf;
}


/**
 * @brief try to load the be_shared_comlink_read symbol in a backend plugin
 */

int be_shared_comlink_read_load(GModule *mod)
{
	gboolean ret;

	gpointer func;


	ret = g_module_symbol(mod, "be_shared_comlink_read", &func);
	if(!ret)
		return -1;
	
        g_message("BACKEND: found symbol %s", __func__);

	p_shared_comlink_read = (typeof(p_shared_comlink_read)) func;
}
