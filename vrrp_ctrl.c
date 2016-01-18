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

#include "vrrp_ctrl.h"
#include "vrrp.h"
#include "common.h"

#include "uvrrpd.h"
#include "bits.h"

#include "log.h"

extern unsigned long reg;


static inline vrrp_event_t vrrp_ctrl_cmd(struct vrrp *vrrp);

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
	ctrl->cmd = malloc(sizeof(char *) * CTRL_CMD_TOKEN);

	if (ctrl->cmd == NULL) {
		log_error("init :: malloc - %m");
		return -1;
	}

	bzero(ctrl->msg, CTRL_MAXCHAR);

	return 0;
}


/**
 * vrrp_ctrl_cmd() - Interprete control fifo cmd
 */
static inline vrrp_event_t vrrp_ctrl_cmd(struct vrrp *vrrp)
{
	int nword;

	nword =
	    split_cmd(vrrp->ctrl.msg, vrrp->ctrl.cmd, CTRL_CMD_TOKEN,
		      WHITESPACE);

	if (nword == 0)
		return INVALID;


	if (matches(vrrp->ctrl.cmd[0], "stop")) {
		log_notice("vrid %d :: control cmd stop, exiting", vrrp->vrid);
		set_bit(UVRRPD_RELOAD, &reg);
		clear_bit(KEEP_GOING, &reg);
		return CTRL_FIFO;
	}

	if (matches(vrrp->ctrl.cmd[0], "reload")) {
		set_bit(UVRRPD_RELOAD, &reg);
		return CTRL_FIFO;
	}

	if (matches(vrrp->ctrl.cmd[0], "state")
	    || matches(vrrp->ctrl.cmd[0], "status")) {

		set_bit(UVRRPD_DUMP, &reg);
		/* dump state in logs */
	}

	if (matches(vrrp->ctrl.cmd[0], "prio")) {
		if (nword != 2) {
			/* error */
			return INVALID;
		}

		/* change prio */
		// todo

		/* reload bit */
		set_bit(UVRRPD_RELOAD, &reg);
		return CTRL_FIFO;
	}

	return INVALID;
}

/**
 * vrrp_ctrl_read() - Read control fifo
 */
vrrp_event_t vrrp_ctrl_read(struct vrrp * vrrp)
{
	int readbytes = 0;

	readbytes = read(vrrp->ctrl.fd, vrrp->ctrl.msg, CTRL_MAXCHAR);

	if (readbytes > 0) {
		flush_fifo(vrrp->ctrl.fd);
		vrrp->ctrl.msg[CTRL_MAXCHAR - 1] = '\0';
		return vrrp_ctrl_cmd(vrrp);
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
