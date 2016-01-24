/*
 * vrrp_exec.c - call an external script while changing state
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
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <spawn.h>

#include "vrrp.h"
#include "vrrp_exec.h"
#include "uvrrpd.h"
#include "common.h"
#include "log.h"

/* SCRIPT_ARG_MAX : bytes of args
 * (255 IPv6 addresses might be specified)
 * 1 IPv6 = 45 bytes in a string format
 */
#define SCRIPT_ARG_MAX INET6_ADDRSTRLEN * NI_MAXHOST
#define ADDRSTRLEN INET6_ADDRSTRLEN 
#define SCRIPT_NARGS 10

/**
 * vrrp_build_args() - prepare args that will be passed to
 *                     external script
 *
 * Build a argv array destined to execve()
 */
static int vrrp_build_args(const char *scriptname, char **argv,
			   const struct vrrp *vrrp, const struct vrrp_net *vnet,
			   vrrp_state state)
{
	/* get basename from scriptname */
	char *name = strchr(scriptname, '/');
	if (name != NULL) {
		name++;
		snprintf(argv[0], SCRIPT_ARG_MAX, "%s", name);
	}
	else
		snprintf(argv[0], SCRIPT_ARG_MAX, "%s", scriptname);

	/* List of args passed to script :
	 *  1. state
	 *  2. vrid
	 *  3. ifname
	 *  4. priority
	 *  5. adv_int
	 *  6. naddr
	 *  7. family
	 *  8 & more. vipaddrs ...
	 */
	snprintf(argv[1], SCRIPT_ARG_MAX, "%s", STR_STATE(state));
	snprintf(argv[2], SCRIPT_ARG_MAX, "%d", vrrp->vrid);
	snprintf(argv[3], SCRIPT_ARG_MAX, "%s", vnet->vif.ifname);
	snprintf(argv[4], SCRIPT_ARG_MAX, "%d", vrrp->priority);
	snprintf(argv[5], SCRIPT_ARG_MAX, "%d", vrrp->adv_int);
	snprintf(argv[6], SCRIPT_ARG_MAX, "%d", vrrp->naddr);
	snprintf(argv[7], SCRIPT_ARG_MAX, "%d",
		 (vnet->family == AF_INET ? 4 : 6));

	/* serialize vipaddrs
	 * ip0,ip1,...,ipn */
	int argv_ips = SCRIPT_NARGS - 2;
	memset(argv[argv_ips], 0, strlen(argv[argv_ips]));
	int plen = 0;
	struct vrrp_ip *vip_ptr = NULL;
	list_for_each_entry_reverse(vip_ptr, &vnet->vip_list, iplist) {
		plen = strlen(argv[argv_ips]);
		if (plen != 0)
			argv[argv_ips][plen] = ',';

		char straddr[ADDRSTRLEN];
		snprintf(argv[argv_ips] + strlen(argv[argv_ips]),
			 SCRIPT_ARG_MAX - plen + 1, "%s",
			 vnet->ipx_to_str(&vip_ptr->ipx, straddr));
	}

	/* the last elmt must be NULL */
	argv[SCRIPT_NARGS - 1] = NULL;

	return 0;
}

/**
 * vrrp_exec()
 */
int vrrp_exec(struct vrrp *vrrp, const struct vrrp_net *vnet, vrrp_state state)
{
	const char *scriptname;

	if (vrrp->scriptname == NULL)
		scriptname = VRRP_SCRIPT;
	else
		scriptname = vrrp->scriptname;

	if (!is_file_executable(scriptname)) {
		log_error("vrid %d :: File %s doesn't exist or is not executable",
			  vrrp->vrid, scriptname);
		return -1;
	}

	vrrp_build_args(scriptname, vrrp->argv, vrrp, vnet, state);

	/* Sig gestion */
	sigset_t blockmask, origmask;
	struct sigaction sa_ignore, sa_origquit, sa_origint, sa_default;

	sigemptyset(&blockmask);	/* Block SIGCHLD */
	sigaddset(&blockmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockmask, &origmask);
	sa_ignore.sa_handler = SIG_IGN;	/* Ignore SIGINT and SIGQUIT */
	sa_ignore.sa_flags = 0;
	sigemptyset(&sa_ignore.sa_mask);
	sigaction(SIGINT, &sa_ignore, &sa_origint);
	sigaction(SIGQUIT, &sa_ignore, &sa_origquit);

	/* fork */
	uvrrpd_sched_unset(); /* remove SCHED_RR */
	pid_t child = fork();
	int status, savedErrno;

	if (child == -1) {
		log_error("vrid %d :: fork - %m", vrrp->vrid);
		vrrp_exec_cleanup(vrrp);
		return -1;
	}

	/* child */
	if (child == 0) {
		sa_default.sa_handler = SIG_DFL;
		sa_default.sa_flags = 0;
		sigemptyset(&sa_default.sa_mask);
		if (sa_origint.sa_handler != SIG_IGN)
			sigaction(SIGINT, &sa_default, NULL);
		if (sa_origquit.sa_handler != SIG_IGN)
			sigaction(SIGQUIT, &sa_default, NULL);
		sigprocmask(SIG_SETMASK, &origmask, NULL);

		/* execve */
		execve(scriptname, (char *const *) vrrp->argv, NULL);

		log_error("vrid %d :: execve - %m", vrrp->vrid);
		vrrp_exec_cleanup(vrrp);
		return -1;
	}

	/* parent */
	if (child > 0) {
		
		uvrrpd_sched_set(); /* restore SCHED_RR */
		while (waitpid(child, &status, 0) == -1) {
			if (errno != EINTR) {	/* Error other than EINTR */
				log_error("vrid %d :: waitpid - %m", vrrp->vrid);
				status = -1;
				break;	/* So exit loop */
			}
		}
	}

	/* Unblock SIGCHLD, restore dispositions of SIGINT and SIGQUIT */
	savedErrno = errno;	/* The following may change 'errno' */
	sigprocmask(SIG_SETMASK, &origmask, NULL);
	sigaction(SIGINT, &sa_origint, NULL);
	sigaction(SIGQUIT, &sa_origquit, NULL);
	errno = savedErrno;

	return status;
}

/**
 * vrrp_exec_init() - init vrrp->argv buffer
 */
int vrrp_exec_init(struct vrrp *vrrp)
{
	vrrp->argv = malloc(sizeof(char *) * SCRIPT_NARGS);

	if (vrrp->argv == NULL) {
		log_error("vrid %d :: malloc - %m", vrrp->vrid);
		return -1;
	}

	for (int i = 0; i < SCRIPT_NARGS - 1; ++i) {
		vrrp->argv[i] = malloc(sizeof(char) * SCRIPT_ARG_MAX);
		if (vrrp->argv[i] == NULL) {
			log_error("vrid %d :: malloc - %m", vrrp->vrid);
			return -1;
		}
		bzero(vrrp->argv[i], sizeof(char) * SCRIPT_ARG_MAX);
	}

	return 0;
}

/**
 * vrrp_exec_cleanup() - cleanup vrrp->argv buffer
 */
void vrrp_exec_cleanup(struct vrrp *vrrp)
{
	if (vrrp->argv != NULL) {
		for (int i = 0; i < SCRIPT_NARGS - 1; ++i) {
			free(vrrp->argv[i]);
			vrrp->argv[i] = NULL;
		}
		free(vrrp->argv);
		vrrp->argv = NULL;
	}
}
