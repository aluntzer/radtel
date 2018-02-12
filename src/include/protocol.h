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
 * NOTE: all payload data are expected in little-endian order!
 *
 */

#ifndef _INCLUDE_PROTOCOL_H_
#define _INCLUDE_PROTOCOL_H_

#include <stdint.h>
#include <stddef.h>

#include <payload/cmd_capabilities.h>
#include <payload/cmd_moveto.h>
#include <payload/cmd_spec_acq.h>
#include <payload/cmd_spec_data.h>


#define DEFAULT_PORT 1420


/**
 * service commands
 */

#define CMD_INVALID_PKT		0xa001	/* invalid packet signal */
#define CMD_CAPABILITIES	0xa002	/* capabilities of the telescope */
#define CMD_STATIONNAME		0xa003	/* name of station */
#define CMD_LOCATION		0xa004	/* geographical location of telescope */
#define CMD_MOVETO_AZEL		0xa005	/* move to azimuth/elevation */
#define CMD_SUCCESS		0xa006  /* last command succeded */
#define CMD_FAIL		0xa007	/* last command failed */
#define CMD_RECAL_POINTING	0xa008	/* recalibrate telescope pointing */
#define CMD_PARK_TELESCOPE	0xa009	/* park telescope */
#define CMD_SPEC_ACQ_START	0xa00a	/* start spectrum acquisition */
#define CMD_SPEC_DATA		0xa00b	/* spectral data */



/**
 * @brief command exchange packet structure
 */

struct packet {
	uint16_t service;
	uint16_t data_crc16;
	uint32_t data_size;
	char data[];
};

/* arbitrarily limit maximum payload size to 32 MiB */
#define MAX_PAYLOAD_SIZE	0x2000000UL
#define MAX_PACKET_SIZE (sizeof(struct packet) + MAX_PAYLOAD_SIZE)


void pkt_hdr_to_net_order(struct packet *pkt);
void pkt_hdr_to_host_order(struct packet *pkt);

void pkt_set_data_crc16(struct packet *pkt);

uint16_t CRC16(unsigned char *buf, size_t size);



#endif /* _INCLUDE_PROTOCOL_H_ */
