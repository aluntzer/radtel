/**
 * @file    server/main.c
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
 * @brief the server main() function
 *
 * @todo we don't clean up on exit...
 */

#include <cfg.h>
#include <backend.h>
#include <net.h>


int main(void)
{
	if (server_cfg_load())
		return -1;

	if (backend_load_plugins())
		return -1;

	if (net_server())
		return -1;
}
