/**
 * @file    include/payload/pr_control.h
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
 * @brief payload structure for PR_CONTROL
 *
 */

#ifndef _INCLUDE_PAYLOAD_PR_CONTROL_H_
#define _INCLUDE_PAYLOAD_PR_CONTROL_H_


struct control {
	uint16_t len;		/* the number of bytes in the digest */
	uint8_t  digest[];	/* a hmac-256 digest */
};


#endif /* _INCLUDE_PAYLOAD_PR_CONTROL_H_ */
