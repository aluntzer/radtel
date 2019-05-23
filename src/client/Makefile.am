SUBDIRS = ../util ../net

#paths set at make time
confdir = etc/radtel/

AM_CPPFLAGS := \-DCONFDIR=\"$(confdir)\"
AM_CPPFLAGS += -I$(top_srcdir)/src/include -Iinclude
AM_CPPFLAGS += -Iinclude
AM_CPPFLAGS += -Iwidgets/include
AM_CPPFLAGS += -Iwidgets/radio/
AM_CPPFLAGS += -Iwidgets/telescope/
AM_CPPFLAGS += -Iwidgets/sswdnd/
AM_CPPFLAGS += -Iwidgets/spectrum
AM_CPPFLAGS += -Iwidgets/history
AM_CPPFLAGS += -Iwidgets/sys_status
AM_CPPFLAGS += -Iwidgets/obs_assist
AM_CPPFLAGS += -Iwidgets/sky
AM_CPPFLAGS += -Iwidgets/chatlog

AM_CFLAGS := $(GTK3_CFLAGS) -Wunused
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(GIO_CFLAGS)


radtel_LDFLAGS = -lm $(radtel_LIBS) 

radtel_LDADD := $(GTK3_LIBS)
radtel_LDADD += $(GTHREAD_LIBS)
radtel_LDADD += $(GLIB_LIBS)
radtel_LDADD += $(GIO_LIBS)
radtel_LDADD += $(top_builddir)/src/net/libproto.a
radtel_LDADD += $(top_builddir)/src/util/libutil.a


if OS_WINDOWS
radtel_LDFLAGS += -mwindows

radtel-res.o: data/radtel.rc
	windres -i data/radtel.rc -o radtel-res.o -O coff
        radtel_LDADD += radtel-res.o
endif


sysconf_radteldir = $(confdir)
sysconf_radtel_DATA = config/sky_objects.cfg

data_desktopdir = $(datadir)/applications
data_desktop_DATA = data/org.uvie.radtel.desktop
EXTRA_DIST = $(data_desktop_DATA)

appicondir=$(datadir)/icons/hicolor/scalable/apps
appicon_DATA=data/org.uvie.radtel.svg


gsettings_SCHEMAS = data/org.uvie.radtel.config.gschema.xml
EXTRA_DIST += $(gsettings_SCHEMAS)

@GSETTINGS_RULES@




bin_PROGRAMS = radtel

radtel_SOURCES = main.c \
		 net.c \
		 gui.c \
		 widgets/xyplot/xyplot.c \
		 widgets/sky/sky.c \
		 widgets/util/desclabel.c \
		 widgets/util/default_grid.c

radtel_SOURCES += pkt_proc.c \
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
		  proc/proc_pr_moveto_azel.c \
		  proc/proc_pr_nopriv.c \
		  proc/proc_pr_message.c \
		  proc/proc_pr_userlist.c


radtel_SOURCES += sig/sig_pr_success.c \
		  sig/sig_pr_fail.c \
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
		  sig/sig_pr_nopriv.c \
		  sig/sig_pr_message.c \
		  sig/sig_pr_userlist.c \
		  sig/sig_status_push.c \
		  sig/sig_tracking.c \
		  sig/sig_shutdown.c \
		  sig/sig_connected.c \
		  sig/signals.c

radtel_SOURCES += widgets/radio/radio.c \
		  widgets/radio/radio_update.c \
		  widgets/radio/radio_acq_freq_range_ctrl.c \
		  widgets/radio/radio_acq_res_ctrl.c \
		  widgets/radio/radio_spec_avg_ctrl.c \
		  widgets/radio/radio_spec_acq_ctrl.c \
		  widgets/radio/radio_spec_acq_num_ctrl.c \
		  widgets/radio/radio_spec_cfg_ctrl.c \
		  widgets/radio/radio_vrest_ctrl.c

radtel_SOURCES += widgets/telescope/telescope.c \
		  widgets/telescope/telescope_update.c \
		  widgets/telescope/telescope_coord_ctrl.c \
		  widgets/telescope/telescope_pos_ctrl.c

radtel_SOURCES += widgets/sswdnd/sswdnd.c

radtel_SOURCES += widgets/spectrum/spectrum.c

radtel_SOURCES += widgets/history/history.c

radtel_SOURCES += widgets/sys_status/sys_status.c \
		  widgets/sys_status/sys_status_position.c \
		  widgets/sys_status/sys_status_info_bar.c

radtel_SOURCES += widgets/obs_assist/obs_assist.c \
		  widgets/obs_assist/obs_assist_cross.c \
		  widgets/obs_assist/obs_assist_azel.c \
		  widgets/obs_assist/obs_assist_npoint.c \
		  widgets/obs_assist/obs_assist_bswitch.c \
		  widgets/obs_assist/obs_assist_spectral_axis.c \
		  widgets/obs_assist/obs_assist_gal_plane.c


radtel_SOURCES += widgets/chatlog/chatlog.c



