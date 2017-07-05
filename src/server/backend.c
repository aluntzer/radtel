/**
 * @file    server/backend.c
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
 * @brief backend plugin loader
 *
 * @note we need to introduce open module tracking, so we can be good and
 *	 clean up on exit
 */

#include <cfg.h>
#include <backend.h>

#include <gmodule.h>
#include <glib.h>


/**
 * @brief try load the symbols in a backend plugin
 */


static void backend_try_load_symbols(GModule *mod)
{
	be_moveto_azel_load(mod);
	be_shared_comlink_acquire_load(mod);
	be_shared_comlink_release_load(mod);
	be_shared_comlink_write_load(mod);
	be_shared_comlink_read_load(mod);
	be_recalibrate_pointing_load(mod);
	be_park_telescope_load(mod);
}



/**
 * @brief load a backend module
 * @note if a module exports a symbol module_extra_init(), the 
 *       function will be executed here
 */

static int backend_load_module(const gchar *plugin_path)
{
	GModule *mod;

	void (*mod_init)(void);



	mod = g_module_open(plugin_path, G_MODULE_BIND_LAZY);

	if(!mod) {
		g_error("Unable to load plugin %s", plugin_path);
		return -1;
	}

	g_module_symbol(mod, "module_extra_init", (gpointer) &mod_init);

	if (mod_init)
		mod_init();


	
	backend_try_load_symbols(mod);


	return 0;
}



/**
 * @brief load the configured backend plugins
 */

int backend_load_plugins(void)
{
	gsize i;
	gchar **pluglist;


	if (!g_module_supported()) {
		g_error("Module loading not supported on platform.");
		return -1;
	}


	pluglist = server_cfg_get_plugins();
	if (!pluglist)
		return -1;


	for (i = 0; pluglist[i] != NULL; i++)  {
		g_message("Loading plugin %s", pluglist[i]);
		backend_load_module(pluglist[i]);
	}


	return 0;
}
