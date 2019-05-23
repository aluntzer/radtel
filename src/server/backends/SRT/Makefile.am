confdir = radtel
plugdir = radtel

plugindir     = $(libdir)/$(plugdir)/SRT
pluginconfdir = $(sysconfdir)/$(confdir)/backends

pluginconf_DATA = $(top_builddir)/src/server/config/backends/srt_spectrometer.cfg \
		  $(top_builddir)/src/server/config/backends/srt_drive.cfg \
		  $(top_builddir)/src/server/config/backends/srt_comlink.cfg

AM_CPPFLAGS := \-DCONFDIR=\"$(confdir)\" \
	       \-DSYSCONFDIR=\"$(sysconfdir)\"


AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += -I$(top_srcdir)/src/include
AM_CFLAGS += -I$(top_srcdir)/src/server/include
AM_CFLAGS += -Wunused


AM_LDFLAGS := -avoid-version
AM_LDFLAGS += -module
AM_LDFLAGS += -shared
AM_LDFLAGS += -export-dynamic


srt_drive_la_LIBADD := -L$(top_builddir)/src/net/ -lproto
srt_drive_la_LIBADD += -L$(top_builddir)/src/net/ -lutil

srt_spectrometer_la_LIBADD := -L$(top_builddir)/src/net/ -lproto
srt_spectrometer_la_LIBADD += -L$(top_builddir)/src/net/ -lutil


plugin_LTLIBRARIES = srt_drive.la srt_comlink.la srt_spectrometer.la

srt_drive_la_SOURCES = srt_drive.c

srt_comlink_la_SOURCES = srt_comlink.c

srt_spectrometer_la_SOURCES = srt_spectrometer.c

