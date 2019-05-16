#paths set at make time
confdir  = ${sysconfdir}/radtel/

# we build plugin shared objects with our own rules rather than via libtool
plugin_CPPFLAGS := \-DCONFDIR=\"$(confdir)\"
plugin_CFLAGS := $(GMODULE_CFLAGS)
plugin_CFLAGS += $(GTHREAD_CFLAGS)
plugin_CFLAGS += $(GLIB_CFLAGS)
plugin_CFLAGS += $(GTK3_CFLAGS)
plugin_CFLAGS += -I$(top_srcdir)/src/include  -I$(top_srcdir)/src/server/include

all: rt_sim.so

SUFFIXES = .so
CLEANFILES = *.so

%.so: %.c
	$(CC) $(plugin_CPPFLAGS) $(plugin_CFLAGS) -shared -fPIC -fopenmp -I $(top_srcdir)/include -o $@  $<

%.so: %.o
	$(CC) $(plugin_LDFLAGS) -shared -fPIC -o $@  $<
