AC_PREREQ(2.50)
AC_INIT(uvrrpd, 1.0, arno@ankhoon.net)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AM_INIT_AUTOMAKE([foreign dist-xz tar-ustar])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AC_CONFIG_SRCDIR(uvrrpd.c)

AC_PROG_CC
AC_PROG_CC_STDC			dnl for -std=gnu99
AC_USE_SYSTEM_EXTENSIONS	dnl for -D_GNU_SOURCE
AC_PROG_LIBTOOL
AC_SEARCH_LIBS([clock_gettime],[rt posix4])

AC_SYS_LARGEFILE

AC_CONFIG_FILES([
	Makefile
])

AC_OUTPUT()
