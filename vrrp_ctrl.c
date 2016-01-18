/*
 * vrrp_ctrl.c - control fifo 
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


#include <sys/types.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "vrrp.h"
#include "vrrp_ctrl.h"
#include "vrrp_adv.h"

#include "common.h"
#include "uvrrpd.h"
#include "bits.h"
#include "log.h"

extern unsigned long reg;

static inline vrrp_event_t vrrp_ctrl_cmd(struct vrrp *vrrp,
					 struct vrrp_net *vnet);

/**
 * flush_fifo() - flush a fifo fd
 */
#define BUFLUSH		2048

static inline int flush_fifo(int fd)
{
	ssize_t bytes;
	char buf[BUFLUSH];

	while (1) {
		bytes = read(fd, buf, sizeof(buf));
		if (bytes <= 0) {
			if (errno == EWOULDBLOCK) {
				return 0;
			}
			else {
				log_error("read - %m");
				return -1;
			}
		}
	}
	return 0;
}

/**
 * split_cmd() - split a *str in words separated by delim
 * Fill a *words_ptr[max_words] with ptr to each found word
 * words_ptr must be pre-allocated
 * @return nword, number of found word,
 * 	   -1 if entry str or words_ptr NULL
 */
static inline int split_cmd(char *str, char **words_ptr, int max_words,
			    char *delim)
{
	if ((str == NULL) || (words_ptr == NULL))
		return -1;

	int nword;
	for (nword = 0; nword < max_words; ++nword) {
		if (str != NULL) {
			while (isspace(*str))
				str++;	/* trim whitespace */
			if (str[0] != '\0')
				words_ptr[nword] = strsep(&str, delim);
		}

		if (words_ptr[nword] == NULL)
			break;
	}


	return nword;
}

/**
 * vrrp_ctrl_init() 
 */
int vrrp_ctrl_init(struct vrrp_ctrl *ctrl)
{
	ctrl->cmd = malloc(sizeof(char *) * CTRL_CMD_NTOKEN);

	if (ctrl->cmd == NULL) {
		log_error("init :: malloc - %m");
		return -1;
	}

	bzero(ctrl->msg, CTRL_MAXCHAR);

	return 0;
}


/**
 * vrrp_ctrl_cmd_flush() - Flush cmd
 */
static inline void vrrp_ctrl_cmd_flush(struct vrrp_ctrl *ctrl)
{
	if (ctrl == NULL)
		return;

	/* clean buff */
	for (int i = 0; i < CTRL_CMD_NTOKEN; ctrl->cmd[i++] = NULL);
	bzero(ctrl->msg, CTRL_MAXCHAR);
}

/**
 * vrrp_ctrl_cmd() - Interprete control fifo cmd
 */
static inline vrrp_event_t vrrp_ctrl_cmd(struct vrrp *vrrp,
					 struct vrrp_net *vnet)
{
	int nword;

	nword =
	    split_cmd(vrrp->ctrl.msg, vrrp->ctrl.cmd, CTRL_CMD_TOKEN,
		      WHITESPACE);

	if (nword == 0)
		return INVALID;


	/* 
	 * control cmd stop 
	 */
	if (matches(vrrp->ctrl.cmd[0], "stop")) {
		log_notice("vrid %d :: control cmd stop, exiting", vrrp->vrid);
		set_bit(UVRRPD_RELOAD, &reg);
		clear_bit(KEEP_GOING, &reg);
		vrrp_ctrl_cmd_flush(&vrrp->ctrl);
		return CTRL_FIFO;
	}

	/* 
	 * control cmd reload 
	 */
	if (matches(vrrp->ctrl.cmd[0], "reload")) {
		set_bit(UVRRPD_RELOAD, &reg);
		vrrp_ctrl_cmd_flush(&vrrp->ctrl);
		return CTRL_FIFO;
	}

	/* 
	 * control cmd state || status 
	 */
	if (matches(vrrp->ctrl.cmd[0], "state")
	    || matches(vrrp->ctrl.cmd[0], "status")) {

		set_bit(UVRRPD_DUMP, &reg);
		vrrp_ctrl_cmd_flush(&vrrp->ctrl);
	}

	/* 
	 * control cmd prio 
	 */
	if (matches(vrrp->ctrl.cmd[0], "prio")) {
		if (nword != 2) {
			log_error
			    ("vrid %d :: invalid syntax, control cmd prio <priority>",
			     vrrp->vrid);
			return INVALID;
		}

		/* fetch priority */
		int err;
		unsigned long opt;
		err = mystrtoul(&opt, vrrp->ctrl.cmd[1], VRRP_PRIO_MAX);

		vrrp_ctrl_cmd_flush(&vrrp->ctrl);

		if (err == -ERANGE) {
			log_error
			    ("vrid %d :: invalid control cmd prio, 0 < priority < 255",
			     vrrp->vrid);
			return INVALID;
		}

		if (err == -EINVAL) {
			log_error
			    ("vrid %d :: invalid control cmd prio, error parsing \"%s\" as a number",
			     vrrp->vrid, vrrp->ctrl.cmd[1]);
			return INVALID;
		}

		vrrp->priority = (uint8_t) opt;

		/* change prio */
		vrrp_adv_set_priority(vnet, vrrp->priority);

		/* reload bit */
		set_bit(UVRRPD_RELOAD, &reg);

		return CTRL_FIFO;
	}

	return INVALID;
}

/**
 * vrrp_ctrl_read() - Read control fifo
 */
vrrp_event_t vrrp_ctrl_read(struct vrrp * vrrp, struct vrrp_net * vnet)
{
	int readbytes = 0;

	readbytes = read(vrrp->ctrl.fd, vrrp->ctrl.msg, CTRL_MAXCHAR);

	if (readbytes > 0) {
		flush_fifo(vrrp->ctrl.fd);
		vrrp->ctrl.msg[CTRL_MAXCHAR - 1] = '\0';
		return vrrp_ctrl_cmd(vrrp, vnet);
	}

	return INVALID;
}


/**
 * vrrp_ctrl_cleanup()
 */
void vrrp_ctrl_cleanup(struct vrrp_ctrl *ctrl)
{
	free(ctrl->cmd);
}
