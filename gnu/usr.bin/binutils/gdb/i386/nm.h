/* Native-dependent definitions for Intel 386 running BSD Unix, for GDB.
   Copyright 1986, 1987, 1989, 1992 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef NM_FREEBSD_H
#define NM_FREEBSD_H

/* Be shared lib aware */
#include "solib.h"

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */

#include <machine/vmparam.h>
#define KERNEL_U_ADDR USRSTACK

/* #undef FLOAT_INFO	/* No float info yet */
#define FLOAT_INFO extern i386_float_info (); \
						  i386_float_info ()

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = i386_register_u_addr ((blockend),(regno));

extern int
i386_register_u_addr PARAMS ((int, int));

#define PTRACE_ARG3_TYPE char*

#endif /* NM_FREEBSD_H */
