/* Native support for SCO OpenServer 5
   Copyright 1996, 1998 Free Software Foundation, Inc.
   Re-written by J. Kean Johnston <jkj@sco.com>.
   Originally written by Robert Lipe <robertl@dgii.com>, based on 
   work by Ian Lance Taylor <ian@cygnus.com> and 
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Basically, its a lot like the older versions ... */
#include "i386/nm-i386sco.h"

/* ... but it can do a lot of SVR4 type stuff too. */
#define SVR4_SHARED_LIBS
#include "solib.h"		/* Pick up shared library support */

#define ATTACH_DETACH

/* SCO does not provide <sys/ptrace.h>.  infptrace.c does not 
   have defaults for these values.  */

#define PTRACE_ATTACH 10
#define PTRACE_DETACH 11
