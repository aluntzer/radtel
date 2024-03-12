confdir = radtel
plugdir = radtel

plugindir     = $(libdir)/$(plugdir)/PWR
pluginconfdir = $(sysconfdir)/$(confdir)/backends

pluginconf_DATA = $(top_builddir)/src/server/config/backends/pwr_ctrl.cfg

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

pwr_ctrl_la_LIBADD := -L$(top_builddir)/src/net/ -lproto -lutil

plugin_LTLIBRARIES = pwr_ctrl.la

if !OS_DARWIN
pwr_ctrl_la_SOURCES = pwr_ctrl.c
endif

