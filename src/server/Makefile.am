AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS = $(server_CFLAGS)

bin_PROGRAMS = server 

server_SOURCES = main.c \
		 cfg.c \
		 net.c \
		 cmd_proc.c \
		 proc/proc_cmd_capabilities.c \
		 proc/proc_cmd_invalid_pkt.c

server_LDADD = $(server_LIBS) $(top_builddir)/src/net/libprotocmd.a
