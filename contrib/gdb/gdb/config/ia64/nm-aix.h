/* Native support for AIX, for GDB, the GNU debugger.
   Copyright 2000, 2001 Free Software Foundation, Inc.

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

#ifndef NM_AIX_H
#define NM_AIX_H

#include "nm-sysv4.h"

#ifndef AIX5
#define AIX5 1
#endif

/* Type of the operation code for sending control messages to the
   /proc/PID/ctl file */
#define PROC_CTL_WORD_TYPE int

#define GDB_GREGSET_T prgregset_t
#define GDB_FPREGSET_T prfpregset_t

#endif /* #ifndef NM_AIX_H */
