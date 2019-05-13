/**
 * @file    client/main.c
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
 * @brief the client main() function
 */

#include <glib-object.h>
#include <net.h>
#include <gui.h>
#include <signals.h>


/**
 * @brief handle status slew
 */

static void main_shutdown_cb(gpointer instance, gpointer data)
{
	net_disconnect();
	g_main_loop_quit((GMainLoop *) data);
}


int main(int argc, char *argv[])
{
	GMainLoop *loop;

	loop = g_main_loop_new(NULL, FALSE);

	/* initialise the signal server */
	sig_init();

	/* build the gui */
	gui_client(argc, argv);


	/* initialise networking */
	net_client_init();

	g_signal_connect(sig_get_instance(), "shutdown",
			 G_CALLBACK(main_shutdown_cb),
			 (gpointer) loop);

	g_main_loop_run(loop);


	return 0;
}
