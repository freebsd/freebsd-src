/* Multi-process/thread control defs for GDB, the GNU debugger.
   Copyright 1987, 88, 89, 90, 91, 92, 1993, 1998

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef GDBTHREAD_H
#define GDBTHREAD_H

/* For bpstat */
#include "breakpoint.h"

extern void init_thread_list PARAMS ((void));

extern void add_thread PARAMS ((int pid));

extern void delete_thread PARAMS ((int));
  
extern int thread_id_to_pid PARAMS ((int));

extern int in_thread_list PARAMS ((int pid));

extern int pid_to_thread_id PARAMS ((int pid));

extern int valid_thread_id PARAMS ((int thread));

extern void load_infrun_state PARAMS ((int, CORE_ADDR *, CORE_ADDR *, char **,
				       int *, struct breakpoint **,
				       struct breakpoint **, CORE_ADDR *,
				       CORE_ADDR *, CORE_ADDR *, int *, int *,
                                       int *, bpstat *, int *));

extern void save_infrun_state PARAMS ((int, CORE_ADDR, CORE_ADDR, char *,
				       int, struct breakpoint *,
				       struct breakpoint *, CORE_ADDR,
				       CORE_ADDR, CORE_ADDR, int, int,
                                       int, bpstat, int));

/* Commands with a prefix of `thread'.  */
extern struct cmd_list_element *thread_cmd_list;

/* Support for external (remote) systems with threads (processes) */
/* For example real time operating systems */
 
#define OPAQUETHREADBYTES 8
/* a 64 bit opaque identifier */
typedef unsigned char threadref[OPAQUETHREADBYTES] ;
/* WARNING: This threadref data structure comes from the remote O.S., libstub
   protocol encoding, and remote.c. it is not particularly changable */

/* Right now, the internal structure is int. We want it to be bigger.
   Plan to fix this.
   */
typedef int gdb_threadref ; /* internal GDB thread reference */

/*  gdb_ext_thread_info is an internal GDB data structure which is
   equivalint to the reply of the remote threadinfo packet */

struct gdb_ext_thread_info
{
  threadref threadid ; /* External form of thread reference */
  int active ;         /* Has state interesting to GDB? , regs, stack */
  char display[256] ;  /* Brief state display, name, blocked/syspended */
  char shortname[32] ; /* To be used to name threads */
  char more_display[256] ; /* Long info, statistics, queue depth, whatever */
} ;

/* The volume of remote transfers can be limited by submitting
   a mask containing bits specifying the desired information.
   Use a union of these values as the 'selection' parameter to
   get_thread_info. FIXME: Make these TAG names more thread specific.
   */
#define TAG_THREADID 1
#define TAG_EXISTS 2
#define TAG_DISPLAY 4
#define TAG_THREADNAME 8
#define TAG_MOREDISPLAY 16 

/* Always initialize an instance of this structure using run time assignments */
/* Because we are likely to add entrtries to it. */
/* Alternatly, WE COULD ADD THESE TO THE TARGET VECTOR */
  
struct target_thread_vector
{
  int (*find_new_threads)PARAMS((void)) ;
  int (*get_thread_info) PARAMS((
			 gdb_threadref * ref,
			 int selection,
			 struct gdb_ext_thread_info * info
			 )) ;
  /* to_thread_alive - Already in the target vector */
  /* to_switch_thread - Done via select frame */
} ;

extern void bind_target_thread_vector PARAMS((struct target_thread_vector * vec)) ;

extern struct target_thread_vector * unbind_target_thread_vector PARAMS ((void)) ;

extern int target_get_thread_info PARAMS((
				  gdb_threadref * ref,
				  int selection,
				  struct gdb_ext_thread_info * info)) ;


#endif	/* GDBTHREAD_H */
