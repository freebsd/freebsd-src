/* Main loop for the standalone kernel debugger, for GDB, the GNU Debugger.
   Copyright 1989, 1991, 1992 Free Software Foundation, Inc.

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

#include "defs.h"

static char *args[] =
{"kdb", "kdb-symbols", 0};

static char *environment[] =
{0};

char **environ;

start ()
{
  INIT_STACK (kdb_stack_beg, kdb_stack_end);

  environ = environment;

  main (2, args, environment);
}
