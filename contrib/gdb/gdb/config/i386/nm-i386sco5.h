/* Native support for SCO OpenServer 5
   Copyright 1996 Free Software Foundation, Inc.
   By Robert Lipe <robertl@dgii.com>. Based on 
   work by Ian Lance Taylor <ian@cygnus.com. and 
   Martin Walker <maw@netcom.com>.

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

/* SCO OpenServer 5 is a superset of 3.2v4.  It is actually quite
   close to SVR4 [ elf, dynamic libes, mmap ] but misses a few things
   like /proc. */

#include "i386/nm-i386sco.h"

/* Since the native compilers [ and linkers ] are licensed from USL,
   we'll try convincing GDB of this... */

#include "solib.h" /* Pick up shared library support */
#define SVR4_SHARED_LIBS

#define ATTACH_DETACH

/* SCO, does not provide <sys/ptrace.h>.  infptrace.c does not 
   have defaults for these values.  */

#define PTRACE_ATTACH 10
#define PTRACE_DETACH 11
