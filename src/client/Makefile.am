SUBDIRS = ../util ../net
AM_CPPFLAGS := -I$(top_srcdir)/src/include -Iinclude
AM_CPPFLAGS += -Iinclude
AM_CPPFLAGS += -Iwidgets/include
AM_CPPFLAGS += -Iwidgets/radio/
AM_CPPFLAGS += -Iwidgets/telescope/
AM_CPPFLAGS += -Iwidgets/sswdnd/
AM_CPPFLAGS += -Iwidgets/spectrum
AM_CPPFLAGS += -Iwidgets/sys_status
AM_CPPFLAGS += -Iwidgets/obs_assist
AM_CPPFLAGS += -Iwidgets/sky

AM_CFLAGS := $(GTK3_CFLAGS) -Wunused
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(GIO_CFLAGS)

client_LDFLAGS = -lm $(client_LIBS)

client_LDADD := $(GTK3_LIBS)
client_LDADD += $(GTHREAD_LIBS)
client_LDADD += $(GLIB_LIBS)
client_LDADD += $(GIO_LIBS)
client_LDADD += $(top_builddir)/src/net/libproto.a
client_LDADD += $(top_builddir)/src/util/libutil.a

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
		  proc/proc_pr_spec_acq_cfg.c \
		  proc/proc_pr_status_acq.c \
		  proc/proc_pr_status_slew.c \
		  proc/proc_pr_status_move.c \
		  proc/proc_pr_status_rec.c \
		  proc/proc_pr_moveto_azel.c


client_SOURCES += sig/sig_pr_success.c \
		  sig/sig_pr_capabilities.c \
		  sig/sig_pr_spec_data.c \
		  sig/sig_pr_getpos_azel.c \
		  sig/sig_pr_spec_acq_enable.c \
		  sig/sig_pr_spec_acq_disable.c \
		  sig/sig_pr_spec_acq_cfg.c \
		  sig/sig_pr_status_acq.c \
		  sig/sig_pr_status_slew.c \
		  sig/sig_pr_status_move.c \
		  sig/sig_pr_status_rec.c \
		  sig/sig_pr_moveto_azel.c \
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

client_SOURCES += widgets/sswdnd/sswdnd.c

client_SOURCES += widgets/spectrum/spectrum.c

client_SOURCES += widgets/sys_status/sys_status.c \
		  widgets/sys_status/sys_status_position.c \
		  widgets/sys_status/sys_status_info_bar.c

client_SOURCES += widgets/obs_assist/obs_assist.c \
		  widgets/obs_assist/obs_assist_cross.c \
		  widgets/obs_assist/obs_assist_azel.c \
		  widgets/obs_assist/obs_assist_gal_plane.c



