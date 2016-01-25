AC_PREREQ(2.50)
AC_INIT(uvrrpd, 0.1, arno@ankhoon.net)
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

AC_ARG_ENABLE(ipv6, 
	      AS_HELP_STRING([--disable-ipv6], 
              		     [disable ipv6 support (default is autodetect)]), 
	      		     ac_cv_have_ipv6=$enable_ipv6,)

dnl check for ipv6 
if test "$ac_cv_have_ipv6" != no; then
	AC_CHECK_TYPES(struct in6_addr,,,[#include <netinet/in.h>])
	AC_CHECK_TYPES(struct ip6_hdr,,,[#include <netinet/ip6.h>])
	AC_CHECK_DECLS(AF_INET6,,,[#include <sys/socket.h>])
	AC_MSG_CHECKING(for IPv6 headers and structures)
	ac_cv_have_ipv6=no

	if test "$ac_cv_type_struct_in6_addr" = yes; then
		if test "$ac_cv_type_struct_ip6_hdr" = yes; then
			if test "$ac_cv_have_decl_AF_INET6" = yes; then
				ac_cv_have_ipv6=yes
				AC_DEFINE([HAVE_IP6], 1, [Define to enable IPv6 support])
			fi
		fi
	fi
	AC_MSG_RESULT($ac_cv_have_ipv6)
fi


AC_CONFIG_FILES([
	Makefile
])

AC_OUTPUT()
