AC_INIT([radtel], [0.1], [armin.luntzer@univie.ac.at], [])
AM_INIT_AUTOMAKE([subdir-objects foreign dist-xz])

AC_PROG_CC
AC_PROG_RANLIB


PKG_CHECK_MODULES([GMODULE], [gmodule-2.0])
PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GIO],  [gio-2.0])
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 >= 3.24.4])
AC_SEARCH_LIBS([log10], [m])


AC_OUTPUT([Makefile])
AC_OUTPUT([src/Makefile])
AC_OUTPUT([src/net/Makefile])
AC_OUTPUT([src/util/Makefile])
AC_OUTPUT([src/client/Makefile])
AC_OUTPUT([src/server/Makefile])
AC_OUTPUT([src/server/api/Makefile])
AC_OUTPUT([src/server/backends/Makefile])
AC_OUTPUT([src/server/backends/SRT/Makefile])
