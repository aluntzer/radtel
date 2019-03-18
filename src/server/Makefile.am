SUBDIRS = ../util ../net backends api

AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(GIO_CFLAGS)
AM_CFLAGS += -I$(top_srcdir)/src/include -Iinclude

AM_LDFLAGS := $(GMODULE_LIBS)
AM_LDFLAGS += $(GTHREAD_LIBS)
AM_LDFLAGS += $(GLIB_LIBS)
AM_LDFLAGS += $(GIO_LIBS)
AM_LDFLAGS += -lm
# force symbols from the .a that we absolutely need (and are unused otherwise)
# it's a bit hackish, but who cares...
AM_LDFLAGS += -u ack_spec_data
AM_LDFLAGS += -u ack_spec_acq_disable
AM_LDFLAGS += -u ack_spec_acq_enable
AM_LDFLAGS += -u ack_status_acq
AM_LDFLAGS += -u ack_status_slew
AM_LDFLAGS += -u ack_status_move
AM_LDFLAGS += -u ack_status_rec
AM_LDFLAGS += -u ack_moveto_azel

bin_PROGRAMS = server

server_SOURCES = main.c \
		 cfg.c \
		 backend.c \
		 net.c \
		 pkt_proc.c \
		 proc/proc_pr_capabilities.c \
		 proc/proc_pr_invalid_pkt.c \
		 proc/proc_pr_moveto_azel.c \
		 proc/proc_pr_recalibrate_pointing.c \
		 proc/proc_pr_park_telescope.c \
		 proc/proc_pr_spec_acq_cfg.c \
		 proc/proc_pr_getpos_azel.c \
		 proc/proc_pr_spec_acq_enable.c \
		 proc/proc_pr_spec_acq_disable.c \
		 proc/proc_pr_spec_acq_cfg_get.c

server_LDADD = $(server_LIBS) \
	       $(top_builddir)/src/net/libproto.a \
	       $(top_builddir)/src/server/api/libbackend.a
