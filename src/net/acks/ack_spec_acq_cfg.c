/**
 * @file    net/acks/ack_spec_acq_cfg.c
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

#include <ack.h>


struct packet *ack_spec_acq_cfg_gen(uint16_t trans_id, struct spec_acq_cfg *acq)
{
	gsize pkt_size;

	struct packet *pkt;


	pkt_size = sizeof(struct packet) + sizeof(struct spec_acq_cfg);

	/* allocate zeroed packet + payload */
	pkt = g_malloc0(pkt_size);

	pkt->service   = PR_SPEC_ACQ_CFG;
	pkt->trans_id  = trans_id;
	pkt->data_size = sizeof(struct spec_acq_cfg);


	memcpy(pkt->data, acq, sizeof(struct spec_acq_cfg));

	pkt_set_data_crc16(pkt);

	pkt_hdr_to_net_order(pkt);

	return pkt;
}


void ack_spec_acq_cfg(uint16_t trans_id, struct spec_acq_cfg *acq)
{
	struct packet *pkt;


	pkt = ack_spec_acq_cfg_gen(trans_id, acq);

	g_debug("Sending current spectrometer configuration "
		  "FREQ range: %g - %g MHz, BW div: %d, BIN div %d,"
		  "STACK: %d, ACQ %d",
		  acq->freq_start_hz / 1e6,
		  acq->freq_stop_hz / 1e6,
		  acq->bw_div,
		  acq->bin_div,
		  acq->n_stack,
		  acq->acq_max);


	net_send((void *) pkt, pkt_size_get(pkt));

	g_free(pkt);
}
