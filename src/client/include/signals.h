/**
 * @file    client/include/signals.h
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

#ifndef _CLIENT_INCLUDE_SIGNALS_H_
#define _CLIENT_INCLUDE_SIGNALS_H_

#include <protocol.h>

void sig_cmd_success(void);
void sig_cmd_capabilities(const struct capabilities *c);
void sig_cmd_spec_data(const struct spec_data *c);



gpointer *sig_get_instance(void);
void sig_init(void);

#endif /* _CLIENT_INCLUDE_SIGNALS_H_ */
