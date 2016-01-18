/*
 * uvrrpd.h
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

#ifndef _UVRRPD_H_
#define _UVRRPD_H_

#include "bits.h"

#define PIDFILE_NAME	stringify(PATHRUN) "/uvrrpd_%d.pid"
#define CTRLFILE_NAME	stringify(PATHRUN) "/uvrrpd_ctrl.%d"

/** 
 * uvrrpd_control
 * Enum server control register flags
 */
enum uvrrpd_control {
	/* daemon keep going bit */
	KEEP_GOING = BIT_MASK(0),
	/* daemon dump bit */
	UVRRPD_DUMP = BIT_MASK(1),
	/* daemon logout bit */
	UVRRPD_LOGOUT = BIT_MASK(2),
	/* daemon reload bit */
	UVRRPD_RELOAD = BIT_MASK(3),
};

#endif /* _UVRRPD_ */
