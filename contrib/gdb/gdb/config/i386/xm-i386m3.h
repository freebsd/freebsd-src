/* Definitions to make GDB run on Mach 3 on an Intel 386
   Copyright (C) 1986, 1987, 1989, 1991 Free Software Foundation, Inc.

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

#define HOST_BYTE_ORDER LITTLE_ENDIAN

/* Avoid "INT_MIN redefined" warnings -- by defining it here, exactly
   the same as in the system <machine/machtypes.h> file.  */
#undef	INT_MIN
#define	INT_MIN		0x80000000

/* Do implement the attach and detach commands.  */
#define ATTACH_DETACH	1

/* Not needeed */
#define KERNEL_U_ADDR 0

#ifndef EMULATOR_BASE
/* For EMULATOR_BASE and EMULATOR_END.
 * OSF 1/MK has different values in some other place.
 */
#include <machine/vmparam.h>
#endif /* EMULATOR_BASE */
