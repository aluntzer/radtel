AM_CPPFLAGS = -I$(top_srcdir)/src/include  -I$(top_srcdir)/src/server/include
AM_CFLAGS := $(GMODULE_CFLAGS)
AM_CFLAGS += $(GTHREAD_CFLAGS)
AM_CFLAGS += $(GLIB_CFLAGS)

noinst_LIBRARIES = libbackend.a

libbackend_a_SOURCES = be_moveto_azel.c \
		       be_shared_comlink_acquire.c \
		       be_shared_comlink_release.c \
		       be_shared_comlink_read.c \
		       be_shared_comlink_write.c \
		       be_recalibrate_pointing.c \
		       be_park_telescope.c \
		       be_spec_acq_cfg.c \
		       be_spec_acq_cfg_get.c \
		       be_getpos_azel.c \
		       be_spec_acq_enable.c \
		       be_get_capabilities_drive.c \
		       be_get_capabilities_spec.c
