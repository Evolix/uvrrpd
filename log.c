/*
 * log.c 
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

/* ISO C */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* POSIX */
#include <syslog.h>

#include "common.h"

#ifdef DEBUG
int __log_trigger = LOG_DEBUG;
#else
int __log_trigger = LOG_NOTICE;
#endif

int log_trigger(char const *level)
{
	/*
	 *  LOG_ERR error conditions
	 *  LOG_WARNING warning conditions
	 *  LOG_NOTICE normal, but significant, condition
	 *  LOG_INFO informational message
	 *  LOG_DEBUG
	 */
	if (level == NULL) {
		return __log_trigger;
	}
	else if (matches(level, "err")) {
		__log_trigger = LOG_ERR;
	}
	else if (matches(level, "warning")) {
		__log_trigger = LOG_WARNING;
	}
	else if (matches(level, "notice")) {
		__log_trigger = LOG_NOTICE;
	}
	else if (matches(level, "info")) {
		__log_trigger = LOG_INFO;
	}
	else if (matches(level, "debug")) {
		__log_trigger = LOG_DEBUG;
	}

	return __log_trigger;
}

void log_open(char const *app, char const *level)
{
	openlog(app ? app : "-", LOG_PERROR | LOG_PID, LOG_DAEMON);
	log_trigger(level);
}

void log_close(void)
{
	closelog();
}

void log_it(int priority, char const *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsyslog(priority, format, ap);
	va_end(ap);
}
