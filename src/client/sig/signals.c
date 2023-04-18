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
 *  @brief internal signal to notify all components of a shutdown
 */

static void setup_sig_shutdown(void)
{
	g_signal_new("shutdown",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}


/**
 *  @brief internal signal to notify all components of an established connection
 */

static void setup_sig_connected(void)
{
	g_signal_new("net-connected",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}


/**
 *  @brief internal signal to control position tracking
 */

static void setup_sig_tracking(void)
{
	g_signal_new("tracking",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 3,
		     G_TYPE_BOOLEAN, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}


/**
 *  @brief internal signal to pass status bar notifications
 */

static void setup_sig_status_push(void)
{
	g_signal_new("status-push",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_STRING);
}


static void setup_sig_pr_success(void)
{
	g_signal_new("pr-success",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}

static void setup_sig_pr_fail(void)
{
	g_signal_new("pr-fail",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void setup_sig_pr_capabilities(void)
{
	g_signal_new("pr-capabilities",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_capabilities_load(void)
{
	g_signal_new("pr-capabilities-load",
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

static void setup_sig_pr_status_move(void)
{
	g_signal_new("pr-status-move",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_status_rec(void)
{
	g_signal_new("pr-status-rec",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_moveto_azel(void)
{
	g_signal_new("pr-moveto-azel",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
}

static void setup_sig_pr_nopriv(void)
{
	g_signal_new("pr-nopriv",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void setup_sig_pr_message(void)
{
	g_signal_new("pr-message",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_userlist(void)
{
	g_signal_new("pr-userlist",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void setup_sig_pr_hot_load_enable(void)
{
	g_signal_new("pr-hot-load-enable",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}

static void setup_sig_pr_hot_load_disable(void)
{
	g_signal_new("pr-hot-load-disable",
		     G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST,
		     0, NULL, NULL, NULL,
		     G_TYPE_NONE, 0);
}

static void setup_sig_pr_video_uri(void)
{
	g_signal_new("pr-video-uri",
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


	setup_sig_shutdown();
	setup_sig_connected();
	setup_sig_tracking();
	setup_sig_status_push();

	setup_sig_pr_success();
	setup_sig_pr_fail();
	setup_sig_pr_capabilities();
	setup_sig_pr_capabilities_load();
	setup_sig_pr_spec_data();
	setup_sig_pr_getpos_azel();
	setup_sig_pr_spec_acq_enable();
	setup_sig_pr_spec_acq_disable();
	setup_sig_pr_spec_acq_cfg();
	setup_sig_pr_status_acq();
	setup_sig_pr_status_slew();
	setup_sig_pr_status_move();
	setup_sig_pr_status_rec();
	setup_sig_pr_moveto_azel();
	setup_sig_pr_nopriv();
	setup_sig_pr_message();
	setup_sig_pr_userlist();
	setup_sig_pr_hot_load_enable();
	setup_sig_pr_hot_load_disable();
	setup_sig_pr_video_uri();
}
