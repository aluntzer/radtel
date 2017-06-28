AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS = $(server_CFLAGS)

noinst_LIBRARIES = libprotocmd.a

libprotocmd_a_SOURCES = protocol.c \
			cmds/cmd_invalid_pkt.c \
			cmds/cmd_capabilities.c \
			cmds/cmd_success.c \
			cmds/cmd_fail.c \
			cmds/cmd_moveto_azel.c \
			acks/ack_capabilities.c
