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
	be_spec_acq_cfg_load(mod);
	be_spec_acq_cfg_get_load(mod);
	be_getpos_azel_load(mod);
	be_spec_acq_enable_load(mod);
	be_get_capabilities_drive_load(mod);
	be_get_capabilities_spec_load(mod);
}




/**
 * @brief load a backend module from a given prefix
 *
 * @note if a module exports a symbol module_extra_init(), the
 *       function will be executed here
 *
 * @returns 0 on success, otherwise error
 */

static int backend_load_module_from_prefix(const gchar *plugin_path)
{
	GModule *mod;

	void (*mod_init)(void);


	g_message("Will try to load plugin from %s", plugin_path);
	mod = g_module_open(plugin_path, G_MODULE_BIND_LAZY);

	if(!mod) {
		g_warning("Unable to load plugin %s: %s", plugin_path,
							g_module_error());
		return -1;
	}

	g_message("Plugin loaded from %s", plugin_path);

	g_module_symbol(mod, "module_extra_init", (gpointer) &mod_init);

	if (mod_init)
		mod_init();



	backend_try_load_symbols(mod);


	return 0;
}


/**
 * @brief try to load a backend module from various paths
 */

static int backend_load_module(const gchar *plugin_path)
{
	int ret;

	gchar *plug;

	/* try supplied path first */
	ret = backend_load_module_from_prefix(plugin_path);

	if (!ret)
		return 0;


	/* try again in plugdir */
	plug = g_strconcat(PLUGDIR, "/", plugin_path, NULL);
	ret = backend_load_module_from_prefix(plug);
	g_free(plug);

	if (!ret)
		return 0;


	/* try again in lib/plugdir */
	plug = g_strconcat("lib/", PLUGDIR, "/", plugin_path, NULL);
	ret = backend_load_module_from_prefix(plug);
	g_free(plug);

	if (!ret)
		return 0;


	/* try again in system lib dir/plugdir */
	plug = g_strconcat(LIBDIR, "/", PLUGDIR, "/", plugin_path, NULL);
	ret = backend_load_module_from_prefix(plug);
	g_free(plug);

	if (ret) {
		g_warning("Could not find plugin: %s. "
			  "Also looked in %s and %s/%s",
			  plugin_path, PLUGDIR, LIBDIR, PLUGDIR);

		return -1;
	}




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
