AM_CPPFLAGS = -I$(top_srcdir)/src/include -Iinclude
AM_CFLAGS = $(server_CFLAGS)

bin_PROGRAMS = server 

server_SOURCES = main.c \
		 net.c \
		 cmd_proc.c \
		 proc/proc_cmd_capabilities.c \
		 proc/proc_cmd_invalid_pkt.c \
		 ../net/protocol.c \
		 ../net/cmds/cmd_invalid_pkt.c \
		 ../net/cmds/cmd_capabilities.c \
		 ../net/acks/ack_capabilities.c
		

server_LDADD = $(server_LIBS)
