/**
 * @file    net/cmds/cmd_spec_acq_cfg.c
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
 */

#include <glib.h>

#include <cmd.h>


/**
 * @brief command spectrum acquisition
 *
 * @param f0 lower bound frequency in Hz
 * @param f1 upper bound frequency in Hz
 * @param bw_div bandwidth divider
 * @param bin_div bins per bandwidth divider
 * @param n_stack number of acquired spectrae to stack on server (0 == 1)
 * @param acq_max maximum number of stacked spectrae to acquire (0 == infinite)
 *
 */

struct packet *cmd_spec_acq_cfg_gen(uint16_t trans_id,
				    uint64_t f0, uint64_t f1, uint32_t bw_div,
				    uint32_t bin_div, uint32_t n_stack,
				    uint32_t acq_max)
{
	gsize pkt_size;

	struct packet *pkt;

	struct spec_acq_cfg *acq;


	pkt_size = sizeof(struct packet) + sizeof(struct spec_acq_cfg);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_SPEC_ACQ_CFG;
	pkt->trans_id  = trans_id;
	pkt->data_size = sizeof(struct spec_acq_cfg);


	acq = (struct spec_acq_cfg *) pkt->data;


	acq->freq_start_hz = f0;
	acq->freq_stop_hz  = f1;
	acq->bw_div	   = bw_div;
	acq->bin_div       = bin_div;
	acq->n_stack       = n_stack;
	acq->acq_max       = acq_max;

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	return pkt;
}


void cmd_spec_acq_cfg(uint16_t trans_id,
		      uint64_t f0, uint64_t f1, uint32_t bw_div,
		      uint32_t bin_div, uint32_t n_stack, uint32_t acq_max)
{
	struct packet *pkt;
	struct spec_acq_cfg *acq;

	pkt = cmd_spec_acq_cfg_gen(trans_id, f0, f1, bw_div,
				   bin_div, n_stack, acq_max);

	acq = (struct spec_acq_cfg *) pkt->data;

	g_debug("Sending command acquire spectrum "
		  "FREQ range: %g - %g MHz, BW div: %d, BIN div %d,"
		  "STACK: %d, ACQ %d",
		  acq->freq_start_hz / 1e6,
		  acq->freq_stop_hz / 1e6,
		  acq->bw_div,
		  acq->bin_div,
		  acq->n_stack,
		  acq->acq_max);


	net_send((void *) pkt, pkt_size_get(pkt));

	/* clean up */
	g_free(pkt);
}
