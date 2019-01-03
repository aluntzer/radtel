SUBDIRS = ../util ../net
AM_CPPFLAGS  = -I$(top_srcdir)/src/include -Iinclude -Iwidgets/include
AM_CFLAGS = $(client_CFLAGS)

bin_PROGRAMS = client 

client_SOURCES = main.c \
		 net.c \
		 gui.c \
		 pkt_proc.c \
		 proc/proc_pr_capabilities.c \
		 proc/proc_pr_invalid_pkt.c \
		 proc/proc_pr_fail.c \
		 proc/proc_pr_success.c \
		 proc/proc_pr_spec_data.c \
		 proc/proc_pr_getpos_azel.c \
		 proc/proc_pr_spec_acq_enable.c \
		 proc/proc_pr_spec_acq_disable.c \
		 proc/proc_pr_spec_acq_cfg.c \
		 sig/sig_pr_success.c \
		 sig/sig_pr_capabilities.c \
		 sig/sig_pr_spec_data.c \
		 sig/sig_pr_getpos_azel.c \
		 sig/sig_pr_spec_acq_enable.c \
		 sig/sig_pr_spec_acq_disable.c \
		 sig/sig_pr_spec_acq_cfg.c \
		 sig/signals.c \
		 widgets/xyplot/xyplot.c \
		 widgets/sky/sky.c \
		 widgets/radio/radio.c \
		 widgets/util/desclabel.c \
		 widgets/util/default_grid.c

client_LDADD = -lm $(client_LIBS)
client_LDADD += $(top_builddir)/src/net/libproto.a
client_LDADD += $(top_builddir)/src/util/libutil.a
