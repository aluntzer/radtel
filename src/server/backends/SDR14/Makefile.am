confdir = radtel
plugdir = radtel

plugindir     = $(libdir)/$(plugdir)/ADC
pluginconfdir = $(sysconfdir)/$(confdir)/backends

pluginconf_DATA = $(top_builddir)/src/server/config/backends/sdr14.cfg

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

sdr14_la_LIBADD := -L$(top_builddir)/src/net/ -lproto
sdr14_la_LIBADD += -L$(top_builddir)/src/net/ -lutil
sdr14_la_LIBADD +=  -lfftw3

plugin_LTLIBRARIES = sdr14.la

sdr14_la_SOURCES = sdr14.c

