ACLOCAL_AMFLAGS = -I config

sbin_SCRIPTS = vrrp_switch.sh
EXTRA_DIST = vrrp_switch.sh

AM_CPPFLAGS = -D_GNU_SOURCE -DPATH="$(sbindir)"

AM_CFLAGS = -Wall -W -Werror

sbin_PROGRAMS = uvrrpd

noinst_HEADERS =				\
	bits.h					\
	common.h				\
	list.h					\
	log.h					\
	uvrrpd.h				\
	vrrp_adv.h				\
	vrrp_arp.h				\
	vrrp_exec.h				\
	vrrp.h					\
	vrrp_ipx.h				\
	vrrp_na.h				\
	vrrp_net.h				\
	vrrp_options.h				\
	vrrp_rfc.h				\
	vrrp_state.h				\
	vrrp_timer.h

uvrrpd_SOURCES =				\
	log.c					\
	uvrrpd.c				\
	vrrp_adv.c				\
	vrrp_arp.c				\
	vrrp.c					\
	vrrp_exec.c				\
	vrrp_ip4.c				\
	vrrp_ip6.c				\
	vrrp_na.c				\
	vrrp_net.c				\
	vrrp_options.c				\
	vrrp_state.c				\
	vrrp_timer.c
