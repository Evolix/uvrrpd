/*
 * vrrp_ctrl.h - control fifo header
 *
 * Copyright (C) 2016 Arnaud Andre 
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

#ifndef _VRRP_CTRL_H_
#define _VRRP_CTRL_H_

#include <stdio.h>

/* from vrrp.h */
struct vrrp;
typedef enum _vrrp_event_type vrrp_event_t;

#define CTRL_MAXCHAR 		64
#define CTRL_CMD_TOKENS		3

/**
 * vrrp_ctrl - infos about control fifo
 */
struct vrrp_ctrl {
	/* control fifo fd */
	int fd;

	/* control fifo msg */
	char msg[CTRL_MAXCHAR];

	/* reformated command */
	char **cmd;
};



int vrrp_ctrl_init(struct vrrp_ctrl *ctrl);
void vrrp_ctrl_cleanup(struct vrrp_ctrl *ctrl);
vrrp_event_t vrrp_ctrl_read(struct vrrp *vrrp);


#endif /* _VRRP_CTRL_H_ */
