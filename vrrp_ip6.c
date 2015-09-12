/*
 * vrrp_ip6.c - IPv6 helpers functions
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


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
/* ifreq + ioctl */
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netdb.h>	/* NI_MAXHOST */

#include "vrrp_ipx.h"
#include "vrrp_net.h"
#include "log.h"
#include "common.h"

#define VRRP_MGROUP6     "ff02::12"
#define IP6HDR_SIZE sizeof(struct ip6_hdr)

/**
 * pshdr_ip6 - pseudo header IPv6
 */
struct pshdr_ip6 {
	struct in6_addr saddr;
	struct in6_addr daddr;
	uint32_t len;
	uint8_t zeros[3];
	uint8_t next_header;
};

/**
 * vrrp_ip6_search_vip() - search one vip in vip list
 * if vip is found
 * 	set found = 1
 * 	_vip_ptr point to vip in vnet->vip_list
 */
#define vrrp_ip6_search_vip(vnet, _vip_ptr, _addr, found) \
    do { \
        list_for_each_entry_reverse(_vip_ptr, &vnet->vip_list, iplist) { \
	    if (memcmp(&(_vip_ptr->ip_addr6), _addr, sizeof(struct in6_addr)) == 0) { \
		found = 1; \
                break; \
	    }\
        } \
    } while(0)

/**
 * vrrp_ip6_setsockopt() - Set socket option
 * used to find ancillary data in recvmsg()
 * see vrrp_ip6_recv()
 */
static int vrrp_ip6_setsockopt(int socket, int vrid)
{
	int on = 1;

	/* IPV6_RECVPKTINFO */
	if (setsockopt
	    (socket, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) < 0) {
		log_error("vrid %d :: setsockopt - %s", vrid, strerror(errno));
		return -1;
	}

	/* IPV6_RECVHOPLIMIT */
	if (setsockopt
	    (socket, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on, sizeof(on)) < 0) {
		log_error("vrid %d :: setsockopt - %s", vrid, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * vrrp_net_join_mgroup6() - join IPv6 VRRP multicast group
 */
static int vrrp_ip6_mgroup(struct vrrp_net *vnet)
{
	/* Join VRRP multicast group */
	struct ipv6_mreq group = { IN6ADDR_ANY_INIT, 0 };
	struct in6_addr group_addr = IN6ADDR_ANY_INIT;

	if (inet_pton(AF_INET6, VRRP_MGROUP6, &group_addr) < 0) {
		log_error("vrid %d :: inet_pton - %s", vnet->vrid,
			  strerror(errno));
		return -1;
	}

	memcpy(&group.ipv6mr_multiaddr, &group_addr, sizeof(struct in6_addr));

	group.ipv6mr_interface = if_nametoindex(vnet->vif.ifname);

	if (setsockopt
	    (vnet->socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &group,
	     sizeof(struct ipv6_mreq)) < 0) {
		log_error("vrid %d :: setsockopt - %s", vnet->vrid,
			  strerror(errno));
		return -1;
	}

	return 0;
}


/**
 * vrrp_ip6_cmp() - Compare VIP list between received vrrpkt and our instance
 * Return 0 if the list is the same,
 * the number of differente VIP else
 */
static int vrrp_ip6_viplist_cmp(struct vrrp_net *vnet, struct vrrphdr *vrrpkt)
{
	uint32_t *vip_addr =
	    (uint32_t *) ((unsigned char *) vrrpkt + VRRP_PKTHDR_SIZE);

#ifdef DEBUG
	print_buf_hexa("vrrp_net_vip6_check vip_addr", vip_addr,
		       sizeof(struct in6_addr));
#endif

	uint32_t pos = 0;
	int naddr = 0;
	int ndiff = 0;

	while (naddr < vnet->naddr) {
		/* vip in vrrpkt */
		struct in6_addr *vip = (struct in6_addr *) (vip_addr + pos);

		/* search in vrrp_ip list */
		struct vrrp_ip *vip_ptr = NULL;
		int found = 0;

		vrrp_ip6_search_vip(vnet, vip_ptr, vip->s6_addr, found);

		if (!found) {
			char host[NI_MAXHOST];
			log_warning
			    ("vrid %d :: Invalid pkt - Virtual IPv6 address unexpected %s",
			     vnet->vrid, inet_ntop(vnet->family, vip, host,
						   sizeof(host)));
			++ndiff;
		}
		pos += 4;
		++naddr;
	}
	return ndiff;
}

/**
 * find_ancillary() - search & find IPv6 ancillary data after 
 * receiving data in a struct msghdr msg (recvmsg())
 */
static inline void *find_ancillary(struct msghdr *msg, int cmsg_type)
{
	struct cmsghdr *cmsg = NULL;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if ((cmsg->cmsg_level == IPPROTO_IPV6)
		    && (cmsg->cmsg_type == cmsg_type)) {
			return (CMSG_DATA(cmsg));
		}
	}

	return NULL;
}

/** 
 * vrrp_ip6_recv() - Fill vrrp_ipx_header from received pkt
 */
static int vrrp_ip6_recv(int sock_fd, struct vrrp_recv *recv,
			 unsigned char *buf, ssize_t buf_size, int *payload_pos)
{
	ssize_t len;

	/* IPv6 raw sockets return no IP header. We must query
	 * src/dest via socket options/ancillary data */
	struct msghdr msg;
	struct sockaddr_in6 src;
	struct iovec iov;
	uint8_t ancillary[64];

	msg.msg_name = &src;
	msg.msg_namelen = sizeof(src);
	iov.iov_base = buf;
	iov.iov_len = buf_size;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = ancillary;
	msg.msg_controllen = sizeof(ancillary);
	msg.msg_flags = 0;

	len = recvmsg(sock_fd, &msg, 0);

	if (len < 0) {
		log_error("recvmsg - %s", strerror(errno));
		return -1;
	}

	/* SRC ADDRESS */
	memcpy(&recv->ip_saddr6, &src.sin6_addr, sizeof(struct in6_addr));

	uint8_t *opt;

	/* HOPLIMIT */
	opt = find_ancillary(&msg, IPV6_HOPLIMIT);
	if (opt == NULL) {
		log_error("recvmsg - unknown hop limit");
		return -1;
	}

	recv->header.ttl = *(int *) opt;

	/* DST ADDRESS */
	opt = find_ancillary(&msg, IPV6_PKTINFO);
	if (opt == NULL) {
		log_error("recvmsg - unknown dst address");
		return -1;
	}

	struct in6_pktinfo *pktinfo = (struct in6_pktinfo *) opt;

	memcpy(&recv->ip_daddr6, &pktinfo->ipi6_addr, sizeof(struct in6_addr));

	recv->header.len = sizeof(struct ip6_hdr);
	recv->header.totlen = recv->header.len + len;
	/* kludge, we force it since we have no way to read it in recvmsg() */
	recv->header.proto = IPPROTO_VRRP;

	/* buf is directly filled with VRRP adv
	 * no need to skip IPv6 header */
	*payload_pos = 0;

	return len;
}

/**
 * vrrp_ip6_getsize() - return the current size of vrrp instance
 */
static int vrrp_ip6_getsize(const struct vrrp_net *vnet)
{
	return sizeof(struct vrrphdr) + vnet->naddr * sizeof(struct in6_addr);
}

/**
 * vrrp_ip6_chksum() - compute VRRP adv chksum
 */
uint16_t vrrp_ip6_chksum(const struct vrrp_net *vnet, struct vrrphdr *pkt,
			 union vrrp_ipx_addr *ipx_saddr,
			 union vrrp_ipx_addr *ipx_daddr)
{
	/* reset chksum */
	pkt->chksum = 0;

	const struct iovec *iov_iph = &vnet->__adv[1];
	const struct ip6_hdr *iph = iov_iph->iov_base;

	/* pseudo_header ipv6 */
	struct pshdr_ip6 psh = { IN6ADDR_ANY_INIT,
		IN6ADDR_ANY_INIT,
		0,
		{0, 0, 0},
		0
	};

	if ((ipx_saddr != NULL) && (ipx_daddr != NULL)) {
		memcpy(&psh.saddr, &ipx_saddr->addr6, sizeof(struct in6_addr));
		memcpy(&psh.daddr, &ipx_daddr->addr6, sizeof(struct in6_addr));
	}
	else {
		memcpy(&psh.saddr, &iph->ip6_src, sizeof(struct in6_addr));
		memcpy(&psh.daddr, &iph->ip6_dst, sizeof(struct in6_addr));
	}


	bzero(&psh.zeros, sizeof(psh.zeros));
	psh.next_header = IPPROTO_VRRP;
	psh.len = htons(vrrp_ip6_getsize(vnet));

	uint32_t psh_size = sizeof(struct pshdr_ip6) + vrrp_ip6_getsize(vnet);
	unsigned short buf[psh_size / sizeof(short)];

	memcpy(buf, &psh, sizeof(struct pshdr_ip6));
	memcpy(buf + sizeof(struct pshdr_ip6) / sizeof(short), pkt,
	       vrrp_ip6_getsize(vnet));

	return cksum(buf, psh_size);
}

/**
 * vrrp_ip6_ntop() - network to string representation
 */
static const char *vrrp_ip6_ntop(union vrrp_ipx_addr *ipx, char *dst)
{
	return inet_ntop(AF_INET6, &ipx->addr6, dst, INET6_ADDRSTRLEN);
}

/**
 * vrrp_ip6_pton() - string representation to network
 */
static int vrrp_ip6_pton(union vrrp_ipx_addr *dst, const char *src)
{
	return inet_pton(AF_INET6, src, &dst->addr6);
}

/**
 * vrrp_ip6_cmp() - compare two vipx
 */
int vrrp_ip6_cmp(union vrrp_ipx_addr *s1, union vrrp_ipx_addr *s2)
{
	return memcmp(&s1->addr6, &s2->addr6, sizeof(struct in6_addr));
}

/* exported VRRP_IP6 helper */
struct vrrp_ipx VRRP_IP6 = {
	.family = AF_INET6,
	.setsockopt = vrrp_ip6_setsockopt,
	.mgroup = vrrp_ip6_mgroup,
	.recv = vrrp_ip6_recv,
	.cmp = vrrp_ip6_cmp,
	.chksum = vrrp_ip6_chksum,
	.getsize = vrrp_ip6_getsize,
	.viplist_cmp = vrrp_ip6_viplist_cmp,
	.ipx_pton = vrrp_ip6_pton,
	.ipx_ntop = vrrp_ip6_ntop
};
