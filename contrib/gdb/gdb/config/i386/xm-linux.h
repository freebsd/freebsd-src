/* Native support for GNU/Linux, for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1989, 1992 Free Software Foundation, Inc.

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

#ifndef XM_LINUX_H
#define XM_LINUX_H

#define HOST_BYTE_ORDER LITTLE_ENDIAN

#define HAVE_TERMIOS

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */
#define KERNEL_U_ADDR 0x0

#define NEED_POSIX_SETPGID

/* Need R_OK etc, but USG isn't defined.  */
#include <unistd.h>

#endif	/* #ifndef XM_LINUX_H */
