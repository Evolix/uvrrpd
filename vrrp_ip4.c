/*
 * vrrp_ip4.c - IP4 helpers functions
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
#include <netinet/ip.h>
#include <arpa/inet.h>
/* ifreq + ioctl */
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <ifaddrs.h>

#include "vrrp.h"
#include "vrrp_ipx.h"
#include "vrrp_net.h"
#include "vrrp_adv.h"
#include "log.h"
#include "common.h"

#define VRRP_MGROUP4     "224.0.0.18"

/**
 * pshdr_ip4 - pseudo header IPv4 
 */
struct pshdr_ip4 {
	uint32_t saddr;
	uint32_t daddr;
	uint8_t zero;
	uint8_t protocol;
	uint16_t len;
};

/**
 * vrrp_ip4_search_vip() - search one vip in vip list
 * if vip is found
 * 	set found = 1
 * 	_vip_ptr point to vip in vnet->vip_list
 */
#define vrrp_ip4_search_vip(vnet, _vip_ptr, _addr, found) \
    do { \
        if (_addr != NULL) \
        list_for_each_entry_reverse(_vip_ptr, &vnet->vip_list, iplist) { \
            if ( _vip_ptr->ip_addr.s_addr == *_addr) {\
                found = 1; \
                break; \
            } \
        } \
    } while(0)


/**
 * vrrp_ip4_mgroup() - join IPv4 VRRP multicast group
 */
static int vrrp_ip4_mgroup(struct vrrp_net *vnet)
{
	/* Join VRRP multicast group */
	struct ip_mreq group = { {0}, {0} };
	struct in_addr group_addr = { 0 };

	if (inet_pton(AF_INET, VRRP_MGROUP4, &group_addr) < 0) {
		log_error("vrid %d :: inet_pton - %s", vnet->vrid,
			  strerror(errno));
		return -1;
	}
	group.imr_multiaddr.s_addr = group_addr.s_addr;
	group.imr_interface.s_addr = vnet->vif.ip_addr.s_addr;

	if (setsockopt
	    (vnet->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group,
	     sizeof(struct ip_mreq)) < 0) {
		log_error("vrid %d :: setsockopt - %s", vnet->vrid,
			  strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * vrrp_ip4_cmp() - Compare VIP list between received vrrpkt and our instance
 * Return 0 if the list is the same,
 * the number of different VIP else
 */
static int vrrp_ip4_cmp(struct vrrp_net *vnet, struct vrrphdr *vrrpkt)
{
	/* compare IP address(es) */
	uint32_t *vip_addr =
	    (uint32_t *) ((unsigned char *) vrrpkt + VRRP_PKTHDR_SIZE);

	uint32_t naddr = 0;
	int ndiff = 0;

	for (naddr = 0; naddr < vnet->naddr; ++naddr) {
		/* vip in vrrpkt */
		in_addr_t *addr = vip_addr + naddr;

		/* search in vrrp_ip list */
		struct vrrp_ip *vip_ptr = NULL;
		int found = 0;
		vrrp_ip4_search_vip(vnet, vip_ptr, addr, found);

		if (!found) {
			log_warning
			    ("vrid %d :: Invalid pkt - Virtual IP address unexpected",
			     vnet->vrid);
			++ndiff;
		}
	}
	return ndiff;
}

/**
 * vrrp_ip4_recv() - Fill vrrp_l3 header from received pkt
 */
static int vrrp_ip4_recv(int sock_fd, struct vrrp_recv *recv,
			 unsigned char *buf, ssize_t buf_size, int *payload_pos)
{
	ssize_t len;
	struct iphdr *ip;

	len = read(sock_fd, buf, buf_size);
	ip = (struct iphdr *) buf;

	recv->header.len = ip->ihl << 2;
	recv->header.proto = ip->protocol;
	recv->header.totlen = ntohs(ip->tot_len);
	recv->header.ttl = ip->ttl;

	/* saddr and daddr of the ip header */
	recv->ip_saddr.s_addr = ip->saddr;
	recv->ip_daddr.s_addr = ip->daddr;

	/* VRRP adv begin here */
	*payload_pos = recv->header.len;

	return len;
}

/**
 * vrrp_ip4_getsize() - return the current size of vrrp instance
 */
static int vrrp_ip4_getsize(const struct vrrp_net *vnet)
{
	return sizeof(struct vrrphdr) + vnet->naddr * sizeof(uint32_t) +
	    VRRP_AUTH_SIZE;
}

/**
 * vrrp_ip4_chksum() - compute vrrp adv chksum 
 */
static uint16_t vrrp_ip4_chksum(const struct vrrp_net *vnet,
				struct vrrphdr *pkt,
				union vrrp_ipx_addr *ipx_saddr,
				union vrrp_ipx_addr *ipx_daddr)
{
	/* reset chksum */
	pkt->chksum = 0;

	if ((pkt->version_type >> 4) == RFC3768)
		return cksum((unsigned short *) pkt, vrrp_ip4_getsize(vnet));

	if ((pkt->version_type >> 4) == RFC5798) {
		const struct iovec *iov_iph = &vnet->__adv[1];
		const struct iphdr *iph = iov_iph->iov_base;

		/* get saddr and daddr */
		uint32_t saddr = 0;
		uint32_t daddr = 0;

		if (ipx_saddr != NULL)
			saddr = ipx_saddr->addr.s_addr;
		if (ipx_daddr != NULL)
			daddr = ipx_daddr->addr.s_addr;

		/* pseudo_header ipv4 */
		struct pshdr_ip4 psh = { 0 };

		/* if saddr and daddr are not specified, use addresses from iphdr */
		psh.saddr = (saddr ? saddr : iph->saddr);
		psh.daddr = (daddr ? daddr : iph->daddr);
		psh.zero = 0;
		psh.protocol = iph->protocol;
		psh.len = htons(vrrp_ip4_getsize(vnet));

		uint32_t psh_size =
		    sizeof(struct pshdr_ip4) + vrrp_ip4_getsize(vnet);

		unsigned short buf[psh_size / sizeof(short)];

		memcpy(buf, &psh, sizeof(struct pshdr_ip4));
		memcpy(buf + sizeof(struct pshdr_ip4) / sizeof(short), pkt,
		       vrrp_ip4_getsize(vnet));

		return cksum(buf, psh_size);
	}

	return 0;
}


/**
 * vrrp_ip4_setsockopt() - no need to setsockopt() in IPv4
 */
static int vrrp_ip4_setsockopt( __attribute__ ((unused))
			       int sock_fd, __attribute__ ((unused))
			       int vrid)
{
	return 0;
}

/* exported VRRP_IP4 helper */
struct vrrp_ipx VRRP_IP4 = {
	.family = AF_INET,
	.setsockopt = vrrp_ip4_setsockopt,
	.mgroup = vrrp_ip4_mgroup,
	.cmp = vrrp_ip4_cmp,
	.recv = vrrp_ip4_recv,
	.chksum = vrrp_ip4_chksum,
	.getsize = vrrp_ip4_getsize,
};
