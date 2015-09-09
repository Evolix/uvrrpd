/*
 * vrrp_arp.h - ARP for VRRP IPv4 advertisement
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

#ifndef _VRRP_ARP_H_
#define _VRRP_ARP_H_

int vrrp_arp_init(struct vrrp_net *vnet);
void vrrp_arp_cleanup(struct vrrp_net *vnet);
int vrrp_arp_send(struct vrrp_net *vnet);

#endif /* _VRRP_ARP_H_ */
