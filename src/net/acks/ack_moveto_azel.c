/**
 * @file    acks/ack_moveto_azel.c
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

#include <glib.h>

#include <cmd.h>
#include <ack.h>
#include <net_common.h>


struct packet *ack_moveto_azel_gen(uint16_t trans_id, double az, double el)
{
	return cmd_moveto_azel_gen(trans_id, az, el);
}



/**
 * @brief acknowledge moveto_azel command
 *
 */
void ack_moveto_azel(uint16_t trans_id, double az, double el)
{
	cmd_moveto_azel(trans_id, az, el);
}
