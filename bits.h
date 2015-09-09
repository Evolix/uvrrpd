/*
 * bits.h
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

#ifndef _BITS_H_
#define _BITS_H_

#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>

/** 
 * BIT_MASK(nr) - bit mask
 */
#define BIT_MASK(nr) (1 << (nr))


/**
 * set_bit - Set a bit in memory
 */
static inline void set_bit(int nr, unsigned long *addr)
{
	*addr |= BIT_MASK(nr);
}

/**
 * clear_bit - Clear a bit in memory
 */
static inline void clear_bit(int nr, unsigned long *addr)
{
	*addr &= ~BIT_MASK(nr);
}

/**
 * change_bit - Toggle a bit in memory
 */
static inline void change_bit(int nr, unsigned long *addr)
{
	*addr ^= BIT_MASK(nr);
}

/** 
 * test_and_set_bit - Set a bit and return its old value
 */
static inline int test_and_set_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long old = *addr;

	*addr = old | mask;
	return (old & mask) != 0;
}

/** 
 * test_and_clear_bit - Clear a bit and return its old value
 */
static inline int test_and_clear_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long old = *addr;

	*addr = old & ~mask;
	return (old & mask) != 0;
}

/**
 * test_and_change_bit - Toggle a bit and return its old value
 */
static inline int test_and_change_bit(int nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long old = *addr;

	*addr = old ^ mask;
	return (old & mask) != 0;
}

/**
 * test_bit - Determine whether a bit is set
 */
static inline int test_bit(int nr, const unsigned long *addr)
{
	return 1UL & (*addr >> nr);
}

#endif /* _BITS_H_ */
