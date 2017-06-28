/**
 * @file    server/include/backend.h
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

#ifndef _SERVER_INCLUDE_BACKEND_H_
#define _SERVER_INCLUDE_BACKEND_H_

#include <gmodule.h>

/* backend calls */
int be_moveto_azel(double az, double el);


/* backend call loaders */
int be_moveto_azel_load(GModule *mod);



int backend_load_plugins(void);



#endif /* _SERVER_INCLUDE_BACKEND_H_ */

