/* This file just picks the correct udiphxxx.h depending on the host.
   The two hosts that are now defined are UNIX and MSDOS.

   Copyright 1993 Free Software Foundation, Inc.

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

/*
 * For the moment, we will default to BSD_IPC; this might change if/when
 * another type of IPC (Mach? SysV?) is implemented.
 */

#if 0

/* We don't seem to have a copy of udiphdos.h.  Furthermore, all the
   things in udiphunix.h are pretty much generic 32-bit machine defines
   which don't have anything to do with IPC.  */

#ifdef DOS_IPC
#include "udiphdos.h"
#else
/*#ifdef BSD_IPC */
#include "udiphunix.h"
#endif

#else

#include "udiphunix.h"

#endif
