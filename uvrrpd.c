/*
 * uvrrpd.c - main entry point, server initialization
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
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "uvrrpd.h"
#include "vrrp.h"
#include "vrrp_net.h"
#include "vrrp_adv.h"
#include "vrrp_arp.h"
#include "vrrp_na.h"
#include "vrrp_options.h"
#include "vrrp_exec.h"

#include "log.h"

unsigned long reg = 0UL;
int background = 1;
char *loglevel = NULL;

static void signal_handler(int sig);
static void signal_setup(void);

/**
 * main() - entry point
 *
 * Declare VRRP instance, init daemon
 * and launch state machine
 */
int main(int argc, char *argv[])
{
	signal_setup();

	/* Current VRRP instance */
	struct vrrp vrrp;
	struct vrrp_net vnet;

	/* Init VRRP instance */
	vrrp_init(&vrrp);
	vrrp_net_init(&vnet);

	/* cmdline options */
	if (! !vrrp_options(&vrrp, &vnet, argc, argv))
		exit(EXIT_FAILURE);

	/* logs */
	log_open("uvrrpd", (char const *) loglevel);

	/* open sockets */
	if ((vrrp_net_socket(&vnet) != 0) || (vrrp_net_socket_xmit(&vnet) != 0))
		exit(EXIT_FAILURE);

	/* hook script */
	if (vrrp_exec_init(&vrrp) != 0)
		exit(EXIT_FAILURE);

	/* advertisement pkt */
	if (vrrp_adv_init(&vnet, &vrrp) != 0)
		exit(EXIT_FAILURE);

	/* net topology */
	if (vnet.family == AF_INET) {
		if (vrrp_arp_init(&vnet) != 0)
			exit(EXIT_FAILURE);
	}
	else if (vnet.family == AF_INET6) {
		if (vrrp_na_init(&vnet) != 0)
			exit(EXIT_FAILURE);
	}

	/* daemonize */
	if (background) {
		daemon(0, (log_trigger(NULL) > LOG_INFO));
	}
	else
		chdir("/");

	/* process */
	set_bit(KEEP_GOING, &reg);
	while (test_bit(KEEP_GOING, &reg) && !vrrp_process(&vrrp, &vnet));

	/* shutdown */
	vrrp_adv_cleanup(&vnet);

	if (vnet.family == AF_INET)
		vrrp_arp_cleanup(&vnet);

	vrrp_cleanup(&vrrp);
	vrrp_exec_cleanup(&vrrp);
	vrrp_net_cleanup(&vnet);

	log_close();
	free(loglevel);

	return EXIT_SUCCESS;
}


/**
 * signal_handler - Signal handler 
 */
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		log_notice("HUP to the init state");
		set_bit(UVRRPD_RELOAD, &reg);
		break;

	case SIGUSR1:
	case SIGUSR2:
		set_bit(UVRRPD_DUMP, &reg);
		break;

	case SIGPIPE:
		log_notice("this is not a SIGPIPE");
		set_bit(UVRRPD_LOGOUT, &reg);
		break;

	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		log_notice("%s - exit daemon", strsignal(sig));
		set_bit(UVRRPD_RELOAD, &reg);
		clear_bit(KEEP_GOING, &reg);
		break;

	case SIGCHLD:
		/* bleh */
		break;

	default:
		log_error("%s %d", strsignal(sig), sig);
		break;
	}
}

/**
 * signal_setup
 *    - register signal handler
 *    - SIGTERM: shutdown daemon
 *    - SIGHUP:  reload daemon (switch to init state)
 *    - SIGCHLD: notify end of task (vrrp_exec())
 *    - SIGUSR1: logs daemon context: vrrp_context()
 *    - SIGUSR2: todo, same as USR1 for the moment
 *    - SIGPIPE: socket write failure
 *
 *   - blocked signal, unblocked them on select() syscall vrrp_process()
 */
static void signal_setup(void)
{
	struct sigaction sa;

	/* setup signal */
	memset(&sa, 0x00, sizeof(sa));
	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);

	/* setup signal mask */
	sigemptyset(&sa.sa_mask);

	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGQUIT);
	sigaddset(&sa.sa_mask, SIGHUP);
	sigaddset(&sa.sa_mask, SIGUSR1);
	sigaddset(&sa.sa_mask, SIGUSR2);

	sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);
}
