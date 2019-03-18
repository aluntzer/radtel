AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)

noinst_LIBRARIES = libproto.a

libproto_a_SOURCES = protocol.c \
		     cmds/cmd_invalid_pkt.c \
		     cmds/cmd_capabilities.c \
		     cmds/cmd_success.c \
		     cmds/cmd_fail.c \
		     cmds/cmd_recalibrate_pointing.c \
		     cmds/cmd_park_telescope.c \
		     cmds/cmd_moveto_azel.c \
		     cmds/cmd_spec_acq_cfg.c \
		     cmds/cmd_getpos_azel.c \
		     cmds/cmd_spec_acq_enable.c \
		     cmds/cmd_spec_acq_disable.c \
		     cmds/cmd_spec_acq_cfg_get.c \
		     acks/ack_capabilities.c \
		     acks/ack_getpos_azel.c \
		     acks/ack_spec_data.c \
		     acks/ack_spec_acq_enable.c \
		     acks/ack_spec_acq_disable.c \
		     acks/ack_fail.c \
		     acks/ack_invalid_pkt.c \
		     acks/ack_success.c \
		     acks/ack_spec_acq_cfg.c \
		     acks/ack_status_acq.c \
		     acks/ack_status_slew.c \
		     acks/ack_status_move.c \
		     acks/ack_status_rec.c \
		     acks/ack_moveto_azel.c

