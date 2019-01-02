AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS = $(server_CFLAGS)

noinst_LIBRARIES = libprotocmd.a

libprotocmd_a_SOURCES = protocol.c \
			cmds/cmd_invalid_pkt.c \
			cmds/cmd_capabilities.c \
			cmds/cmd_success.c \
			cmds/cmd_fail.c \
			cmds/cmd_recalibrate_pointing.c \
			cmds/cmd_park_telescope.c \
			cmds/cmd_moveto_azel.c \
			cmds/cmd_spec_acq_cfg.c \
			cmds/cmd_spec_data.c \
			cmds/cmd_getpos_azel.c \
			acks/ack_capabilities.c \
			acks/ack_getpos_azel.c
