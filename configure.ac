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

AC_MSG_CHECKING(for debug options)

DEBUG_OPTS=""
AC_ARG_ENABLE(debug-logs,
	AS_HELP_STRING([--enable-debug-logs],[Enable debug logs]),
	[case "$enableval" in
	y | yes) DEBUG_OPTS="-DDEBUG";;
	esac])

AC_ARG_ENABLE(asserts,
	AS_HELP_STRING([--disable-asserts],[Disable run-time assertions]),
	[case "$enableval" in
	n | no) DEBUG_OPTS="$DEBUG_OPTS -DNDEBUG";;
	esac])

AC_MSG_RESULT(${DEBUG_OPTS:-none})
AC_SUBST(DEBUG_OPTS)

AC_CONFIG_FILES([
	Makefile
])

AC_OUTPUT()
