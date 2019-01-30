# we build plugin shared objects with our own rules rather than via libtool
plugin_CFLAGS := $(GMODULE_CFLAGS)
plugin_CFLAGS += $(GTHREAD_CFLAGS)
plugin_CFLAGS += $(GLIB_CFLAGS)
plugin_CFLAGS += -I$(top_srcdir)/src/include  -I$(top_srcdir)/src/server/include

all: srt_drive.so srt_comlink.so srt_spectrometer.so

SUFFIXES = .so
CLEANFILES = *.so

%.so: %.c
	$(CC) $(plugin_CFLAGS) -shared -fPIC -I $(top_srcdir)/include -o $@  $<

%.so: %.o
	$(CC) $(plugin_LDFLAGS) -shared -fPIC -o $@  $<
