/**
 * vrrp_ipx.h
 *
 * Copyright (C) 2014 Arnaud Andre 
 *
 * This file is part of uvrrpd.
 *
 * uvrrpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * uvrrpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with uvrrpd.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VRRP_IPX_
#define _VRRP_IPX_


/* from vrrp_net.h */
struct vrrp_net;
struct vrrphdr;
struct vrrp_recv;

/**
 * struct vrrp_ipx_header - IPvX header informations from received adv pkt
 */
struct vrrp_ipx_header {
	int len;
	int proto;
	int totlen;
	int ttl;
};

/**
 * union vrrp_ipx_addr - IPv4 and IPv6 addr struct
 */
union vrrp_ipx_addr {
	struct in_addr addr;
	struct in6_addr addr6;
};
#define ip_addr   ipx.addr
#define ip_addr6  ipx.addr6
#define ip_saddr  s_ipx.addr
#define ip_daddr  d_ipx.addr
#define ip_saddr6 s_ipx.addr6
#define ip_daddr6 d_ipx.addr6

/**
 * struct vrrp_ipx - Helper functions 
 */
struct vrrp_ipx {
	int family;
	int (*setsockopt) (int, int);
	int (*mgroup) (struct vrrp_net *);
	int (*cmp) (union vrrp_ipx_addr *, union vrrp_ipx_addr *);
	int (*recv) (int, struct vrrp_recv *, unsigned char *, ssize_t, int *);
	int (*getsize) (const struct vrrp_net *);
	int (*viplist_cmp) (struct vrrp_net *, struct vrrphdr *);
	 uint16_t(*chksum) (const struct vrrp_net *, struct vrrphdr *,
			    union vrrp_ipx_addr *, union vrrp_ipx_addr *);
	const char *(*ipx_ntop) (union vrrp_ipx_addr *, char *);
	int (*ipx_pton) (union vrrp_ipx_addr *, const char *);
};
#define set_sockopt  ipx_helper->setsockopt
#define join_mgroup  ipx_helper->mgroup
#define vip_compare  ipx_helper->viplist_cmp
#define ipx_cmp      ipx_helper->cmp
#define pkt_receive  ipx_helper->recv
#define adv_checksum ipx_helper->chksum
#define adv_getsize  ipx_helper->getsize
#define ipx_to_str   ipx_helper->ipx_ntop
#define str_to_ipx   ipx_helper->ipx_pton

extern struct vrrp_ipx VRRP_IP4;	/* IPv4 module */
extern struct vrrp_ipx VRRP_IP6;	/* IPv6 module */

/**
 * vrrp_ipx_set() - Set l3 helper for vrrp_net 
 */
static inline struct vrrp_ipx *vrrp_ipx_set(int family)
{
	if (family == AF_INET) {
		return &VRRP_IP4;
	}
	if (family == AF_INET6) {
		return &VRRP_IP6;
	}

	return NULL;
}

#endif /* _VRRP_IPX_ */
