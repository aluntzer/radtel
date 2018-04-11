SUBDIRS = ../util ../net
AM_CPPFLAGS  = -I$(top_srcdir)/src/include -Iinclude -Iwidgets/include
AM_CFLAGS = $(client_CFLAGS)

bin_PROGRAMS = client 

client_SOURCES = main.c \
		 net.c \
		 gui.c \
		 cmd_proc.c \
		 proc/proc_cmd_capabilities.c \
		 proc/proc_cmd_invalid_pkt.c \
		 proc/proc_cmd_fail.c \
		 proc/proc_cmd_success.c \
		 proc/proc_cmd_spec_data.c \
		 sig/sig_cmd_success.c \
		 sig/sig_cmd_capabilities.c \
		 sig/sig_cmd_spec_data.c \
		 sig/signals.c \
		 widgets/xyplot/xyplot.c \
		 widgets/sky/sky.c \
		 widgets/radio/radio.c \
		 widgets/util/desclabel.c \
		 widgets/util/default_grid.c

client_LDADD = -lm $(client_LIBS)
client_LDADD += $(top_builddir)/src/net/libprotocmd.a
client_LDADD += $(top_builddir)/src/util/libutil.a
