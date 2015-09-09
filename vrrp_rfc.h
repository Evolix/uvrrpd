/*
 * vrrp_rfc.h
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

#ifndef _VRRP_RFC_H_
#define _VRRP_RFC_H_

#include <stdint.h>

/* 
 * vrrphdr
 * Header structure for rfc3768 et rfc5798 
 */

struct vrrphdr {
	uint8_t version_type;	/* 0-3=version, 4-7=type */
	uint8_t vrid;
	uint8_t priority;
	uint8_t naddr;
	union {

		/* rfc 3768 */
		struct {
			uint8_t auth_type;
			uint8_t adv_int;
		};

		/* rfc 5798 */
		uint16_t max_adv_int;	/* 0-3=rsvd, 4-5=adv_int */
	};
	uint16_t chksum;

	/* virtual IPs start here */

} __attribute__ ((packed));

#endif /* _VRRP_RFC_H_ */
