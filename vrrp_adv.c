/*
 * vrrp_adv.c - VRRP advertisement
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

#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>	// ETH_P_IP = 0x0800, ETH_P_IPV6 = 0x86DD
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include "log.h"
#include "vrrp.h"
#include "vrrp_net.h"
#include "vrrp_rfc.h"

/* VRRP multicast group */
#define INADDR_VRRP_GROUP   0xe0000012	/* 224.0.0.18 */
#define IN6ADDR_VRRP_GROUP "FF02::12"

#define ETHDR_SIZE sizeof(struct ether_header)

#define VRRP_TYPE_ADV   1

/**
 * ether_header vrrp_adv_eth - VRRP ethernet header
 */
static struct ether_header vrrp_adv_eth = {
	.ether_dhost = {0x01,
			0x00,
			0x5e,
			(INADDR_VRRP_GROUP >> 16) & 0x7F,
			(INADDR_VRRP_GROUP >> 8) & 0xFF,
			INADDR_VRRP_GROUP & 0xFF},
	.ether_shost = {0x00,
			0x00,
			0x5e,
			0x00,
			0x01,
			0x00},	/* vrrp->vrid */
};

/**
 * vrrp_adv_eth_build() - build VRRP adv ethernet header
 */
static int vrrp_adv_eth_build(struct iovec *iov, const uint8_t vrid,
			      const int family)
{
	iov->iov_base = malloc(sizeof(struct ether_header));
	struct ether_header *hdr = iov->iov_base;

	if (hdr == NULL) {
		log_error("vrid %d :: malloc - %m", vrid);
		return -1;
	}

	memcpy(hdr, &vrrp_adv_eth, sizeof(struct ether_header));
	hdr->ether_shost[5] = vrid;
	if (family == AF_INET)
		hdr->ether_type = htons(ETH_P_IP);
	else	/* AF_INET6 */
		hdr->ether_type = htons(ETH_P_IPV6);

	iov->iov_len = ETHDR_SIZE;

	return 0;
}

/**
 * vrrp_adv_ip4_build() - build VRRP IPv4 advertisement
 */
static int vrrp_adv_ip4_build(struct iovec *iov, const struct vrrp_net *vnet)
{
	iov->iov_base = malloc(sizeof(struct iphdr));

	struct iphdr *iph = iov->iov_base;

	if (iph == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	iph->ihl = 0x5;
	iph->version = IPVERSION;
	iph->tos = 0x00;
	iph->tot_len = htons(IPHDR_SIZE + vnet->adv_getsize(vnet));
	iph->id = htons(0xdead);
	iph->ttl = 0xff;	/* VRRP_TTL */
	iph->frag_off = 0x00;
	iph->protocol = IPPROTO_VRRP;

	iph->saddr = vnet->vif.ip_addr.s_addr;
	iph->daddr = htonl(INADDR_VRRP_GROUP);

	iph->check = 0;
	iph->check = cksum((unsigned short *) iph, IPHDR_SIZE);

	iov->iov_len = IPHDR_SIZE;

	return 0;
}

/**
 * vrrp_adv_ip6_build() - build VRRP IPv6 advertisement
 */
static int vrrp_adv_ip6_build(struct iovec *iov, const struct vrrp_net *vnet)
{
	iov->iov_base = malloc(sizeof(struct ip6_hdr));

	struct ip6_hdr *ip6h = iov->iov_base;

	if (ip6h == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	ip6h->ip6_flow = htonl((6 << 28) | (0 << 20) | 0);
	ip6h->ip6_plen = htons(vnet->adv_getsize(vnet));
	ip6h->ip6_nxt = IPPROTO_VRRP;
	ip6h->ip6_hlim = 0xff;	/* VRRP_TTL */

	memcpy(&ip6h->ip6_src, &vnet->vif.ip_addr6, sizeof(struct in6_addr));

	if (inet_pton(AF_INET6, IN6ADDR_VRRP_GROUP, &(ip6h->ip6_dst)) != 1) {
		log_error("vrid %d :: inet_pton - %m", vnet->vrid);
		return -1;
	}

	iov->iov_len = sizeof(struct ip6_hdr);

	return 0;
}

/**
 * vrrp_net_adv_build() - build VRRP adv pkt
 */
static int vrrp_adv_build(struct iovec *iov, const struct vrrp_net *vnet,
			  const struct vrrp *vrrp)
{
	iov->iov_base = malloc(vnet->adv_getsize(vnet));

	struct vrrphdr *pkt = iov->iov_base;

	if (pkt == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	pkt->version_type = (vrrp->version << 4) | VRRP_TYPE_ADV;
	pkt->vrid = vnet->vrid;
	pkt->priority = vrrp->priority;
	pkt->naddr = vrrp->naddr;

	if (vrrp->version == RFC3768) {
		pkt->auth_type = vrrp->auth_type;
		pkt->adv_int = vrrp->adv_int;
	}
	else if (vrrp->version == RFC5798) {
		pkt->max_adv_int = htons(vrrp->adv_int);
	}

	/* write vrrp_ip addresses */
	uint32_t *vip_addr =
	    (uint32_t *) ((unsigned char *) pkt + VRRP_PKTHDR_SIZE);

	struct vrrp_ip *vip_ptr = NULL;
	uint32_t pos = 0;
	int naddr = 0;

	list_for_each_entry_reverse(vip_ptr, &vnet->vip_list, iplist) {

		if (vnet->family == AF_INET) {
			vip_addr[pos] = vip_ptr->ip_addr.s_addr;
			++pos;
		}
		else {	/* AF_INET6 */
			memcpy(&vip_addr[pos], &vip_ptr->ip_addr6,
			       sizeof(struct in6_addr));
			pos += 4;
		}
		++naddr;

		if (naddr > vrrp->naddr) {
			log_error
			    ("vrid %d :: Build invalid avd pkt, try to write more vip than expected",
			     vnet->vrid);
			return -1;
		}
	};

	/* auth password */
	if ((vrrp->version == RFC2338) && (vrrp->auth_type == SIMPLE)
	    && (vrrp->auth_data != NULL)) {
		uint32_t *auth_data = vip_addr + pos;
		memcpy(auth_data, vrrp->auth_data, strlen(vrrp->auth_data));
	}

	/* chksum */
	pkt->chksum = vnet->adv_checksum(vnet, pkt, NULL, NULL);

	/* iov_len */
	iov->iov_len = vnet->adv_getsize(vnet);

	return 0;
}

/**
 * vrrp_adv_send() - send VRRP adv pkt
 */
int vrrp_adv_send(struct vrrp_net *vnet)
{
	return vrrp_net_send(vnet, vnet->__adv, ARRAY_SIZE(vnet->__adv));
}

/**
 * vrrp_adv_send_zero() - send VRRP adv pkt with priority 0
 */
int vrrp_adv_send_zero(struct vrrp_net *vnet)
{
	/* get adv buffer */
	struct iovec *iov = &vnet->__adv[2];
	struct vrrphdr *pkt = iov->iov_base;

	/* set priority to 0 and recompute checksum */
	uint8_t priority = pkt->priority;
	pkt->priority = 0;
	uint16_t chksum = pkt->chksum;

	/* chksum */
	pkt->chksum = vnet->adv_checksum(vnet, pkt, NULL, NULL);

	/* send adv pkt */
	int ret = vrrp_net_send(vnet, vnet->__adv, ARRAY_SIZE(vnet->__adv));

	/* restaure original priority and checksum */
	pkt->priority = priority;
	pkt->chksum = chksum;

	return ret;
}

/**
 * vrrp_adv_init() - init advertisement pkt to send 
 */
int vrrp_adv_init(struct vrrp_net *vnet, const struct vrrp *vrrp)
{
	int status = -1;

	status = vrrp_adv_eth_build(&vnet->__adv[0], vnet->vrid, vnet->family);

	if (vnet->family == AF_INET)
		status |= vrrp_adv_ip4_build(&vnet->__adv[1], vnet);
	else /* AF_INET6 */
		status |= vrrp_adv_ip6_build(&vnet->__adv[1], vnet);

	status |= vrrp_adv_build(&vnet->__adv[2], vnet, vrrp);

	return status;
}

/**
 * vrrp_adv_cleanup() 
 */
void vrrp_adv_cleanup(struct vrrp_net *vnet)
{
	/* clean iovec */
	for (int i = 0; i < 3; ++i) {
		struct iovec *iov = &vnet->__adv[i];
		free(iov->iov_base);
	}
}
