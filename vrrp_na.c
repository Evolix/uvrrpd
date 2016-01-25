/*
 * vrrp_na.c - Neighbour Advertisement 
 *
 *	TODO : need work to use chksum helper in vrrp_ip6.c
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

#ifdef HAVE_IP6

#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>	// struct nd_neighbor_advert

#include "log.h"
#include "vrrp.h"
#include "vrrp_net.h"

#define IN6ADDR_MCAST "ff02::1"

#define ETHDR_SIZE sizeof(struct ether_header)

/**
 * ether_header vrrp_na_eth
 */
static struct ether_header vrrp_na_eth = {
	.ether_dhost = {0x33, 0x33, 0x00,
			0x00, 0x00, 0x01},
	.ether_shost = {0x00,
			0x00,
			0x52,
			0x00,
			0x01,
			0x00},	/* vrrp->vrid */
};

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
 * vrrp_na_eth_build()
 */
static int vrrp_na_eth_build(struct iovec *iov, const uint8_t vrid)
{
	iov->iov_base = malloc(sizeof(struct ether_header));

	struct ether_header *hdr = iov->iov_base;

	if (hdr == NULL) {
		log_error("vrid %d :: malloc - %m", vrid);
		return -1;
	}

	memcpy(hdr, &vrrp_na_eth, sizeof(struct ether_header));

	hdr->ether_shost[5] = vrid;
	hdr->ether_type = htons(ETH_P_IPV6);

	iov->iov_len = ETHDR_SIZE;

	return 0;
}

/**
 * vrrp_na_ip6_build()
 */
static int vrrp_na_ip6_build(struct iovec *iov, struct vrrp_ip *ip,
			     const struct vrrp_net *vnet)
{
	iov->iov_base = malloc(sizeof(struct ip6_hdr));

	struct ip6_hdr *ip6h = iov->iov_base;

	if (ip6h == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	ip6h->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
	ip6h->ip6_plen = htons(sizeof(struct nd_neighbor_advert));
	ip6h->ip6_nxt = IPPROTO_ICMPV6;
	ip6h->ip6_hlim = 0xff;

	memcpy(&ip6h->ip6_src, &ip->ip_addr6, sizeof(struct in6_addr));

	if (inet_pton(AF_INET6, IN6ADDR_MCAST, &(ip6h->ip6_dst)) != 1) {
		log_error("vrid %d :: inet_pton - %m", vnet->vrid);
		return -1;
	}

	iov->iov_len = sizeof(struct ip6_hdr);

	return 0;
}


/**
 * vrrp_na_chksum() - compute na chksum
 */
uint16_t vrrp_na_chksum(struct iovec * iov, struct nd_neighbor_advert * na)
{
	/* reset chksum */
	na->nd_na_hdr.icmp6_cksum = 0;

	const struct ip6_hdr *iph = iov->iov_base;

	/* pseudo_header ipv6 */
	struct pshdr_ip6 psh = { IN6ADDR_ANY_INIT,	/* saddr */
		IN6ADDR_ANY_INIT,	/* daddr */
		0,	/* length */
		{0, 0, 0},	/* zeros[3] */
		0	/* next_header */
	};

	memcpy(&psh.saddr, &iph->ip6_src, sizeof(struct in6_addr));
	memcpy(&psh.daddr, &iph->ip6_dst, sizeof(struct in6_addr));

	bzero(&psh.zeros, sizeof(psh.zeros));
	psh.next_header = IPPROTO_ICMPV6;
	psh.len = htons(sizeof(struct nd_neighbor_advert));

	uint32_t psh_size =
	    sizeof(struct pshdr_ip6) + sizeof(struct nd_neighbor_advert);
	unsigned short buf[psh_size / sizeof(short)];

	memcpy(buf, &psh, sizeof(struct pshdr_ip6));
	memcpy(buf + sizeof(struct pshdr_ip6) / sizeof(short), na,
	       sizeof(struct nd_neighbor_advert));

	return cksum(buf, psh_size);
}

/**
 * vrrp_na_build()
 */
static int vrrp_na_build(struct iovec *iov, struct vrrp_ip *ip,
			 const struct vrrp_net *vnet)
{
	iov->iov_base = malloc(sizeof(struct nd_neighbor_advert));

	struct nd_neighbor_advert *na = iov->iov_base;

	if (na == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	na->nd_na_hdr.icmp6_type = ND_NEIGHBOR_ADVERT;
	na->nd_na_hdr.icmp6_code = 0;
	na->nd_na_hdr.icmp6_cksum = 0;
	/* Set R/S/O flags as = R=1, S=0, O=1 (RFC 5798, 6.4.2.(395)) */
	na->nd_na_flags_reserved = ND_NA_FLAG_ROUTER | ND_NA_FLAG_OVERRIDE;
	memcpy(&na->nd_na_target, &ip->ip_addr6, sizeof(struct in6_addr));

	na->nd_na_hdr.icmp6_cksum = vrrp_na_chksum(&ip->__topology[1], na);

	iov->iov_len = sizeof(struct nd_neighbor_advert);

	return 0;
}

/**
 * vrrp_na_send() - for each vip send an unsollicited neighbor advertisement
 */
int vrrp_na_send(struct vrrp_net *vnet)
{
	struct vrrp_ip *vip_ptr = NULL;

	/* we have to send one na by vip */
	list_for_each_entry_reverse(vip_ptr, &vnet->vip_list, iplist) {
		vrrp_net_send(vnet, vip_ptr->__topology,
			      ARRAY_SIZE(vip_ptr->__topology));
	}

	return 0;
}

/**
 * vrrp_na_init() 
 */
int vrrp_na_init(struct vrrp_net *vnet)
{
	int status = -1;

	/* we have to build one na pkt by vip */
	struct vrrp_ip *vip_ptr = NULL;

	list_for_each_entry_reverse(vip_ptr, &vnet->vip_list, iplist) {
		status = vrrp_na_eth_build(&vip_ptr->__topology[0], vnet->vrid);
		status |=
		    vrrp_na_ip6_build(&vip_ptr->__topology[1], vip_ptr, vnet);
		status |= vrrp_na_build(&vip_ptr->__topology[2], vip_ptr, vnet);
	}

	return status;
}


/**
 * vrrp_na_cleanup() 
 */
void vrrp_na_cleanup(struct vrrp_net *vnet)
{
	/* clean na buffer for each vrrp_ip addr */
	struct vrrp_ip *vip_ptr = NULL;

	list_for_each_entry(vip_ptr, &vnet->vip_list, iplist) {

		/* clean iovec */
		for (int i = 0; i < 3; ++i) {
			struct iovec *iov = &vip_ptr->__topology[i];
			free(iov->iov_base);
		}

	}
}

#endif /* HAVE_IP6 */
