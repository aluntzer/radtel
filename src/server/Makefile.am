SUBDIRS = backends api
AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS = $(server_CFLAGS)

AM_LDFLAGS = -lm

bin_PROGRAMS = server 

server_SOURCES = main.c \
		 cfg.c \
		 backend.c \
		 net.c \
		 cmd_proc.c \
		 proc/proc_cmd_capabilities.c \
		 proc/proc_cmd_invalid_pkt.c \
		 proc/proc_cmd_moveto_azel.c \
		 proc/proc_cmd_recalibrate_pointing.c \
		 proc/proc_cmd_park_telescope.c \
		 proc/proc_cmd_spec_acq_start.c

server_LDADD = $(server_LIBS) \
	       $(top_builddir)/src/net/libprotocmd.a \
	       $(top_builddir)/src/server/api/libbackend.a
