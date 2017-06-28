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


#define DEFAULT_PORT 1420


struct packet {
	uint16_t service;
	uint16_t data_crc16;
	uint32_t data_size;
	char data[];
};

/* arbitrarily limit maximum payload size to 32 MiB */
#define MAX_PAYLOAD_SIZE	0x2000000UL
#define MAX_PACKET_SIZE (sizeof(struct packet) + )


void pkt_hdr_to_net_order(struct packet *pkt);
void pkt_hdr_to_host_order(struct packet *pkt);

void pkt_set_data_crc16(struct packet *pkt);

uint16_t CRC16(unsigned char *buf, size_t size);



/*
 * Digital SRT radiometer values:
 *
 * freq_min_hz		1370 0000 0000	(1370 MHz)
 * freq_max_hz		1800 0000 0000  (1800 MHz)
 * freq_inc_hz		      40  0000  (  40 kHź)
 *
 * bw_max_hz		      500 0000	(500 kHz)
 *
 * bw_max_div_lin;	             0  (unused)
 * bw_max_div_rad2		     2  (0 = 500 kHz, 1 = 250 kHz, 2 = 125 kHz)
 *
 * bw_max_bins			    64
 *
 * bw_max_bin_div_lin		     0 (unused)
 * bw_max_bin_div_rad2		     0 (always 64)
 *
 */

struct capabilities {

	uint32_t az_min_arcsec;		/* left limit      */				
	uint32_t az_max_arcsec;		/* right limit     */
	uint32_t az_res_arcsec;		/* step resolution */

	uint32_t el_min_arcsec;		/* lower limit     */
	uint32_t el_max_arcsec;		/* upper limit     */
	uint32_t el_res_arcsec;		/* step resolution */

	/* frequency limits */
	uint64_t freq_min_hz;		/* lower frequency limit */
	uint64_t freq_max_hz;		/* upper frequency limit */
	uint64_t freq_inc_hz;		/* frequency increment   */


	/* filter bandwidth limits
	 * and maxium filter bandwidth divider:
	 *	bw_max_hz/bw_max_div = bw_min_hz
	 *
	 * the filter bandwith divider may be given as linear integer limit or
	 * radix-2 exponent
	 *
	 * NOTE: a value of 0 for the LINEAR divider marks it as unused
	 */

	uint32_t bw_max_hz;		/* max resolution bandwidth */
	uint32_t bw_max_div_lin;	/* max BW divider (linear)*/
	uint32_t bw_max_div_rad2;	/* max BW divider (2^n) */


	/* number of bins per bandwidth, e.g. length of FFT
	 * and maximum divider for bins per bandwidth:
	 *	max_bins/max_div = min_bins
	 *
	 * the bin divider may be given as linear integer limit or
	 * radix-2 exponent
	 *
	 * NOTE: a value of 0 for the LINEAR divider marks it as unused
	 */

	uint32_t bw_max_bins;		/* upper number of bins per bandwidth */
	uint32_t bw_max_bin_div_lin;	/* max bin per BW divider (linear) */
	uint32_t bw_max_bin_div_rad2;	/* max bin per BW divider (2^n) */
};





#define CMD_INVALID_PKT		0xa001	/* invalid packet signal */

#define CMD_CAPABILITIES	0xa002	/* capabilities of the telescope */
#define CMD_STATIONNAME		0xa003	/* name of station */
#define CMD_LOCATION		0xa004	/* geographical location of telescope */



#endif /* _INCLUDE_PROTOCOL_H_ */
