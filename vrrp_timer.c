/*
 * vrrp_timer.c - functions manipulating VRRP timers
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
 *
 *
 * These functions use clock_gettime() and CLOCK_MONOTONIC_RAW
 * which access to a raw hardware-based time that is not subject to
 * NTP adjustements.
 */

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "vrrp_timer.h"
#include "log.h"

/* 1s for timespec operations */
#define NANOUL  1000000000
#define CENTUL  10000000

/**
 * timespec_substract() - substract two timestamp
 *
 * Subtract the `struct timespec' values X and Y,
 * The difference is stored in timespect result.
 * @return 1 if the difference is negative, otherwise 0.
 */
static inline int timespec_substract(struct timespec *result,
				     struct timespec *x, struct timespec *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_nsec < y->tv_nsec) {
		time_t nsec = (y->tv_nsec - x->tv_nsec) / NANOUL + 1;
		y->tv_nsec -= NANOUL * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_nsec - y->tv_nsec > NANOUL) {
		time_t nsec = (x->tv_nsec - y->tv_nsec) / NANOUL;
		y->tv_nsec += NANOUL * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	   tv_nsec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_nsec = x->tv_nsec - y->tv_nsec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

/**
 * vrrp_timer_set() - set timer and reset delta
 *
 * @delay
 * @return -1 if clock_gettime() fail, 0 else
 */
int vrrp_timer_set(struct vrrp_timer *timer, time_t delay, long delay_cs)
{
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &timer->ts) == -1) {
		log_error("clock_gettime: %s", strerror(errno));
		return -1;
	}

	timer->ts.tv_sec += delay;
	timer->ts.tv_nsec += delay_cs * CENTUL;

#ifdef DEBUG
	log_debug("delay %ld", delay);
	log_debug("delay_cs %ld", delay_cs);

	log_debug("timer->ts.tv_sec %ld", timer->ts.tv_sec);
	log_debug("timer->ts.tv_nsec %ld", timer->ts.tv_nsec);
#endif /* DEBUG */

	/* reset delta */
	timer->delta.tv_sec = 0;
	timer->delta.tv_nsec = 0;

	return 0;
}

/**
 * vrrp_timer_clear() - clear (reset) timer
 */
void vrrp_timer_clear(struct vrrp_timer *timer)
{
	if (timer == NULL)
		return;

	timer->ts.tv_sec = 0;
	timer->ts.tv_nsec = 0;
	timer->delta.tv_sec = 0;
	timer->delta.tv_nsec = 0;
}

/**
 * vrrp_timer_is_running() - determine if a timer is 
 *                           currently used
 *
 * @return 1 if running, 0 else
 */
int vrrp_timer_is_running(struct vrrp_timer *timer)
{
	if ((timer->ts.tv_sec != 0) || (timer->ts.tv_nsec != 0))
		return 1;

	return 0;
}

/**
 * vrrp_timer_update() - update a timer
 *
 * @return -1 if clock_gettime() failed
 * @return ETIME if timer is expired
 * @return 0 if timer is successfully updated
 */
int vrrp_timer_update(struct vrrp_timer *timer)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == -1) {
		log_error("clock_gettime: %s", strerror(errno));
		return -1;	/* TODO die() */
	}

	if (timespec_substract(&timer->delta, &timer->ts, &ts)) {
		log_debug("current timer expired");
		return 1;
	}

	timer->ts.tv_sec = ts.tv_sec + timer->delta.tv_sec;
	timer->ts.tv_nsec = ts.tv_nsec + timer->delta.tv_nsec;

#ifdef DEBUG
	log_debug("timer->ts.tv_sec %ld", timer->ts.tv_sec);
	log_debug("timer->ts.tv_nsec %ld", timer->ts.tv_nsec);
#endif /* DEBUG */

	return 0;
}

/**
 * vrrp_timer_is_expired() - check if a timer is expired
 *
 */
int vrrp_timer_is_expired(struct vrrp_timer *timer)
{
	if (vrrp_timer_update(timer)
	    || ((timer->ts.tv_sec <= 0) && (timer->ts.tv_nsec <= 0)))
		return 1;

	return 0;
}
