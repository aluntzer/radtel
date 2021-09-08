/**
 * @file    include/payload/common.h
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
 * @brief common structures for payloads
 *
 */

#ifndef _INCLUDE_PAYLOAD_COMMON_H_
#define _INCLUDE_PAYLOAD_COMMON_H_

struct local_horizon {
	int32_t az;
	int32_t el;
};


#endif /* _INCLUDE_PAYLOAD_COMMON_H_ */
