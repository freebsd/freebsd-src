/* Native-dependent definitions for Sparc running SVR4.
   Copyright 1994, 1996, 1997, 1999, 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "regcache.h"

/* Include the generic SVR4 definitions.  */

#include <nm-sysv4.h>

/* Before storing, we need to read all the registers.  */

#define CHILD_PREPARE_TO_STORE() read_register_bytes (0, NULL, REGISTER_BYTES)

/* Solaris PSRVADDR support does not seem to include a place for nPC.  */

#define PRSVADDR_BROKEN

/* gdb wants to use the prgregset_t interface rather than
   the gregset_t interface, partly because that's what's
   used in core-sol2.c */

#define GDB_GREGSET_T prgregset_t
#define GDB_FPREGSET_T prfpregset_t

#ifdef NEW_PROC_API	/* Solaris 6 and above can do HW watchpoints */

#define TARGET_HAS_HARDWARE_WATCHPOINTS

/* The man page for proc4 on solaris 6 and 7 says that the system
   can support "thousands" of hardware watchpoints, but gives no
   method for finding out how many.  So just tell GDB 'yes'.  */
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(TYPE, CNT, OT) 1

/* When a hardware watchpoint fires off the PC will be left at the
   instruction following the one which caused the watchpoint.  
   It will *NOT* be necessary for GDB to step over the watchpoint. */
#define HAVE_CONTINUABLE_WATCHPOINT

extern int procfs_stopped_by_watchpoint (ptid_t);
#define STOPPED_BY_WATCHPOINT(W) \
  procfs_stopped_by_watchpoint(inferior_ptid)

/* Use these macros for watchpoint insertion/deletion.  */
/* type can be 0: write watch, 1: read watch, 2: access watch (read/write) */

extern int procfs_set_watchpoint (ptid_t, CORE_ADDR, int, int, int);
#define target_insert_watchpoint(ADDR, LEN, TYPE) \
        procfs_set_watchpoint (inferior_ptid, ADDR, LEN, TYPE, 1)
#define target_remove_watchpoint(ADDR, LEN, TYPE) \
        procfs_set_watchpoint (inferior_ptid, ADDR, 0, 0, 0)

#endif /* NEW_PROC_API */
