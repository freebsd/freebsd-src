/* Multi-process/thread control defs for GDB, the GNU debugger.
   Copyright 1987, 1988, 1989, 1990, 1991, 1992, 1993

   Contributed by Lynx Real-Time Systems, Inc.  Los Gatos, CA.
   Free Software Foundation, Inc.

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

#ifndef THREAD_H
#define THREAD_H

extern void init_thread_list PARAMS ((void));

extern void add_thread PARAMS ((int pid));

extern int in_thread_list PARAMS ((int pid));

extern int pid_to_thread_id PARAMS ((int pid));

extern int valid_thread_id PARAMS ((int thread));

#endif	/* THREAD_H */
