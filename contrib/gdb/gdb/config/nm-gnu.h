/* Common declarations for the GNU Hurd

   Copyright (C) 1995 Free Software Foundation, Inc.

   Written by Miles Bader <miles@gnu.ai.mit.edu>

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef __NM_GNU_H__
#define __NM_GNU_H__

#include <unistd.h>
#include <mach.h>
#include <mach/exception.h>

#include "solib.h"     /* Support for shared libraries. */

#undef target_pid_to_str
#define target_pid_to_str(pid) gnu_target_pid_to_str(pid)
extern char *gnu_target_pid_to_str (int pid);

/* Before storing, we need to read all the registers.  */
#define CHILD_PREPARE_TO_STORE() read_register_bytes (0, NULL, REGISTER_BYTES)

/* Don't do wait_for_inferior on attach.  */
#define ATTACH_NO_WAIT

/* Use SVR4 style shared library support */
#define SVR4_SHARED_LIBS
#define NO_CORE_OPS

#define MAINTENANCE_CMDS 1

#endif /* __NM_GNU_H__ */
