/*
 * vrrp_adv.h - VRRP advertisement
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

#ifndef _VRRP_ADV_H_
#define _VRRP_ADV_H_


int vrrp_adv_init(struct vrrp_net *vnet, const struct vrrp *vrrp);
void vrrp_adv_cleanup(struct vrrp_net *vnet);
int vrrp_adv_send(struct vrrp_net *vnet);
int vrrp_adv_send_zero(struct vrrp_net *vnet);
uint16_t vrrp_adv_chksum(struct vrrp_net *vnet, struct vrrphdr *pkt,
			 uint32_t saddr, uint32_t daddr);

uint16_t vrrp_adv_ip6_chksum(struct vrrp_net *vnet, struct vrrphdr *pkt,
			     struct in6_addr *saddr, struct in6_addr *daddr);

/**
 * vrrp_adv_get_version() - get version_type from received adv pkt
 */
static inline int vrrp_adv_get_version(const struct vrrp_net *vnet)
{
	return vnet->__pkt.adv.version_type;
}

/**
 * vrrp_adv_get_priority() - get priority from received adv priority
 */
static inline int vrrp_adv_get_priority(const struct vrrp_net *vnet)
{
	return vnet->__pkt.adv.priority;
}

/**
 * vrrp_adv_addr_to_str() - return source ip from received adv pkt
 * 		            in string format
 */
static inline const char *vrrp_adv_addr_to_str(struct vrrp_net *vnet, char *dst)
{
	return vnet->ipx_to_str(&vnet->__pkt.s_ipx, dst);
}

/**
 * vrrp_adv_get_compare() - compare received adv pkt addr to local 
 * 			    primary address
 */
static inline int vrrp_adv_addr_cmp(struct vrrp_net *vnet)
{
	return vnet->ipx_cmp(&vnet->__pkt.s_ipx, &vnet->vif.ipx);
}

/**
 * vrrp_adv_get_advint() - get adv interval
 */
static inline uint16_t vrrp_adv_get_advint(const struct vrrp_net *vnet)
{
	if (vnet->__pkt.adv.version_type >> 4 == RFC5798)
		return ntohs(vnet->__pkt.adv.max_adv_int);

	return vnet->__pkt.adv.adv_int;

}
#endif /* _VRRP_ADV_H_ */
