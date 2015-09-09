/*
 * vrrp.c - init VRRP instance and VRRP state functions
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
#include "vrrp_state.h"

#include "uvrrpd.h"
#include "bits.h"

#include "log.h"

extern unsigned long reg;

/**
 * vrrp_init() - init struct vrrp with default values
 */
void vrrp_init(struct vrrp *vrrp)
{
	/* VRRP version */
	vrrp->version = RFC3768;

	vrrp->vrid = 0;
	vrrp->priority = PRIO_DFL;
	vrrp->naddr = 0;

	/* auth */
	vrrp->auth_type = NOAUTH;
	vrrp->auth_data = NULL;

	/* state */
	vrrp->state = INIT;
	vrrp->preempt = PREEMPT_DFL;

	/* script */
	vrrp->scriptname = NULL;
	vrrp->argv = NULL;

	/* timers */
	vrrp->adv_int = 0;
	vrrp->master_adv_int = 0;
	vrrp_timer_clear(&vrrp->adv_timer);
	vrrp_timer_clear(&vrrp->masterdown_timer);
}

/**
 * vrrp_context() - dump vrrp info 
 */
static void vrrp_context(struct vrrp *vrrp)
{
	log_notice("====================");
	log_notice("VRID          %d", vrrp->vrid);
	log_notice("current_state %s", STR_STATE(vrrp->state));
	log_notice("adv_int       %d", vrrp->adv_int);
	if (vrrp->version == RFC5798)
		log_notice("master_adv_int      %d", vrrp->master_adv_int);
	log_notice("preempt       %s", STR_PREEMPT(vrrp->preempt));
	log_notice("naddr         %d", vrrp->naddr);
	log_notice("====================");
}


/**
 * vrrp_process() - vrrp control and state machine
 */
int vrrp_process(struct vrrp *vrrp, struct vrrp_net *vnet)
{
	switch (vrrp->state) {
	case INIT:
		vrrp_state_init(vrrp, vnet);
		break;

	case BACKUP:
		vrrp_state_backup(vrrp, vnet);
		break;

	case MASTER:
		vrrp_state_master(vrrp, vnet);
		break;

	default:
		/* invalid state */
		return -ENOSYS;
		break;
	}

	if (test_and_clear_bit(UVRRPD_DUMP, &reg))
		vrrp_context(vrrp);

	return 0;
}


/**
 * vrrp_cleanup() - clean before exiting
 */
void vrrp_cleanup(struct vrrp *vrrp)
{
	free(vrrp->scriptname);
	free(vrrp->auth_data);
}
