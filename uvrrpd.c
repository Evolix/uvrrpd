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
#include <sys/file.h>
#include <sys/mman.h>
#ifdef _POSIX_PRIORITY_SCHEDULING
  #include <sched.h>
#else
  #warning "no sched rt"
#endif

#include "uvrrpd.h"
#include "vrrp.h"
#include "vrrp_net.h"
#include "vrrp_adv.h"
#include "vrrp_arp.h"
#include "vrrp_na.h"
#include "vrrp_options.h"
#include "vrrp_exec.h"
#include "vrrp_ctrl.h"

#include "log.h"

/* global constants */
unsigned long reg = 0UL;
int background = 1;
char *loglevel = NULL;
char *pidfile_name = NULL;
char *ctrlfile_name = NULL;

/* local methods */
static void signal_handler(int sig);
static void signal_setup(void);
static int pidfile_init(int vrid);
static void pidfile_unlink(void);
static void pidfile_check(int vrid);
static void pidfile(int vrid);
static int ctrlfile_init(int vrid);
static void ctrlfile_unlink(void);
static void ctrlfile(int vrid, int *fd);

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

	/* pidfile init && check */
	if (pidfile_init(vrrp.vrid) != 0)
		exit(EXIT_FAILURE);

	pidfile_check(vrrp.vrid);

	/* logs */
	log_open("uvrrpd", (char const *) loglevel);

	/* init and open control file fifo */
	ctrlfile_init(vrrp.vrid);
	ctrlfile(vrrp.vrid, &vrrp.ctrl.fd);
	if (vrrp_ctrl_init(&vrrp.ctrl) != 0)
		exit(EXIT_FAILURE);

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


	/* pidfile */
	pidfile(vrrp.vrid);

	/* lock procress's virtual address space into RAM */
	mlockall(MCL_CURRENT | MCL_FUTURE);
	/* set SCHED_RR */
	uvrrpd_sched_set();

	/* process */
	set_bit(KEEP_GOING, &reg);
	while (test_bit(KEEP_GOING, &reg) && !vrrp_process(&vrrp, &vnet));

	/* shutdown */
	vrrp_adv_cleanup(&vnet);

	if (vnet.family == AF_INET)
		vrrp_arp_cleanup(&vnet);
	else	/* AF_INET6 */
		vrrp_na_cleanup(&vnet);

	vrrp_cleanup(&vrrp);
	vrrp_exec_cleanup(&vrrp);
	vrrp_ctrl_cleanup(&vrrp.ctrl);
	vrrp_net_cleanup(&vnet);

	log_close();
	free(loglevel);
	pidfile_unlink();
	ctrlfile_unlink();
	free(pidfile_name);

	munlockall();

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

/**
 * pidfile_init() 
 */
static int pidfile_init(int vrid)
{
	int max_len = NAME_MAX + PATH_MAX;
	if (pidfile_name == NULL) {
		pidfile_name = malloc(max_len);
		if (pidfile_name == NULL) {
			log_error("vrid %d :: malloc - %m", vrid);
			return -1;
		}

		snprintf(pidfile_name, max_len, PIDFILE_NAME, vrid);
	}

	return 0;
}

/**
 * pidfile_unlink() - remove pidfile
 */
static void pidfile_unlink(void)
{
	if (pidfile_name)
		unlink(pidfile_name);
}

/**
 * pidfile_check()
 */
static void pidfile_check(int vrid)
{
	struct flock fl;
	int err, fd;

	fd = open(pidfile_name, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (errno == ENOENT)
			return;
		fprintf(stderr, "vrid %d :: error opening PID file %s: %m\n",
			vrid, pidfile_name);
		exit(EXIT_FAILURE);
	}

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	err = fcntl(fd, F_GETLK, &fl);
	close(fd);
	if (err < 0) {
		fprintf(stderr, "vrid %d :: error getting PID file %s lock: %m",
			vrid, pidfile_name);
		exit(EXIT_FAILURE);
	}

	if (fl.l_type == F_UNLCK)
		return;

	fprintf(stderr, "vrid %d :: uvrrpd is already running (pid %d)\n", vrid,
		(int) fl.l_pid);
	exit(EXIT_FAILURE);
}

/**
 * pid_file()
 */
static void pidfile(int vrid)
{
	struct flock fl;
	char buf[16];
	int err, fd;

	fd = open(pidfile_name,
		  O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		log_error("vrid %d :: error opening PID file %s: %m", vrid,
			  pidfile_name);
		exit(EXIT_FAILURE);
	}

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;

	err = fcntl(fd, F_SETLK, &fl);
	if (err < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			log_error("vrid %d :: uvrrpd is already running",
				  vrid);
			exit(EXIT_FAILURE);
		}
		log_error("vrid %d :: error setting PID file %s lock: %m",
			  vrid, pidfile_name);
		exit(EXIT_FAILURE);
	}

	atexit(pidfile_unlink);

	err = snprintf(buf, sizeof(buf), "%d\n", (int) getpid());
	if (err < 0) {
		perror("snprintf");
		exit(EXIT_FAILURE);
	}

	err = write(fd, buf, err);
	if (err < 0) {
		log_error("vrid %d :: error writing PID to PID file %s: %m",
			  vrid, pidfile_name);
		exit(EXIT_FAILURE);
	}
}


/**
 * ctrlfile_init()
 */
static int ctrlfile_init(int vrid)
{
	int max_len = NAME_MAX + PATH_MAX;
	if (ctrlfile_name == NULL) {
		ctrlfile_name = malloc(max_len);
		if (ctrlfile_name == NULL) {
			log_error("vrid %d :: malloc - %m", vrid);
			return -1;
		}

		snprintf(ctrlfile_name, max_len, CTRLFILE_NAME, vrid);
	}

	return 0;
}

/**
 * ctrlfile_unlink()
 */
static void ctrlfile_unlink()
{
	if (ctrlfile_name)
		unlink(ctrlfile_name);
}

/**
 * ctrlfile()
 */
static void ctrlfile(int vrid, int *fd)
{
	if (fd == NULL) {
		log_error("vrid %d :: invalid use of ctrlfile(), fd NULL", vrid);
		exit(EXIT_FAILURE);
	}

	ctrlfile_unlink();
	if (mkfifo(ctrlfile_name, 0600) != 0) {
		log_error("vrid %d :: error while creating control fifo %s: %m", vrid,
			  ctrlfile_name);
		exit(EXIT_FAILURE);
	}

	atexit(ctrlfile_unlink);

	*fd = open(ctrlfile_name, O_RDWR | O_NONBLOCK);
	if (*fd == -1) {
		log_error("vrid %d :: error while opening control fifo %s: %m", vrid,
			  ctrlfile_name);
		exit(EXIT_FAILURE);
	}
}

/**
 * uvrrpd_sched_set() - set SCHED_RR scheduler
 */
int uvrrpd_sched_set()
{
#ifdef _POSIX_PRIORITY_SCHEDULING
	struct sched_param param;

	param.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &param) != 0) {
		log_error("sched_setscheduler() - %m");
		return -1;
	}
#else
	nice(-20);
#endif

	return 0;
}

/**
 * uvrrpd_sched_unset() - unset SCHED_RR scheduler
 */
int uvrrpd_sched_unset()
{
#ifdef _POSIX_PRIORITY_SCHEDULING
	struct sched_param param;

	param.sched_priority = sched_get_priority_max(SCHED_OTHER);
	if (sched_setscheduler(0, SCHED_OTHER, &param) != 0) {
		log_error("sched_setscheduler() - %m");
		return -1;
	}
#endif
	return 0;
}
