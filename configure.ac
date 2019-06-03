AC_INIT([radtel], [0.0.1], [armin.luntzer@univie.ac.at], [])
AM_INIT_AUTOMAKE([subdir-objects foreign dist-xz])

AC_PROG_CC
AC_CONFIG_MACRO_DIRS([m4])


AC_PREFIX_DEFAULT([/usr])
# want /etc by default
test "x$prefix" = xNONE && test "x$sysconfdir" = 'x${prefix}/etc' && sysconfdir='/etc'


AC_CANONICAL_HOST
# Detect the target system
case "${host_os}" in
    mingw*)
        IS_WINDOWS=true
        ;;
    darwin*)
        IS_DARWIN=true
        ;;
esac

dnl libootl sucks on windows (for our purpose), so we'll build
dnl plugin dlls ourselves
test x$IS_WINDOWS != xtrue && LT_INIT([dlopen])
test x$IS_WINDOWS = xtrue && AC_PROG_RANLIB


AM_CONDITIONAL([OS_WINDOWS], [test x$IS_WINDOWS = xtrue])
AM_CONDITIONAL([OS_DARWIN],  [test x$IS_DARWIN = xtrue])




AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
GLIB_GSETTINGS

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
AC_OUTPUT([src/server/backends/SIM/Makefile])
