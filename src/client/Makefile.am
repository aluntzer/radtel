AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS = $(client_CFLAGS)

bin_PROGRAMS = client 

client_SOURCES = main.c \
		 net.c \
		 cmd_proc.c \
		 proc/proc_cmd_capabilities.c \
		 proc/proc_cmd_invalid_pkt.c \
		 proc/proc_cmd_fail.c \
		 proc/proc_cmd_success.c

client_LDADD = $(client_LIBS) $(top_builddir)/src/net/libprotocmd.a
