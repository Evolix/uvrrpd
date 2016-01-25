/*
 * vrrp_net.c - net layer
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
#include <netinet/ip6.h>
#include <arpa/inet.h>
/* ifreq + ioctl */
#include <sys/ioctl.h>
#include <net/if.h>

#include <sys/types.h>
#include <ifaddrs.h>
#include <netdb.h>	/* NI_MAXHOST */

#include "vrrp.h"
#include "vrrp_net.h"
#include "vrrp_adv.h"

#include "common.h"
#include "list.h"
#include "log.h"

#include "linux/types.h"

#define VRRP_TTL         255

static inline void vrrp_net_invalidate_buffer(struct vrrp_net *vnet);

/**
 * vrrp_net_init() - init struct vrrp_net of vrrp instance
 */
void vrrp_net_init(struct vrrp_net *vnet)
{
	vnet->vrid = 0;
	vnet->naddr = 0;
	vnet->socket = 0;
	vnet->xmit = 0;
	vnet->family = AF_INET;
	vnet->ipx_helper = NULL;

	/* init VRRP IPs list */
	INIT_LIST_HEAD(&vnet->vip_list);

	/* init vrrp interface */
	bzero((void *) &vnet->vif, sizeof(struct vrrp_if));

	/* init pkt buffer */
	bzero((void *) &vnet->__pkt, sizeof(struct vrrp_recv));
}

/**
 * vrrp_net_cleanup() - cleanup struct vrrp_net
 */
void vrrp_net_cleanup(struct vrrp_net *vnet)
{
	/* clean VIP addr */
	struct vrrp_ip *vip_ptr = NULL;
	struct vrrp_ip *n = NULL;
	list_for_each_entry_safe(vip_ptr, n, &vnet->vip_list, iplist)
	    free(vip_ptr);

	free(vnet->vif.ifname);

	/* close sockets */
	close(vnet->socket);
	close(vnet->xmit);
}

/**
 * vrrp_net_socket() - create VRRP socket destined to receive VRRP pkt
 */
int vrrp_net_socket(struct vrrp_net *vnet)
{
	/* Open RAW socket */
	vnet->socket = socket(vnet->family, SOCK_RAW, IPPROTO_VRRP);

	if (vnet->socket < 0) {
		log_error("vrid %d :: socket - %m", vnet->vrid);
		return -1;
	}

	vnet->ipx_helper = vrrp_ipx_set(vnet->family);

	if (vnet->ipx_helper == NULL) {
		log_error("family not valid");
		return -1;
	}

	int status = -1;

	status = vnet->set_sockopt(vnet->socket, vnet->vrid);
	status |= vnet->join_mgroup(vnet);

	return status;
}

/**
 * vrrp_net_socket_xmit() - open raw VRRP xmit socket
 */
int vrrp_net_socket_xmit(struct vrrp_net *vnet)
{
	vnet->xmit = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

	if (vnet->xmit < 0) {
		log_error("vrid %d :: socket xmit - %m", vnet->vrid);
		return -1;
	}

	return 0;
}


/**
 * vrrp_net_vif_getaddr() - get IPvX addr from a VRRP interface
 */
int vrrp_net_vif_getaddr(struct vrrp_net *vnet)
{

	struct ifaddrs *ifaddr, *ifa;
	int family;

	if (getifaddrs(&ifaddr) == -1) {
		log_error("vrid %d :: getifaddrs - %m", vnet->vrid);
		return -1;
	}

	/* search address */
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, vnet->vif.ifname) != 0)
			continue;

		if (ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;

		if (family != vnet->family)
			continue;

		if (vnet->family == AF_INET) {
			vnet->vif.ip_addr.s_addr =
			    ((struct sockaddr_in *) ifa->ifa_addr)->
			    sin_addr.s_addr;
		}
#ifdef HAVE_IP6
		else {	/* AF_INET6 */
			struct sockaddr_in6 *src =
			    (struct sockaddr_in6 *) ifa->ifa_addr;
			memcpy(&vnet->vif.ip_addr6, &src->sin6_addr,
			       sizeof(struct in6_addr));
		}
#endif /* HAVE_IP6 */
	}

	freeifaddrs(ifaddr);

	vrrp_net_vif_mtu(vnet);

	return 0;
}

/**
 * vrrp_net_vif_mtu() - get MTU of vrrp interface
 */
int vrrp_net_vif_mtu(struct vrrp_net *vnet)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, vnet->vif.ifname, IFNAMSIZ - 1);

	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	ifr.ifr_addr.sa_family = AF_INET;

	if (ioctl(fd, SIOCGIFMTU, &ifr) < 0) {
		log_error("vrid %d :: ioctl - %m", vnet->vrid);
		return -1;
	}

	vnet->vif.mtu = ifr.ifr_mtu;
	log_debug("%s mtu : %d", vnet->vif.ifname, vnet->vif.mtu);

	close(fd);

	return 0;
}


/**
 * vrrp_net_vip_set() - register VRRP virtual IPvX addresses
 */
int vrrp_net_vip_set(struct vrrp_net *vnet, const char *ip)
{
	struct vrrp_ip *vip = malloc(sizeof(struct vrrp_ip));

	if (vip == NULL) {
		log_error("vrid %d :: malloc - %m", vnet->vrid);
		return -1;
	}

	/* split ip / netmask */
	int status = -1;

	if (vnet->family == AF_INET)
		status =
		    split_ip_netmask(vnet->family, ip, &vip->ip_addr,
				     &vip->netmask);

#ifdef HAVE_IP6
	if (vnet->family == AF_INET6)
		status =
		    split_ip_netmask(vnet->family, ip, &vip->ip_addr6,
				     &vip->netmask);
#endif /* HAVE_IP6 */

	if (status != 0) {
		fprintf(stderr, "vrid %d :: invalid IP addr %s", vnet->vrid,
			ip);
		free(vip);
		return -1;
	}

	list_add_tail(&vip->iplist, &vnet->vip_list);

	return 0;
}


/**
 * vrrp_net_invalidate_buffer() 
 * invalidate internal buffer used to stock received pkt
 * vnet->__pkt
 */
static inline void vrrp_net_invalidate_buffer(struct vrrp_net *vnet)
{
	vnet->__pkt.adv.version_type = 0;
}

/**
 * vrrp_net_recv() - read and check a received VRRP pkt advertisement
 *
 * @return vrrp_pkt_t
 */
vrrp_event_t vrrp_net_recv(struct vrrp_net *vnet, const struct vrrp *vrrp)
{
	/* fetch pkt data received to buf */
	unsigned char buf[IP_MAXPACKET];

	/* read IPvX header values and fill vrrp_recv buffer __pkt */
	int payload_pos = 0;
	ssize_t len = vnet->pkt_receive(vnet->socket, &vnet->__pkt, buf,
					IP_MAXPACKET, &payload_pos);

	/* check len */
	if (len == -1) {
		log_error("vrid %d :: invalid pkt", vnet->vrid);
		return INVALID;
	}

	if ((len > vnet->vif.mtu) || (len < vnet->adv_getsize(vnet))) {
		log_error("vrid %d :: invalid pkt len", vnet->vrid);
		return INVALID;
	}

	/* read and check vrrp advertisement pkt */
	struct vrrphdr *vrrpkt;

	vrrpkt = (struct vrrphdr *) (buf + payload_pos);

	/* TODO : is this really necessary ?? */
	vrrp_net_invalidate_buffer(vnet);

	/* check VRRP pkt size (including VRRP IP address(es) and Auth data) */
	unsigned int payload_size =
	    vnet->__pkt.header.totlen - vnet->__pkt.header.len;

	if ((payload_size < VRRP_PKT_MINSIZE)
	    || (payload_size > VRRP_PKT_MAXSIZE)) {
		log_info
		    ("vrid %d :: Invalid pkt - Invalid packet size %d, expecting size between %ld and %ld",
		     vrrp->vrid, payload_size, VRRP_PKT_MINSIZE,
		     VRRP_PKT_MAXSIZE);
		return INVALID;
	}

	/* verify ip proto */
	if (vnet->__pkt.header.proto != IPPROTO_VRRP) {
		log_info("vrid %d :: Invalid pkt - ip proto not valid %d",
			 vrrp->vrid, vnet->__pkt.header.proto);
		return INVALID;
	}

	/* verify VRRP version */
	if ((vrrpkt->version_type >> 4) != vrrp->version) {
		log_info
		    ("vrid %d :: Invalid pkt - version %d mismatch, expecting %d",
		     vrrp->vrid, vrrpkt->version_type >> 4, vrrp->version);
		return INVALID;
	}

	/* TTL must be 255 */
	if (vnet->__pkt.header.ttl != VRRP_TTL) {
		log_info("vrid %d :: Invalid pkt - TTL isn't %d", vrrp->vrid,
			 VRRP_TTL);
		return INVALID;
	}

	/* check if VRID is the same as the current instance */
	if (vrrpkt->vrid != vrrp->vrid) {
		log_debug("vrid %d :: Invalid pkt - Invalid VRID %d",
			 vrrp->vrid, vrrpkt->vrid);
		return VRID_MISMATCH;
	}

	/* verify VRRP checksum */
	int chksum = vrrpkt->chksum;	/* save checksum */
	if (vnet->adv_checksum(vnet, vrrpkt, &vnet->__pkt.s_ipx,
			       &vnet->__pkt.d_ipx) != chksum) {
		log_info("vrid %d :: Invalid pkt - Invalid checksum %x",
			 vrrp->vrid, chksum);

		return INVALID;
	}
	/* restore checksum */
	vrrpkt->chksum = chksum;

	/* local router is the IP address owner
	 * (Priority equals 255) 
	 */
	if (vrrp->priority == PRIO_OWNER) {
		log_info
		    ("vrid %d :: Invalid pkt - *We* are the owner of IP address(es) (priority %d)",
		     vrrp->vrid, vrrp->priority);
		return INVALID;
	}


	/* Auth type (RFC2338/3768) */
	if (vrrp->version != RFC5798) {

		/* auth type must be the same locally configured */
		if (vrrpkt->auth_type != vrrp->auth_type) {
			log_info
			    ("vrid %d :: Invalid pkt - Invalid authentication type",
			     vrrp->vrid);
			return INVALID;
		}

		/* auth type is simple */
		if (vrrpkt->auth_type == SIMPLE) {
			uint32_t *auth_data =
			    (uint32_t *) ((unsigned char *) vrrpkt +
					  VRRP_PKTHDR_SIZE +
					  vrrp->naddr * sizeof(uint32_t));
			if (memcmp
			    (auth_data, vrrp->auth_data,
			     strlen(vrrp->auth_data))
			    != 0) {
				log_info
				    ("vrid %d :: Invalid pkt - Invalid authentication password",
				     vrrp->vrid);
				return INVALID;
			}
		}
	}

	/* count of IP address(es) and list may be the same 
	 * or generated by the owner 
	 */
	if (((vrrpkt->naddr != vrrp->naddr)
	     || (vnet->vip_compare(vnet, vrrpkt) != 0))
	    && (vrrpkt->priority != PRIO_OWNER) && (vrrp->version == 2)) {
		log_info
		    ("vrid %d :: Invalid pkt not generated by the owner, drop it",
		     vrrp->vrid);
		return INVALID;
	}

	/* advert interval must be the same as the locally configured */
	if (vrrpkt->adv_int != vrrp->adv_int) {
		log_info
		    ("vrid %d :: Invalid pkt - Advertisement interval mismatch\n",
		     vrrp->vrid);
		return INVALID;
	}

	/* pkt is valid, keep it in internal buffer */
	memcpy(&vnet->__pkt.adv, vrrpkt, sizeof(struct vrrphdr));

	return PKT;
}

/**
 * vrrp_net_send - send pkt 
 */
int vrrp_net_send(const struct vrrp_net *vnet, struct iovec *iov, size_t len)
{
	if (iov == NULL) {
		log_error("vrid %d :: No data to send !?", vnet->vrid);
		return -1;
	}

	struct sockaddr_ll device = { 0 };

	device.sll_family = AF_PACKET;
	device.sll_ifindex = if_nametoindex(vnet->vif.ifname);

	if (device.sll_ifindex == 0) {
		log_error("vrid %d :: if_nametoindex - %m", vnet->vrid);
		return -1;
	}

	struct msghdr msg = { 0 };

	msg.msg_name = &device;
	msg.msg_namelen = sizeof(device);
	msg.msg_iov = iov;
	msg.msg_iovlen = len;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	if (sendmsg(vnet->xmit, &msg, 0) < 0) {
		log_error("vrid %d :: sendmsg - %m", vnet->vrid);
		return -1;
	}

	return 0;
}
