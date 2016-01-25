/*
 * vrrp_na.h - Neighbour Advertisement
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

#ifndef _VRRP_NA_H_
#define _VRRP_NA_H_

int vrrp_na_init(struct vrrp_net *vnet);
void vrrp_na_cleanup(struct vrrp_net *vnet);
int vrrp_na_send(struct vrrp_net *vnet);

#endif /* _VRRP_NA_H_ */

#endif /* HAVE_IP6 */
