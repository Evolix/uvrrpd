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
#ifdef HAVE_IP6
	struct in6_addr addr6;
#endif
};

/**
 * struct vrrp_ipx - Helper functions 
 */
struct vrrp_ipx {
	/* AF_INET or AF6_INET */
	int family;

	/* setsockopt() - socket options */
	int (*setsockopt) (int, int);

	/* mgroup() - join IPvX VRRP multicast group */
	int (*mgroup) (struct vrrp_net *);

	/* cmp() - Compare two IPvX address */
	int (*cmp) (union vrrp_ipx_addr *, union vrrp_ipx_addr *);

	/* recv() - Read received pkt and fetch/store information
	 *          in struct vrrp_recv */
	int (*recv) (int, struct vrrp_recv *, unsigned char *, ssize_t, int *);

	/* getsize() - get size of IPvX VRRP advertisement pkt */
	int (*getsize) (const struct vrrp_net *);

	/* viplist_cmp() - compare VRRP Virtual IPvX list */
	int (*viplist_cmp) (struct vrrp_net *, struct vrrphdr *);

	/* chksum() - compute IPvX checksum while building
	 * 	      advertisement packet */
	uint16_t(*chksum) (const struct vrrp_net *, struct vrrphdr *,
			    union vrrp_ipx_addr *, union vrrp_ipx_addr *);

	/* ipx_ntop() - call ntop() on IPvX addr */
	const char *(*ipx_ntop) (union vrrp_ipx_addr *, char *);

	/* ipx_pton() - call pton() on IPvX addr */
	int (*ipx_pton) (union vrrp_ipx_addr *, const char *);
};

/* IP4 and IP6 internal modules */
extern struct vrrp_ipx VRRP_IP4;	/* IPv4 module */
#ifdef HAVE_IP6
extern struct vrrp_ipx VRRP_IP6;	/* IPv6 module */
#endif

/**
 * vrrp_ipx_set() - Set l3 helper for vrrp_net 
 */
static inline struct vrrp_ipx *vrrp_ipx_set(int family)
{
	if (family == AF_INET) {
		return &VRRP_IP4;
	}
#ifdef HAVE_IP6
	if (family == AF_INET6) {
		return &VRRP_IP6;
	}
#endif
	return NULL;
}

#endif /* _VRRP_IPX_ */
