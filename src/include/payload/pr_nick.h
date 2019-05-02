/**
 * @file    include/payload/pr_nick.h
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
 * @brief payload structure for PR_NICK
 *
 */

#ifndef _INCLUDE_PAYLOAD_PR_NICK_H_
#define _INCLUDE_PAYLOAD_PR_NICK_H_


struct nick {
	uint16_t len;		/* the number of bytes in the text */
	uint8_t  nick[];	/* any texy */
};


#endif /* _INCLUDE_PAYLOAD_PR_NICK_H_ */
