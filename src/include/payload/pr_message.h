/**
 * @file    include/payload/pr_message.h
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
 * @brief payload structure for PR_MESSAGE
 *
 */

#ifndef _INCLUDE_PAYLOAD_PR_MESSAGE_H_
#define _INCLUDE_PAYLOAD_PR_MESSAGE_H_


struct message {
	uint16_t len;		/* the number of bytes in the text */
	uint8_t  message[];	/* any texy */
};


#endif /* _INCLUDE_PAYLOAD_PR_MESSAGE_H_ */
