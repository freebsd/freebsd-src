/* $FreeBSD: src/gnu/usr.bin/binutils/gdb/alpha/tm.h,v 1.2.2.1 2000/07/06 22:08:07 obrien Exp $ */
/* Definitions to make GDB run on an Alpha box under FreeBSD.  The
   definitions here are used when the _target_ system is running Linux.
   Copyright 1996 Free Software Foundation, Inc.

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

#ifndef TM_FREEBSDALPHA_H
#define TM_FREEBSDALPHA_H

#include "alpha/tm-alpha.h"
#ifndef S0_REGNUM
#define S0_REGNUM (T7_REGNUM+1)
#endif


/* Number of traps that happen between exec'ing the shell to run an
   inferior, and when we finally get to the inferior code.  This is 2
   on FreeBSD and most implementations.  */

#undef START_INFERIOR_TRAPS_EXPECTED
#define START_INFERIOR_TRAPS_EXPECTED 2

struct objfile;
void freebsd_uthread_new_objfile PARAMS ((struct objfile *objfile));
#define target_new_objfile(OBJFILE) freebsd_uthread_new_objfile (OBJFILE)

extern char *freebsd_uthread_pid_to_str PARAMS ((int pid));
#define target_pid_to_str(PID) freebsd_uthread_pid_to_str (PID)

#endif /* TM_FREEBSDALPHA_H */
