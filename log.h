/*
 * log.h
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

#ifndef _LOG_H_
#define _LOG_H_


/* ISO C */
#include <errno.h>
#include <string.h>

/* POSIX */
#include <syslog.h>

/**
 * log_trigger (char const *level) 
 *   Set up logs level trigger. If level is NULL return current level trigger
 * - <b>err</b> Error conditions
 * - <b>warning</b> Warning conditions
 * - <b>notice</b> Normal, but significant, condition
 * - <b>info</b> Informational message
 * - <b>debug</b> Debug message
 */
int log_trigger(char const *level);

/**
 * log_open (char const *app, char const *level)
 *    Open logs system. Define logs level trigger.
 *  @app Application name
 *  @level Logs level
 */
void log_open(char const *app, char const *level);

/**
 * log_close( void )
 *  Close logs system.
 */
void log_close(void);

/**
 * log_it( int priority, char const *format, ...)
 *   Log message if priority is higher than level trigger
 *  @priority Message
 *  @format Log message
 *  @... Arguments list
 */
void log_it(int priority, char const *format, ...)
    __attribute__ ((__format__(__printf__, 2, 3)));

/**
 * log_error( fmt, ... ) 
 *  @fmt Log message
 *  @... Arguments list
 */
#define log_error( fmt, ... ) \
do { \
    log_it( LOG_ERR, "%s::%s "fmt, \
    		 "error", __func__, ##__VA_ARGS__ ); \
} while ( 0 )

/** 
 * log_sys_error( fmt, ... )
 *  @fmt Log message
 *  @... Arguments list
 */
#define log_sys_error( fmt, ... ) \
do { \
    log_it( LOG_ERR, "%s::%s "fmt" %s", \
    		 "error", __func__, ##__VA_ARGS__, strerror(errno) ); \
} while ( 0 )

/**
 * log_warning( fmt, ... )
 *  @fmt Log message
 *  @... Arguments list
 */
#define log_warning( fmt, ... ) \
do { \
    log_it( LOG_WARNING, "%s::%s "fmt, \
    		 "warning", __func__, ##__VA_ARGS__ ); \
} while ( 0 )

/** 
 * log_notice( ... )
 *  @... Log message and arguments list
 */
#define log_notice( ... ) \
do { \
    log_it( LOG_NOTICE, ##__VA_ARGS__ ); \
} while ( 0 )

/**
 * log_info( ... )
 *  Log message. Enable in info or higher level
 *  @... Log message and arguments list
 */
#define log_info( ... ) \
do { \
	extern int __log_trigger; \
 \
	if ( LOG_INFO <= __log_trigger ) \
		log_it( LOG_INFO, ##__VA_ARGS__ ); \
} while ( 0 )

/**
 * log_debug( fmt, ... )
 *  Log message. Enable in debug level
 *  @fmt Log message
 *  @... Arguments list
 */
#define log_debug( fmt, ... ) \
do { \
	extern int __log_trigger; \
 \
	if ( LOG_DEBUG <= __log_trigger ) \
		log_it( LOG_DEBUG, "%s %s "fmt, \
                	 "--- DEBUG ---", __func__, ##__VA_ARGS__ ); \
} while ( 0 )

#ifdef DEBUG
/**
 * log_devel( fmt, ... )
 *  Log message. Enable in devel level
 *  @fmt Log message
 *  @... Arguments list
 */
#define log_devel( fmt, ... ) \
do { \
	extern int __log_trigger; \
 \
	if ( LOG_DEBUG <= __log_trigger ) \
		log_it( LOG_DEBUG, "%s %s( " fmt " )", \
                	"--- DEVEL ---", __func__, ##__VA_ARGS__ ); \
} while ( 0 )
#else
#define log_devel( fmt, ... )
#endif /* DEBUG */


#endif /* _LOG_H_ */
