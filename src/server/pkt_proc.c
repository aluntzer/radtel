/**
 * @file    server/pkt_proc.c
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
 * @brief client command processing
 *
 */

#include <pkt_proc.h>
#include <protocol.h>

#include <gio/gio.h>
#include <glib.h>

#include <ack.h>



/**
 * @brief check if a command type is priviledged
 */

static gboolean cmd_is_priv(struct packet *pkt)
{
	switch(pkt->service) {

	case PR_MOVETO_AZEL:
	case PR_RECAL_POINTING:
	case PR_PARK_TELESCOPE:
	case PR_SPEC_ACQ_CFG:
	case PR_SPEC_ACQ_ENABLE:
	case PR_SPEC_ACQ_DISABLE:
	case PR_HOT_LOAD_ENABLE:
	case PR_HOT_LOAD_DISABLE:
		return TRUE;
	default:
		break;
	}


	return FALSE;
}


/**
 * @brief process unpriviledged commands
 */

static int process_pkt_other(struct packet *pkt, gpointer ref)
{
	switch(pkt->service) {

	case PR_INVALID_PKT:
		proc_pr_invalid_pkt(pkt, ref);
		break;

	case PR_CAPABILITIES:
		proc_pr_capabilities(pkt);
		break;

	case PR_CAPABILITIES_LOAD:
		proc_pr_capabilities_load(pkt);
		break;

	case PR_GETPOS_AZEL:
		proc_pr_getpos_azel(pkt, ref);
		break;

	case PR_SPEC_ACQ_CFG_GET:
		proc_pr_spec_acq_cfg_get(pkt, ref);
		break;

	case PR_CONTROL:
		proc_pr_control(pkt, ref);
		break;

	case PR_MESSAGE:
		proc_pr_message(pkt, ref);
		break;

	case PR_NICK:
		proc_pr_nick(pkt, ref);
		break;

	default:

		if (!cmd_is_priv(pkt)) {
			g_message("Service command %x not understood\n",
				  pkt->service);
			/* XXX need ack: PR_INVALID_SERVICE */
			ack_fail(pkt->trans_id, ref);
			return -1;
		}

		ack_nopriv(pkt->trans_id, ref);
		break;
	}


	g_free(pkt);

	return 0;
}


/**
 * @brief process priviledged commands
 */

static int process_pkt_priv(struct packet *pkt, gpointer ref)
{
	switch(pkt->service) {

	case PR_MOVETO_AZEL:
		proc_pr_moveto_azel(pkt, ref);
		break;

	case PR_RECAL_POINTING:
		proc_pr_recalibrate_pointing(pkt, ref);
		break;

	case PR_PARK_TELESCOPE:
		proc_pr_park_telescope(pkt, ref);
		break;

	case PR_SPEC_ACQ_CFG:
		proc_pr_spec_acq_cfg(pkt, ref);
		break;

	case PR_SPEC_ACQ_ENABLE:
		proc_pr_spec_acq_enable(pkt, ref);
		break;

	case PR_SPEC_ACQ_DISABLE:
		proc_pr_spec_acq_disable(pkt, ref);
		break;

	case PR_HOT_LOAD_ENABLE:
		proc_pr_hot_load_enable(pkt, ref);
		break;

	case PR_HOT_LOAD_DISABLE:
		proc_pr_hot_load_disable(pkt, ref);
		break;

	default:
		/* connection has priviledge, but command is other */
		return process_pkt_other(pkt, ref);
	}


	g_free(pkt);

	return 0;
}


/**
 * @brief process a command pkt
 * @returns -1 on error
 */

int process_pkt(struct packet *pkt, gboolean priv, gpointer ref)
{
	int ret = 0;


	if (priv)
		ret = process_pkt_priv(pkt, ref);
	else
		ret = process_pkt_other(pkt, ref);

	return ret;
}
