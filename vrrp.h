/*
 * vrrp.h - define main struct vrrp (VRRP instance) and some 
 *          constants
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

#ifndef _VRRP_H_
#define _VRRP_H_

#include <stdint.h>

#include "common.h"
#include "vrrp_net.h"
#include "vrrp_timer.h"
#include "vrrp_state.h"

/* MAX values */
#define VRID_MAX        255
#define VRRP_PRIO_MAX   255
#define ADVINT_MAX      4095	/* RFC5798 */
#define PRIO_OWNER      VRRP_PRIO_MAX

/* DEFAULT values */
#define PREEMPT_DFL     TRUE
#define PRIO_DFL        100

/* External script */
#define VRRP_SCRIPT    stringify(PATH) "/vrrp_switch.sh"
#define VRRP_SCRIPT_MAX sysconf(_SC_ARG_MAX)

/* preemption */
#define STR_PREEMPT(s) (s == TRUE ? "true" : "false")

/**
 * vrrp_version - VRRP version protocol
 * @RFC2338 : VRRPv2 (deprecated)
 * @RFC3768 : VRRPv2 
 * @RFC5798 : VRRPv3
 */
typedef enum {
	RFC2338 = 2,
	RFC3768 = 2,
	RFC5798 = 3
} vrrp_version;

/**
 * vrrp_authtype - Authentication type 
 * (from VRRPv2 / rfc2332)
 */
#define VRRP_AUTH_PASS_LEN 8
typedef enum {
	NOAUTH,
	SIMPLE,
	HMAC			/* not supported */
} vrrp_authtype;

/**
 * vrrp - Main structure defining VRRP instance
 */
struct vrrp {
	vrrp_version version;	/* VRRP version */
	uint8_t vrid;		/* VRID 1 - 255 */
	uint8_t priority;	/* PRIO 0 - 255 */
	uint8_t naddr;		/* count ip addresses */

	/* Advertisement interval 
	 *
	 * VRRPv2 :
	 * - delay in s
	 * - default 1s
	 * - 8 bits field
	 *
	 * VRRPv3 : 
	 * - delay in centisecond
	 * - default 100cs (1s)
	 * - 12 bits field
	 */
	uint16_t adv_int;

	/* Master advertisement interval
	 * only in VRRPv3 / rfc5798
	 */
	uint16_t master_adv_int;

	vrrp_authtype auth_type;
	char *auth_data;

	vrrp_state state;
	bool preempt;

	char *scriptname;
	char **argv;

	struct vrrp_timer adv_timer;
	struct vrrp_timer masterdown_timer;
};

/* funcs */
void vrrp_init(struct vrrp *vrrp);
void vrrp_cleanup(struct vrrp *vrrp);
int vrrp_process(struct vrrp *vrrp, struct vrrp_net *vnet);

#endif /* _VRRP_H_ */
