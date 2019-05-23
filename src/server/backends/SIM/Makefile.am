
confdir = radtel
plugdir = radtel

plugindir     = $(libdir)/$(plugdir)/SIM
pluginconfdir = $(sysconfdir)/$(confdir)/backends

pluginconf_DATA = $(top_builddir)/src/server/config/backends/rt_sim.cfg


AM_CPPFLAGS := \-DCONFDIR=\"$(confdir)\" \
	       \-DSYSCONFDIR=\"$(sysconfdir)\"


AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(GTK3_CFLAGS)
AM_CFLAGS += $(GIO_CFLAGS)
AM_CFLAGS += -I$(top_srcdir)/src/include
AM_CFLAGS += -I$(top_srcdir)/src/server/include
AM_CFLAGS += -Wunused
AM_CFLAGS += -fopenmp


AM_LDFLAGS := -avoid-version
AM_LDFLAGS += -module
AM_LDFLAGS += -shared
AM_LDFLAGS += -export-dynamic


rt_sim_la_LIBADD := -L$(top_builddir)/src/net/ -lproto
rt_sim_la_LIBADD += -L$(top_builddir)/src/util -lutil


plugin_LTLIBRARIES = rt_sim.la

rt_sim_la_SOURCES = rt_sim.c

