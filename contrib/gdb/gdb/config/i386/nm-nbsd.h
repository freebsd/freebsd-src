/* Native-dependent definitions for Intel 386 running NetBSD, for GDB.
   Copyright 1986, 1987, 1989, 1992, 1994 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef NM_NBSD_H
#define NM_NBSD_H

/* Get generic NetBSD native definitions. */
#include "nm-nbsd.h"

/* #define FLOAT_INFO	{ i386_float_info(); } */

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = i386_register_u_addr ((blockend),(regno));

extern int
i386_register_u_addr PARAMS ((int, int));

#endif /* NM_NBSD_H */
