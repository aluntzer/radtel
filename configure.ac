AC_INIT([radtel], [0.1], [armin.luntzer@univie.ac.at], [])
AM_INIT_AUTOMAKE([subdir-objects foreign dist-xz])

AC_PROG_CC





PKG_CHECK_MODULES(server, "glib-2.0 gio-2.0")
AC_SUBST(server_CFLAGS)
AC_SUBST(server_LIBS)

PKG_CHECK_MODULES(client, "glib-2.0 gio-2.0")
AC_SUBST(client_CFLAGS)
AC_SUBST(client_LIBS)



AC_OUTPUT([Makefile src/Makefile src/server/Makefile src/client/Makefile])
