/*
 * vrrp_arp.c 
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
#include <net/if_arp.h>

#include "log.h"
#include "vrrp.h"
#include "vrrp_net.h"

#define ETHDR_SIZE sizeof(struct ether_header)

/**
 * ether_header vrrp_arp_eth 
 */
static struct ether_header vrrp_arp_eth = {
	.ether_dhost = {0xff, 0xff, 0xff,
			0xff, 0xff, 0xff},
	.ether_shost = {0x00,
			0x00,
			0x5e,
			0x00,
			0x01,
			0x00},	/* vrrp->vrid */
};


#define IP_ALEN     4
/**
 * arphdr_eth - ARP header
 */
struct arphdr_eth {
	unsigned char ar_sha[ETH_ALEN];	/* Sender hardware address */
	unsigned char ar_sip[IP_ALEN];	/* Sender IP address */
	unsigned char ar_tha[ETH_ALEN];	/* Target hardware address */
	unsigned char ar_tip[IP_ALEN];	/* Target IP address */
};


/**
 * vrrp_arp_eth_build() 
 */
static int vrrp_arp_eth_build(struct iovec *iov, const uint8_t vrid)
{
	iov->iov_base = malloc(sizeof(struct ether_header));

	struct ether_header *hdr = iov->iov_base;

	if (hdr == NULL) {
		log_error("vrid %d :: malloc - %m", vrid);
		return -1;
	}

	memcpy(hdr, &vrrp_arp_eth, sizeof(struct ether_header));

	hdr->ether_shost[5] = vrid;
	hdr->ether_type = htons(ETHERTYPE_ARP);

	iov->iov_len = ETHDR_SIZE;

	return 0;
}

/**
 * vrrp_arp_send() - Send arp gratuitous for each vip
 */
int vrrp_arp_send(struct vrrp_net *vnet)
{
	struct vrrp_ip *vip_ptr = NULL;

	/* we have to send one arp pkt by vip */
	list_for_each_entry_reverse(vip_ptr, &vnet->vip_list, iplist) {
		vrrp_net_send(vnet, vip_ptr->__topology,
			      ARRAY_SIZE(vip_ptr->__topology));
	}

	return 0;
}

/**
 * vrrp_arp_build() - Build arp header
 */
static int vrrp_arp_build(struct iovec *iov, const uint8_t vrid)
{
	iov->iov_base = malloc(sizeof(struct arphdr));

	struct arphdr *arph = iov->iov_base;

	if (arph == NULL) {
		log_error("vrid %d :: malloc - %m", vrid);
		return -1;
	}

	arph->ar_hrd = htons(ARPHRD_ETHER);	/* Format of hardware address */
	arph->ar_pro = htons(ETHERTYPE_IP);	/* Format of protocol address */
	arph->ar_hln = ETH_ALEN;	/* Length of hardware address */
	arph->ar_pln = IP_ALEN;	/* Length of protocol address */
	arph->ar_op = htons(ARPOP_REQUEST);	/* ARP opcode (command) */

	iov->iov_len = sizeof(struct arphdr);

	return 0;
}

/**
 * vrrp_arp_vrrp_build() - VRRP arp payload
 */
static int vrrp_arp_vrrp_build(struct iovec *iov, struct vrrp_ip *vip,
			       struct vrrp_net *vnet)
{
	iov->iov_base = malloc(sizeof(struct arphdr_eth));

	struct arphdr_eth *arpeth = iov->iov_base;

	if (arpeth == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	arpeth->ar_sha[0] = 0x00;
	arpeth->ar_sha[1] = 0x00;
	arpeth->ar_sha[2] = 0x5e;
	arpeth->ar_sha[3] = 0x00;
	arpeth->ar_sha[4] = 0x01;
	arpeth->ar_sha[5] = vnet->vrid;

	memcpy(arpeth->ar_sip, &vip->ip_addr.s_addr, IP_ALEN);
	memset(arpeth->ar_tha, 0xFF, ETH_ALEN);
	memcpy(arpeth->ar_tip, &arpeth->ar_sip, IP_ALEN);

	iov->iov_len = sizeof(struct arphdr_eth);

	return 0;
}

/**
 * vrrp_arp_init() 
 */
int vrrp_arp_init(struct vrrp_net *vnet)
{
	int status = -1;

	/* we have to build one arp pkt by vip */
	struct vrrp_ip *vip_ptr = NULL;

	list_for_each_entry_reverse(vip_ptr, &vnet->vip_list, iplist) {
		status =
		    vrrp_arp_eth_build(&vip_ptr->__topology[0], vnet->vrid);
		status |= vrrp_arp_build(&vip_ptr->__topology[1], vnet->vrid);
		status |=
		    vrrp_arp_vrrp_build(&vip_ptr->__topology[2], vip_ptr, vnet);
	}

	return status;
}

/**
 * vrrp_arp_cleanup() 
 */
void vrrp_arp_cleanup(struct vrrp_net *vnet)
{
	/* clean arp buffer for each vrrp_ip addr */
	struct vrrp_ip *vip_ptr = NULL;

	list_for_each_entry(vip_ptr, &vnet->vip_list, iplist) {

		/* clean iovec */
		for (int i = 0; i < 3; ++i) {
			struct iovec *iov = &vip_ptr->__topology[i];
			free(iov->iov_base);
		}
	}
}
