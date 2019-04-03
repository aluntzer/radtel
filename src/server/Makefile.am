SUBDIRS = ../util ../net backends api

#paths set at make time
confdir = ${sysconfdir}/radtel/
plugdir = $(libdir)/radtel/

AM_CPPFLAGS := \-DCONFDIR=\"$(confdir)\" \
	       \-DPLUGDIR=\"$(plugdir)\"

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



# cfg to /etc
sysconf_radteldir = $(confdir)
sysconf_radtel_DATA = config/server.cfg

sysconf_radtel_backendsdir = $(sysconfdir)/radtel/backends
sysconf_radtel_backends_DATA = config/backends/srt_spectrometer.cfg \
			       config/backends/srt_drive.cfg \
			       config/backends/srt_comlink.cfg

# cal to /etc
sysconf_radtel_calibrationdir = $(sysconfdir)/radtel/backends/calibration
sysconf_radtel_calibration_DATA = config/backends/calibration/spectral_response.dat

# plugins to /lib
lib_radtelplug_srtdir = $(plugdir)/SRT
lib_radtelplug_srt_DATA = backends/SRT/srt_comlink.so \
			  backends/SRT/srt_drive.so \
			  backends/SRT/srt_spectrometer.so


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
		    proc/proc_pr_spec_acq_cfg_get.c

radtelsrv_LDADD = $(server_LIBS) \
		  $(top_builddir)/src/net/libproto.a \
		  $(top_builddir)/src/server/api/libbackend.a
