/*
 * vrrp_state.h - VRRP state machine
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

#ifndef _VRRP_STATE_H_
#define _VRRP_STATE_H_

/**
 * vrrp_state - VRRP states
 */
typedef enum {
	INIT,
	BACKUP,
	MASTER
} vrrp_state;


#define STR_STATE(s) (s == INIT ? "init" : \
                     ((s == BACKUP) ? "backup" : "master"))

int vrrp_state_init(struct vrrp *vrrp, struct vrrp_net *vnet);
int vrrp_state_master(struct vrrp *vrrp, struct vrrp_net *vnet);
int vrrp_state_backup(struct vrrp *vrrp, struct vrrp_net *vnet);

#endif /* _VRRP_STATE_H_ */
