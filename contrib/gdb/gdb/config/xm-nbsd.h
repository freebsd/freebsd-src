/* Host-dependent definitions for any CPU running NetBSD.
   Copyright 1993, 1994 Free Software Foundation, Inc.

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

/* We have to include these files now, so that GDB will not make
   competing definitions in defs.h.  */
#include <limits.h>

#include <machine/endian.h>
#if BYTE_ORDER == BIG_ENDIAN
#define HOST_BYTE_ORDER BIG_ENDIAN
#else
#define HOST_BYTE_ORDER LITTLE_ENDIAN
#endif

/* NetBSD has termios facilities. */
#define HAVE_TERMIOS

#if 0
#define CC_HAS_LONG_LONG	1
#define PRINTF_HAS_LONG_LONG	1
#endif
