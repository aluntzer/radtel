/**
 * @file    include/payload/pr_status.h
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
 * @brief payload structure for PR_STATUS_*
 *
 */

#ifndef _INCLUDE_PAYLOAD_PR_STATUS_H_
#define _INCLUDE_PAYLOAD_PR_STATUS_H_


/**
 * PR_STATUS_* packet payload structure
 */

struct status {
	int32_t  busy;		/* flag */
	uint32_t eta_msec;	/* ETA until not busy (0 == unknown) */
};



#endif /* _INCLUDE_PAYLOAD_PR_STATUS_H_ */
