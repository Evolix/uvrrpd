/*
 * vrrp_timer.h - functions manipulating VRRP timers
 *
 * These functions use clock_gettime() and CLOCK_MONOTONIC_RAW
 * which use a raw hardware-based time that is not subject to
 * NTP adjustements.
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


#ifndef _VRRP_TIMER_H_
#define _VRRP_TIMER_H_

#include <sys/time.h>

/**
 * struct vrrp_timer
 *
 * @ts timestamp in ns
 * @delta time since the last update
 */
struct vrrp_timer {
	struct timespec ts;
	struct timespec delta;
};

/* prototype functions */
int vrrp_timer_set(struct vrrp_timer *timer, time_t delay, long delay_cs);
void vrrp_timer_clear(struct vrrp_timer *timer);
int vrrp_timer_is_running(struct vrrp_timer *timer);
int vrrp_timer_update(struct vrrp_timer *timer);
int vrrp_timer_is_expired(struct vrrp_timer *timer);

/* Specific VRRP timer macros */
#define SKEW_TIME( v )      \
    ( (256 - v->priority) * \
    (v->version==3?v->master_adv_int:1) / 256 )

#define MASTERDOWN_INT( v ) \
    (3 * v->master_adv_int + SKEW_TIME( v ))

#define VRRP_SET_ADV_TIMER( v )             \
    vrrp_timer_set(&v->adv_timer,           \
        (v->version == 3 ? 0:v->adv_int),    \
        (v->version == 3 ? v->master_adv_int:0))

#define VRRP_SET_MASTERDOWN_TIMER( v )                  \
    vrrp_timer_set(&v->masterdown_timer,                \
            (v->version == 3 ? 0:MASTERDOWN_INT( v )),  \
            (v->version == 3 ? MASTERDOWN_INT( v ):0))

#define VRRP_SET_SKEW_TIME( v )                         \
    vrrp_timer_set(&v->masterdown_timer,                \
            (v->version == 3 ? 0:SKEW_TIME( v )),       \
            (v->version == 3 ? SKEW_TIME( v ):0))

#define VRRP_SET_STARTDELAY_TIMER( v )			\
    vrrp_timer_set(&v->masterdown_timer,		\
	    (v->version == 3 ? 0:v->start_delay),	\
            (v->version == 3 ? v->start_delay:0))

#endif /* _VRRP_TIMER_H_ */
