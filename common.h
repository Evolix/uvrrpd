/*
 * common.h - common types and macros
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


#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "log.h"

/**
 * bool - boolean type
 */
typedef enum {
	FALSE,
	TRUE
} bool;

/**
 * matches( s, c_str ) - Compare strings.
 *  s     Data strings
 *  c_str C-Strings
 */
#define matches( s, c_str ) \
({ \
	const char __dummy[] = c_str; \
	(void)(&__dummy); \
	( memcmp ( s, c_str, sizeof( c_str ) ) == 0 ); \
})

#define _stringify(x) #x
#define stringify(x) _stringify(x)

/**
 * ARRAY_SIZE()
 */
#define ARRAY_SIZE(a)      (sizeof(a)/sizeof((a)[0]))


/**
 * cksum - compute IP checksum 
 */
static inline int unsigned short cksum(unsigned short *buf, int nbytes)
{
	uint32_t sum;
	uint16_t oddbyte;

	sum = 0;
	while (nbytes > 1) {
		sum += *buf++;
		nbytes -= 2;
	}

	if (nbytes == 1) {
		oddbyte = 0;
		*((uint16_t *) & oddbyte) = *(uint16_t *) buf;
		sum += oddbyte;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (uint16_t) ~ sum;
}

/**
 * mystrtoul - convert a string to an unsigned long int
 */
static inline int mystrtoul(unsigned long *const dest,
			    const char *const str, unsigned long max)
{
	unsigned long val;
	char *endptr;

	errno = 0;
	val = strtoull(str, &endptr, 0);

	if ((val == 0 || val == LONG_MAX) && errno == ERANGE)
		return -ERANGE;

	if (val > max)
		return -ERANGE;

	if (*endptr != '\0')
		return -EINVAL;

	*dest = val;
	return 0;
}

/**
 * is_file_executable
 */
static inline int is_file_executable(const char *filename)
{
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		perror("stat");
		return 0;
	}

	if (S_ISREG(sb.st_mode) &&
	    (sb.st_mode & S_IRUSR) && (sb.st_mode & S_IXUSR))
		return 1;

	return 0;
}


#define IP4_NMASK 	 32
#define IP6_NMASK 	 128
/**
 * split_ip_netmask() - split IPvX and netmask from a string
 */
static inline int split_ip_netmask(int family,
				   const char *str, void *addr,
				   uint8_t * netmask)
{
	char *tmp;
	unsigned long ul;

	int netmask_length, err;

	/* IPv4 */
	if (family == AF_INET)
		netmask_length = IP4_NMASK;

	/* IPv6 */
	if (family == AF_INET6)
		netmask_length = IP6_NMASK;

	tmp = strstr(str, "/");
	*netmask = 0;

	if (tmp != NULL) {
		*tmp = '\0';
		++tmp;
		err = mystrtoul(&ul, tmp, netmask_length);
		if (err == -ERANGE) {
			log_error("%s", "CIDR netmask out of range");
			return -ERANGE;
		}
		if (err == -EINVAL) {
			log_error("Error parsing %s as a number", tmp);
			return -EINVAL;
		}
		if (netmask != NULL)
			*netmask = (uint8_t) ul;
	}

	if (inet_pton(family, str, addr) == 0) {
		log_error("inet_pton - %m");
		return -1;
	}

	return 0;
}


#ifdef DEBUG
/**
 * print buf hexa
 */
static inline void print_buf_hexa(const char *str, void *buf, size_t x)
{
	uint8_t i;
	int j = 0;

	unsigned char *pbuf = (unsigned char *) buf;

	printf("*** %s ***\n", str);

	for (i = 0; i < x; ++i, ++j) {
		if (j == 4) {
			printf("\n");
			j = 0;
		}
		if (j > 0)
			printf(":");
		printf("%02X", pbuf[i]);
	}
	printf("\n");
}
#endif

#endif /* _COMMON_H_ */
