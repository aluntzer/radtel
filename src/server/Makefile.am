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

# force symbols from the .a that we absolutely need (and are unused otherwise)
# it's a bit hackish, but who cares...
AM_LDFLAGS := -u ack_spec_data
AM_LDFLAGS += -u ack_spec_acq_disable
AM_LDFLAGS += -u ack_spec_acq_enable
AM_LDFLAGS += -u ack_spec_acq_cfg
AM_LDFLAGS += -u ack_status_acq
AM_LDFLAGS += -u ack_status_slew
AM_LDFLAGS += -u ack_status_move
AM_LDFLAGS += -u ack_status_rec
AM_LDFLAGS += -u ack_moveto_azel
AM_LDFLAGS += -u horizontal_to_galactic
AM_LDFLAGS += -u galactic_to_equatorial
AM_LDFLAGS += -u vlsr

radtelsrv_LDFLAGS = -lm $(radtel_LIBS)

radtelsrv_LDADD := $(GMODULE_LIBS)
radtelsrv_LDADD += $(GTHREAD_LIBS)
radtelsrv_LDADD += $(GLIB_LIBS)
radtelsrv_LDADD += $(GIO_LIBS)
radtelsrv_LDADD += $(top_builddir)/src/net/libproto.a
radtelsrv_LDADD += $(top_builddir)/src/util/libutil.a
radtelsrv_LDADD += $(top_builddir)/src/server/api/libbackend.a
		  


# cfg to /etc
sysconf_radteldir = $(confdir)
sysconf_radtel_DATA = config/server.cfg

sysconf_radtel_backendsdir = $(sysconfdir)/radtel/backends
sysconf_radtel_backends_DATA = config/backends/srt_spectrometer.cfg \
			       config/backends/srt_drive.cfg \
			       config/backends/srt_comlink.cfg \
			       config/backends/rt_sim.cfg

# cal to /etc
sysconf_radtel_calibrationdir = $(sysconfdir)/radtel/backends/calibration
sysconf_radtel_calibration_DATA = config/backends/calibration/spectral_response.dat

# plugins to /lib
lib_radtelplug_srtdir = $(plugdir)/SRT
lib_radtelplug_srt_DATA = backends/SRT/srt_comlink.so \
			  backends/SRT/srt_drive.so \
			  backends/SRT/srt_spectrometer.so
# plugins to /lib
lib_radtelplug_simdir = $(plugdir)/SIM
lib_radtelplug_sim_DATA = backends/SIM/rt_sim.so




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

