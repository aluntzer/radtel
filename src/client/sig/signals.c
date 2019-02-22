/**
 * @file    client/sig/signals.c
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
#include <glib-object.h>

#include <protocol.h>

static gpointer *sig_server;


/**
 * special internal signal to notify of any updates
 */

static void setup_sig_update(void)
{
	g_signal_new("update",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}


/**
 * special internal signal to notify all compnents of a shutdown
 */

static void setup_sig_shutdown(void)
{
	g_signal_new("shutdown",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}


static void setup_sig_pr_success(void)
{
	g_signal_new("pr-success",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}


static void setup_sig_pr_capabilities(void)
{
	g_signal_new("pr-capabilities",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_spec_data(void)
{
	g_signal_new("pr-spec-data",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_getpos_azel(void)
{
	g_signal_new("pr-getpos-azel",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_spec_acq_enable(void)
{
	g_signal_new("pr-spec-acq-enable",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}

static void setup_sig_pr_spec_acq_disable(void)
{
	g_signal_new("pr-spec-acq-disable",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}

static void setup_sig_pr_spec_acq_cfg(void)
{
	g_signal_new("pr-spec-acq-cfg",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_status_acq(void)
{
	g_signal_new("pr-status-acq",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_status_slew(void)
{
	g_signal_new("pr-status-slew",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}


gpointer *sig_get_instance(void)
{
	return sig_server;
}


void sig_init(void)
{
	sig_server = g_object_new(G_TYPE_OBJECT, NULL);

#if 0
	setup_sig_update();
#endif
	setup_sig_shutdown();

	setup_sig_pr_success();
	setup_sig_pr_capabilities();
	setup_sig_pr_spec_data();
	setup_sig_pr_getpos_azel();
	setup_sig_pr_spec_acq_enable();
	setup_sig_pr_spec_acq_disable();
	setup_sig_pr_spec_acq_cfg();
	setup_sig_pr_status_acq();
	setup_sig_pr_status_slew();
}
