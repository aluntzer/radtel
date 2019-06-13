/**
 * @file    include/protocol.h
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
 * NOTE: * all payload data are expected in little-endian order
 *
 *	 * the transaction identifier is copied into an ACK packet, so the
 *	   client can track success/failure of commands if so desired;
 *	   transaction identifiers not recorded in a client's command log
 *	   should be ignored, as there could be multiple clients (with different
 *	   priviledge levels) requesting various server parameters;
 *	   packets sent without a designated transaction identifiers should
 *	   use PKT_TRANS_ID_UNDEF as the identifier
 *
 *	 * the primary network performance bottleneck is the CRC16, so if this
 *	   ever needs adaptation to very high data rate, the algorithm or the
 *	   implementation must be replaced or threaded.
 *	   Alternatively, protocol command could turn of CRC checks for certain
 *	   types of payloads.
 *	   Some approximate figures: on an i7-5700HQ CPU @ 2.70GHz, the server
 *	   can send about 400 MiBs on the loopback with CRC, and about 900 MiBs
 *	   without.
 *
 *
 */

#ifndef _INCLUDE_PROTOCOL_H_
#define _INCLUDE_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>

#include <payload/pr_capabilities.h>
#include <payload/pr_moveto.h>
#include <payload/pr_spec_acq_cfg.h>
#include <payload/pr_spec_data.h>
#include <payload/pr_getpos.h>
#include <payload/pr_status.h>
#include <payload/pr_control.h>
#include <payload/pr_message.h>
#include <payload/pr_userlist.h>
#include <payload/pr_nick.h>


#define DEFAULT_PORT 1420


/**
 * service command/response protocol identifiers
 */

#define PR_INVALID_PKT		0xa001	/* invalid packet signal */
#define PR_CAPABILITIES		0xa002	/* capabilities of the telescope */
#define PR_STATIONNAME		0xa003	/* name of station */
#define PR_CONTROL		0xa004	/* request control of the telescope */
#define PR_MOVETO_AZEL		0xa005	/* move to azimuth/elevation */
#define PR_SUCCESS		0xa006  /* last command succeded */
#define PR_FAIL			0xa007	/* last command failed */
#define PR_RECAL_POINTING	0xa008	/* recalibrate telescope pointing */
#define PR_PARK_TELESCOPE	0xa009	/* park telescope */
#define PR_SPEC_ACQ_CFG		0xa00a	/* spectrum acquisition configuration */
#define PR_SPEC_DATA		0xa00b	/* spectral data */
#define PR_GETPOS_AZEL		0xa00c	/* get azimuth/elevation */
#define PR_SPEC_ACQ_ENABLE	0xa00d  /* enable spectrum acquisition */
#define PR_SPEC_ACQ_DISABLE	0xa00e  /* disable spectrum acquisition */
#define PR_SPEC_ACQ_CFG_GET	0xa00f	/* get spectrum acquisition config */
#define PR_STATUS_ACQ		0xa010	/* spectrum acquisition status */
#define PR_STATUS_SLEW		0xa011	/* drive slew status */
#define PR_STATUS_MOVE		0xa012	/* drive move (to target) status */
#define PR_STATUS_REC		0xa013	/* (full) spectrum record status */
#define PR_NOPRIV		0xa014	/* lack of priviledge */
#define PR_MESSAGE		0xa015	/* an arbitrary message */
#define PR_USERLIST		0xa016	/* a list of connected users */
#define PR_NICK			0xa017	/* to set the user's nickname */



/**
 * @brief command exchange packet structure
 */

#define PKT_TRANS_ID_UNDEF	0xffff

struct packet {
	uint16_t service;
	uint16_t trans_id;
	uint16_t data_crc16;
	uint32_t data_size;
	char data[];
} __attribute__((packed));

/* arbitrarily limit maximum payload size to 32 MiB */
#define MAX_PAYLOAD_SIZE	0x2000000UL
#define MAX_PACKET_SIZE (sizeof(struct packet) + MAX_PAYLOAD_SIZE)


size_t pkt_size_get(struct packet *pkt);

void pkt_hdr_to_net_order(struct packet *pkt);
void pkt_hdr_to_host_order(struct packet *pkt);

void pkt_set_data_crc16(struct packet *pkt);

uint16_t CRC16(unsigned char *buf, size_t size);



#endif /* _INCLUDE_PROTOCOL_H_ */
