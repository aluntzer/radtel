confdir = radtel
plugdir = radtel

plugindir     = $(libdir)/$(plugdir)/MD01
pluginconfdir = $(sysconfdir)/$(confdir)/backends

pluginconf_DATA = $(top_builddir)/src/server/config/backends/md01_rot2prog.cfg

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

md01_rot2prog_la_LIBADD := -L$(top_builddir)/src/net/ -lproto
md01_rot2prog_la_LIBADD += -L$(top_builddir)/src/net/ -lutil

plugin_LTLIBRARIES = md01_rot2prog.la

md01_rot2prog_la_SOURCES = md01_rot2prog.c

