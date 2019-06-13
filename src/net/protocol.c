/**
 * @file    protocol.c
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
 * @brief networking protocol utilities
 */


#include <glib.h>
#include <protocol.h>



/**
 * @brief get the total size of a packet in bytes
 */

size_t pkt_size_get(struct packet *pkt)
{
	if (!pkt)
		return 0;

	return sizeof(struct packet) + g_htonl(pkt->data_size);
}


/**
 * @brief convert packet header to network byte order
 */

void pkt_hdr_to_net_order(struct packet *pkt)
{
	pkt->service    = g_htons(pkt->service);
	pkt->data_size  = g_htonl(pkt->data_size);
	pkt->data_crc16 = g_htons(pkt->data_crc16);
}


/**
 * @brief convert packet header to host byte order
 */

void pkt_hdr_to_host_order(struct packet *pkt)
{
	pkt->service    = g_ntohs(pkt->service);
	pkt->data_size  = g_ntohl(pkt->data_size);
	pkt->data_crc16 = g_ntohs(pkt->data_crc16);
}

/**
 * @brief set the data CRC16 of a packet
 */

void pkt_set_data_crc16(struct packet *pkt)
{
	pkt->data_crc16 = CRC16(pkt->data, pkt->data_size);
}


/**
 * @brief calculate a CRC16 of a buffer
 * @returns CRC16 or 0xffff on error or zero-length buffers
 */

uint16_t CRC16(unsigned char *buf, size_t size)
{
	size_t i;
	size_t j;

	char b;

	uint16_t S = 0xffff;


	if (!buf)
		return S;


	for (i = 0; i < size; i++) {
		b = buf[i];

		for (j = 0; j < 8; j++) {

			if ((b & 0x80) ^ ((S & 0x8000) >> 8))
				S = ((S << 1) ^ 0x1021) & 0xFFFF;
			else
				S = (S << 1) & 0xFFFF;

			b = b << 1;
		}
	}

	return S;
}
