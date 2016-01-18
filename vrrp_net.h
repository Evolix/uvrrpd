/*
 * vrrp_net.h 
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


#ifndef _VRRP_NET_
#define _VRRP_NET_

#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>

#include "vrrp_ipx.h"
#include "vrrp_rfc.h"
#include "list.h"

/* from vrrp.h */
struct vrrp;
typedef enum _vrrp_event_type vrrp_event_t;

/**
 * constants
 */
#define IPPROTO_VRRP    112
#define VIP_MAX         255

#define VRRP_AUTH_SIZE 2*sizeof(uint32_t)
#define VRRP_VIPMAX_SIZE VIP_MAX * sizeof(uint32_t)
#define VRRP_PKTHDR_SIZE sizeof(struct vrrphdr)
#define VRRP_PKT_MINSIZE VRRP_PKTHDR_SIZE + sizeof(uint32_t)
#define VRRP_PKT_MAXSIZE VRRP_PKTHDR_SIZE + VRRP_VIPMAX_SIZE + VRRP_AUTH_SIZE

#define IPHDR_SIZE sizeof(struct iphdr)

/**
 * struct vrrp_ip - VRRP IPs addresses
 */
struct vrrp_ip {
	union vrrp_ipx_addr ipx;
	uint8_t netmask;
	struct list_head iplist;
	/* internal buffer for topology update pkt */
	struct iovec __topology[3];
};

/**
 * struct vrrp_if - VRRP interface 
 */
struct vrrp_if {
	char *ifname;
	int mtu;
	union vrrp_ipx_addr ipx;
};


/**
 * struct vrrp_recv - VRRP buffer recv
 */
struct vrrp_recv {
	union vrrp_ipx_addr s_ipx;
	union vrrp_ipx_addr d_ipx;
	struct vrrp_ipx_header header;
	struct vrrphdr adv;
};

#define ip_addr   ipx.addr
#define ip_addr6  ipx.addr6
#define ip_saddr  s_ipx.addr
#define ip_daddr  d_ipx.addr
#define ip_saddr6 s_ipx.addr6
#define ip_daddr6 d_ipx.addr6

/**
 * struct vrrp_net - VRRP net structure
 */
struct vrrp_net {
	/* VRRP id */
	uint8_t vrid;

	/* VRRP interface */
	struct vrrp_if vif;

	/* list of VRRP IP adresses */
	struct list_head vip_list;

	/* family */
	int family;

	/* count IP addresses */
	uint8_t naddr;

	/* listen VRRP socket */
	int socket;

	/* xmit VRRP socket */
	int xmit;

	/* buffer for received pkt */
	struct vrrp_recv __pkt;

	/* buffer for advertisement pkt */
	struct iovec __adv[3];

	/* family helper functions */
	struct vrrp_ipx *ipx_helper;
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


/*
 * funcs
 */
void vrrp_net_init(struct vrrp_net *vnet);
void vrrp_net_cleanup(struct vrrp_net *vnet);
int vrrp_net_socket(struct vrrp_net *vnet);
int vrrp_net_socket_xmit(struct vrrp_net *vnet);
int vrrp_net_vif_getaddr(struct vrrp_net *vnet);
int vrrp_net_vif_mtu(struct vrrp_net *vnet);
int vrrp_net_vip_set(struct vrrp_net *vnet, const char *ip);
vrrp_event_t vrrp_net_recv(struct vrrp_net *vnet, const struct vrrp *vrrp);
int vrrp_net_send(const struct vrrp_net *vnet, struct iovec *iov, size_t len);

#endif /* _VRRP_NET_ */
