/**
 * @file    include/payload/pr_moveto.h
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
 * @brief payload structure for PR_MOVETO_*
 *
 */

#ifndef _INCLUDE_PAYLOAD_PR_MOVETO_H_
#define _INCLUDE_PAYLOAD_PR_MOVETO_H_


/**
 * PR_MOVETO_* packet payload structure
 */

struct moveto {
	int32_t az_arcsec;
	int32_t el_arcsec;
};



#endif /* _INCLUDE_PAYLOAD_PR_MOVETO_H_ */
