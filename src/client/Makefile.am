SUBDIRS = ../util ../net
AM_CPPFLAGS  = -I$(top_srcdir)/src/include
AM_CPPFLAGS += -Iinclude
AM_CPPFLAGS += -Iwidgets/include
AM_CPPFLAGS += -Iwidgets/radio/
AM_CPPFLAGS += -Iwidgets/telescope/
AM_CFLAGS = $(client_CFLAGS)

bin_PROGRAMS = client

client_SOURCES = main.c \
		 net.c \
		 gui.c \
		 widgets/xyplot/xyplot.c \
		 widgets/sky/sky.c \
		 widgets/util/desclabel.c \
		 widgets/util/default_grid.c

client_SOURCES += pkt_proc.c \
		  proc/proc_pr_capabilities.c \
		  proc/proc_pr_invalid_pkt.c \
		  proc/proc_pr_fail.c \
		  proc/proc_pr_success.c \
		  proc/proc_pr_spec_data.c \
		  proc/proc_pr_getpos_azel.c \
		  proc/proc_pr_spec_acq_enable.c \
		  proc/proc_pr_spec_acq_disable.c \
		  proc/proc_pr_spec_acq_cfg.c


client_SOURCES += sig/sig_pr_success.c \
		  sig/sig_pr_capabilities.c \
		  sig/sig_pr_spec_data.c \
		  sig/sig_pr_getpos_azel.c \
		  sig/sig_pr_spec_acq_enable.c \
		  sig/sig_pr_spec_acq_disable.c \
		  sig/sig_pr_spec_acq_cfg.c \
		  sig/signals.c

client_SOURCES += widgets/radio/radio.c \
		  widgets/radio/radio_update.c \
		  widgets/radio/radio_acq_freq_range_ctrl.c \
		  widgets/radio/radio_acq_res_ctrl.c \
		  widgets/radio/radio_spec_avg_ctrl.c \
		  widgets/radio/radio_spec_acq_ctrl.c \
		  widgets/radio/radio_spec_acq_num_ctrl.c \
		  widgets/radio/radio_spec_cfg_ctrl.c \
		  widgets/radio/radio_vrest_ctrl.c

client_SOURCES += widgets/telescope/telescope.c \
		  widgets/telescope/telescope_update.c \
		  widgets/telescope/telescope_coord_ctrl.c \
		  widgets/telescope/telescope_pos_ctrl.c





client_LDADD = -lm $(client_LIBS)
client_LDADD += $(top_builddir)/src/net/libproto.a
client_LDADD += $(top_builddir)/src/util/libutil.a
