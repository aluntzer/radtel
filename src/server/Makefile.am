SUBDIRS = ../util ../net api backends


confdir = radtel
plugdir = radtel

AM_CPPFLAGS := \-DCONFDIR=\"$(confdir)\" \
	       \-DPLUGDIR=\"$(plugdir)\" \
	       \-DSYSCONFDIR=\"$(sysconfdir)\" \
	       \-DLIBDIR=\"$(libdir)\"

AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(GTK3_CFLAGS)
AM_CFLAGS += $(GIO_CFLAGS)
AM_CFLAGS += -I$(top_srcdir)/src/include
AM_CFLAGS += -Iinclude
AM_CFLAGS += -Wunused

radtelsrv_LDADD := $(GMODULE_LIBS)
radtelsrv_LDADD += $(GTHREAD_LIBS)
radtelsrv_LDADD += $(GLIB_LIBS)
radtelsrv_LDADD += $(GTK3_LIBS)
radtelsrv_LDADD += $(GIO_LIBS)
radtelsrv_LDADD += -L$(top_builddir)/src/net/ -lproto
radtelsrv_LDADD += -L$(top_builddir)/src/util -lutil
radtelsrv_LDADD += -L$(top_builddir)/src/server/api -lbackend

radtelsrv_LDFLAGS := $(radtel_LIBS)
radtelsrv_LDFLAGS += -lm

if OS_WINDOWS
radtelsrv_LDFLAGS += -Wl,--out-implib,libhost.a
radtelsrv_LDFLAGS += -Wl,--export-all-symbols
endif





if OS_WINDOWS
radtelsrv_LDFLAGS += -mwindows

radtel-res.o: data/radtelsim.rc
	windres -i data/radtelsim.rc -o radtel-res.o -O coff
        radtelsrv_LDADD += radtel-res.o
endif


bin_PROGRAMS = radtelsrv

radtelsrv_SOURCES = main.c \
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
		    proc/proc_pr_spec_acq_cfg_get.c \
		    proc/proc_pr_control.c \
		    proc/proc_pr_message.c \
		    proc/proc_pr_nick.c

# cfg to /etc
sysconf_radteldir = $(sysconfdir)/$(confdir)
sysconf_radtel_DATA = config/server.cfg

#sysconf_radtel_backendsdir = $(sysconfdir)/$(confdir)/radtel/backends
#sysconf_radtel_backends_DATA = config/backends/srt_spectrometer.cfg \
#			       config/backends/srt_drive.cfg \
#			       config/backends/srt_comlink.cfg \
#			       config/backends/rt_sim.cfg

# cal to /etc
sysconf_radtel_calibrationdir = $(sysconfdir)/$(confdir)/backends/calibration
sysconf_radtel_calibration_DATA = config/backends/calibration/spectral_response.dat

if !OS_WINDOWS
# plugins to /lib
#lib_radtelplug_srtdir = $(libdir)/$(plugdir)/SRT
#lib_radtelplug_srt_DATA = backends/SRT/srt_comlink.so \
#			  backends/SRT/srt_drive.so \
#			  backends/SRT/srt_spectrometer.so
endif

data_desktopdir = $(datadir)/applications
data_desktop_DATA = data/org.uvie.radtelsim.desktop
EXTRA_DIST = $(data_desktop_DATA)

appicondir=$(datadir)/icons/hicolor/scalable/apps
appicon_DATA=data/org.uvie.radtelsim.svg


