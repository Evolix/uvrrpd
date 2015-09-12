/*
 * vrrp_state.c 
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

#include "vrrp.h"
#include "vrrp_net.h"
#include "vrrp_adv.h"
#include "vrrp_arp.h"
#include "vrrp_na.h"
#include "vrrp_exec.h"

#include "log.h"
#include "bits.h"
#include "uvrrpd.h"

extern unsigned long reg;

/**
 * switching state functions
 */
static int vrrp_state_goto_master(struct vrrp *vrrp, struct vrrp_net *vnet);
static int vrrp_state_goto_backup(struct vrrp *vrrp, struct vrrp_net *vnet);

/**
 * vrrp_state_init() - Initial state of VRRP instance
 *
 * Switch to master or backup state
 */
int vrrp_state_init(struct vrrp *vrrp, struct vrrp_net *vnet)
{
	log_notice("vrid %d :: %s", vrrp->vrid, "init");

	/* init Master_Adver_Interval */
	vrrp->master_adv_int = vrrp->adv_int;
	log_debug("%d", vrrp->master_adv_int);

	/* router owns ip address(es) */
	if (vrrp->priority == 255) {
		log_debug("%s %d :%s", "priority", vrrp->priority,
			  "router owns VIP address(es)");
		return vrrp_state_goto_master(vrrp, vnet);
	}

	return vrrp_state_goto_backup(vrrp, vnet);
}

/**
 * vrrp_state_backup() - handle backup state
 */
int vrrp_state_backup(struct vrrp *vrrp, struct vrrp_net *vnet)
{
	int event = vrrp_net_listen(vnet, vrrp);
	char straddr[INET6_ADDRSTRLEN];

	switch (event) {
	case TIMER:
		log_notice("vrid %d :: %s", vrrp->vrid,
			   "masterdown_timer expired");

		vrrp_state_goto_master(vrrp, vnet);
		break;

	case PKT:	/* valid PKT adv */
		/* Must be impossible */
		if (vrrp_adv_get_version(vnet) == 0) {
			log_error("vrid %d :: %s", vrrp->vrid,
				  "recv buffer empty !?");
			break;
		}

		log_debug("vrid %d :: %s", vrrp->vrid,
			  "pkt received from current master");
		log_debug("vrid %d :: %s:%d %s:%d %s:%s", vrrp->vrid, "prio",
			  vrrp->priority, "prio recv",
			  vrrp_adv_get_priority(vnet), "preempt",
			  STR_PREEMPT(vrrp->preempt));

		/* Priority of received pkt is 0 
		 * => set Master_Down_Timer to skew_time
		 */
		if (vrrp_adv_get_priority(vnet) == 0) {
			log_info("vrid %d :: %s", vrrp->vrid,
				 "receive packet with priority 0");
			log_notice("vrid %d :: %s %d", vrrp->vrid,
				   "set masterdown_timer to skew_time",
				   SKEW_TIME(vrrp));

			VRRP_SET_SKEW_TIME(vrrp);
			break;
		}

		/* Master has send its adv pkt, or preemption mode is false
		 */
		if ((vrrp->preempt == FALSE) ||
		    (vrrp_adv_get_priority(vnet) >= vrrp->priority)) {

			/* RFC5798: Set Master_Adver_Interval to 
			 * Advertisement_Interval
			 */
			if (vrrp->version == RFC5798)
				vrrp->master_adv_int =
				    vrrp_adv_get_advint(vnet);

#ifdef DEBUG
			print_buf_hexa("hexa master_adv_int",
				       &vrrp->master_adv_int, sizeof(uint16_t));
#endif



			VRRP_SET_MASTERDOWN_TIMER(vrrp);
			break;
		}

		/* Our priority is greater and preemption mode is true
		 * So we discard advertisement.
		 * Maybe we'll become Master if Master_Down_Timer expire,
		 */
		log_info("vrid %d :: %s", vrrp->vrid, "discard advertisement");

		break;

	case SIGNAL:
		log_debug("vrid %d :: signal", vrrp->vrid);

		/* shutdown/reload event ? */
		if (test_and_clear_bit(UVRRPD_RELOAD, &reg)) {
			vrrp_timer_clear(&vrrp->masterdown_timer);
			vrrp->state = INIT;
		}

		break;

	case INVALID:
		log_warning("vrid %d :: %s %s, %s", vrrp->vrid,
			    "receive an invalid advertisement packet from",
			    vrrp_adv_addr_to_str(vnet, straddr), "ignore it");

		break;

	default:
		log_error("vrid %d :: %s r:%d", vrrp->vrid, "unknown event",
			  event);
		break;
	}

	return event;
}


/**
 * vrrp_state_master() - handle master state
 */
int vrrp_state_master(struct vrrp *vrrp, struct vrrp_net *vnet)
{
	int event = vrrp_net_listen(vnet, vrrp);

	switch (event) {
	case TIMER:
		/* adv_timer expired, time to send another */
		log_info("vrid %d :: %s", vrrp->vrid, "adv_timer expired");
		vrrp_adv_send(vnet);
		VRRP_SET_ADV_TIMER(vrrp);
		break;

	case PKT:
		if (vrrp_adv_get_version(vnet) == 0) {
			log_error("vrid %d :: %s", vrrp->vrid,
				  "recv buffer empty !?");
			break;
		}

		log_debug("vrid %d :: %s:%d %s:%d %s:%s", vrrp->vrid, "prio",
			  vrrp->priority, "prio recv",
			  vrrp_adv_get_priority(vnet), "preempt",
			  STR_PREEMPT(vrrp->preempt));

		/* Priority of received pkt is 0
		 * We send an advertisement 
		 * and rearm Adv_timer
		 */
		if (vrrp_adv_get_priority(vnet) == 0) {
			log_info("vrid %d :: %s", vrrp->vrid,
				 "receive packet with priority 0");
			vrrp_adv_send(vnet);
			VRRP_SET_ADV_TIMER(vrrp);
			break;
		}

		/* Priority of received pkt is greater than our,
		 * switch to backup
		 */
		if (vrrp_adv_get_priority(vnet) > vrrp->priority) {

			log_notice("vrid %d :: %s", vrrp->vrid,
				   "receive packet with higher priority");

			vrrp_state_goto_backup(vrrp, vnet);
			break;
		}

		/* Priority of received pkt is equal, but 
		 * primary IP address of the sender is greater than 
		 * the local primary IP address,
		 * switch to backup
		 */
		if (vrrp_adv_get_priority(vnet) == vrrp->priority) {
			if (vrrp_adv_addr_cmp(vnet) > 0) {
				log_notice("vrid %d :: %s", vrrp->vrid,
					   "Same priority !");
				log_notice("vrid %d :: %s", vrrp->vrid,
				           "Primary IP address of the sender greater than the local primary address");

				vrrp_state_goto_backup(vrrp, vnet);
				break;
			}

			log_notice("vrid %d :: %s", vrrp->vrid,
				   "Same priority !");
			log_notice("vrid %d :: %s", vrrp->vrid,
				   "Local primary address is greater than the primary IP address of the sender");

		}

		/* We have the greatest priority */
		log_info("vrid %d :: %s", vrrp->vrid, "discard advertisement");

		break;

	case SIGNAL:
		log_debug("vrid %d :: signal", vrrp->vrid);

		/* shutdown/reload event ? */
		if (test_and_clear_bit(UVRRPD_RELOAD, &reg)) {
			vrrp_timer_clear(&vrrp->adv_timer);
			vrrp_adv_send_zero(vnet);
			/* berk */
			vrrp_exec(vrrp, vnet, BACKUP);
			vrrp->state = INIT;
		}

		break;

	case INVALID:
		log_warning("vrid %d :: invalid event", vrrp->vrid);
		break;

	default:
		log_error("vrid %d :: %s r:%d", vrrp->vrid, "unknown event",
			  event);
		break;
	}

	return event;
}


/**
 * vrrp_state_goto_master() - switch state to master
 */
static int vrrp_state_goto_master(struct vrrp *vrrp, struct vrrp_net *vnet)
{
	log_notice("vrid %d :: %s -> %s", vrrp->vrid,
		   STR_STATE(vrrp->state), "master");

	vrrp->state = MASTER;

	/* IPv4 specific */
	vrrp_adv_send(vnet);

	if (vnet->family == AF_INET)
		vrrp_arp_send(vnet);
	else if (vnet->family == AF_INET6)
		vrrp_na_send(vnet);

	/* script */
	vrrp_exec(vrrp, vnet, vrrp->state);

	/* reset masterdown_timer && set ADV timer */
	vrrp_timer_clear(&vrrp->masterdown_timer);
	VRRP_SET_ADV_TIMER(vrrp);

	return 0;
}


/**
 * vrrp_state_goto_backup() - switch state to backup
 */
static int vrrp_state_goto_backup(struct vrrp *vrrp, struct vrrp_net *vnet)
{
	log_notice("vrid %d :: %s -> %s", vrrp->vrid,
		   STR_STATE(vrrp->state), "backup");

	int previous_state = vrrp->state;
	vrrp->state = BACKUP;

	log_debug("%s:%s", STR_STATE(previous_state), STR_STATE(vrrp->state));

	/* script */
	if (previous_state != INIT)
		vrrp_exec(vrrp, vnet, vrrp->state);

	if (vrrp->version == RFC5798) {
		if (previous_state == INIT)
			vrrp->master_adv_int = vrrp->adv_int;
		else if (previous_state == MASTER)
			vrrp->master_adv_int = vrrp_adv_get_advint(vnet);
	}

	/* clear adv timer && set masterdown_timer */
	vrrp_timer_clear(&vrrp->adv_timer);
	VRRP_SET_MASTERDOWN_TIMER(vrrp);

	log_debug("%d %d", vrrp->master_adv_int,
		  3 * vrrp->master_adv_int + SKEW_TIME(vrrp));
	return 0;
}
