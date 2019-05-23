#paths set at make time
confdir  = ${sysconfdir}/radtel/

# we build plugin shared objects with our own rules rather than via libtool
if !OS_WINDOWS
plugin_CPPFLAGS := \-DCONFDIR=\"$(confdir)\"
endif
plugin_CFLAGS := $(GMODULE_CFLAGS)
plugin_CFLAGS += $(GTHREAD_CFLAGS)
plugin_CFLAGS += $(GLIB_CFLAGS)
plugin_CFLAGS += $(GTK3_CFLAGS)
plugin_CFLAGS += -I$(top_srcdir)/src/include  -I$(top_srcdir)/src/server/include
plugin_LDADD := $(GTK3_LIBS)
plugin_LDADD += $(GTHREAD_LIBS)
plugin_LDADD += $(GLIB_LIBS)
plugin_LDADD += $(GIO_LIBS)



if OS_WINDOWS
all: rt_sim.dll

SUFFIXES = .dll
CLEANFILES = *.dll


%.dll: %.c
	$(CC) -Wl,-no-undefined -Wl,--unresolved-symbols=ignore-all -Wl,--enable-runtime-pseudo-reloc   $(plugin_CPPFLAGS) $(plugin_CFLAGS) -shared -fPIC -fopenmp -I $(top_srcdir)/include -o $@  $< -L../../ -lhost $(plugin_LDADD)

else

all: rt_sim.so

SUFFIXES = .so
CLEANFILES = *.so

%.so: %.c
	$(CC) $(plugin_CPPFLAGS) $(plugin_CFLAGS) -shared -fPIC -fopenmp -I $(top_srcdir)/include -o $@  $< $(plugin_LDADD)

%.so: %.o
	$(CC) $(plugin_LDFLAGS) -shared -fPIC -o $@  $<
endif
