AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude -Iwidgets/xyplot
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
		 sig/sig_cmd_success.c \
		 sig/sig_cmd_capabilities.c \
		 sig/signals.c \
		 widgets/xyplot/xyplot.c

client_LDADD = -lm $(client_LIBS) $(top_builddir)/src/net/libprotocmd.a
