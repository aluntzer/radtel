
confdir = radtel
plugdir = radtel

plugindir     = $(libdir)/$(plugdir)/SIM
pluginconfdir = $(sysconfdir)/$(confdir)/backends

pluginconf_DATA = $(top_builddir)/src/server/config/backends/rt_sim.cfg


AM_CPPFLAGS := -DCONFDIR=\"$(confdir)\" \
	       -DSYSCONFDIR=\"$(sysconfdir)\"


AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)
AM_CFLAGS += $(GTK3_CFLAGS)
AM_CFLAGS += $(GIO_CFLAGS)
AM_CFLAGS += -I$(top_srcdir)/src/include
AM_CFLAGS += -I$(top_srcdir)/src/server/include
AM_CFLAGS += -Wunused

if !OS_DARWIN
AM_CFLAGS += -fopenmp
endif

if !OS_WINDOWS

plugin_LTLIBRARIES = rt_sim.la
rt_sim_la_LDFLAGS := -avoid-version
rt_sim_la_LDFLAGS += -module
rt_sim_la_LDFLAGS += -shared
rt_sim_la_LDFLAGS += -export-dynamic

rt_sim_la_LIBADD := -L$(top_builddir)/src/net/  -lproto
rt_sim_la_LIBADD += -L$(top_builddir)/src/util/ -lutil
rt_sim_la_LIBADD += $(GMODULE_LIBS)
rt_sim_la_LIBADD += $(GTHREAD_LIBS)
rt_sim_la_LIBADD += $(GLIB_LIBS)
rt_sim_la_LIBADD += $(GTK3_LIBS)
rt_sim_la_LIBADD += $(GIO_LIBS)

rt_sim_la_SOURCES = rt_sim.c

else

# libtool stinks on windows. Rather, windows dlls stink
# I'll build my own! With Black Jack! And hookers!

all: rt_sim.dll

SUFFIXES = .dll
CLEANFILES = *.dll

AM_CFLAGS += -Wl,-no-undefined
AM_CFLAGS += -Wl,--unresolved-symbols=ignore-all
AM_CFLAGS += -Wl,--enable-runtime-pseudo-reloc
AM_CFLAGS += -shared
AM_CFLAGS += -fPIC

AM_LDFLAGS := -L$(top_builddir)/src/net/  -lproto
AM_LDFLAGS += -L$(top_builddir)/src/util/ -lutil
AM_LDFLAGS += $(GMODULE_LIBS)
AM_LDFLAGS += $(GTHREAD_LIBS)
AM_LDFLAGS += $(GLIB_LIBS)
AM_LDFLAGS += $(GTK3_LIBS)
AM_LDFLAGS += $(GIO_LIBS)
AM_LDFLAGS += -L$(top_builddir)/src/server -lhost


plugin_DATA = $(top_builddir)/src/server/backends/SIM/rt_sim.dll

%.dll: %.c
	$(CC) $(AM_CPPFLAGS) $(AM_CFLAGS) -I $(top_srcdir)/include -o $@  $< $(AM_LDFLAGS)
endif


