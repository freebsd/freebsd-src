/* Mach 3.0 common definitions and global vars.

   Copyright (C) 1992 Free Software Foundation, Inc.

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

#ifndef NM_M3_H
#define NM_M3_H

#include <mach.h>

/* Mach3 doesn't declare errno in <errno.h>.  */
extern int errno;

/* Task port of our debugged inferior. */

extern task_t inferior_task;

/* Thread port of the current thread in the inferior. */

extern thread_t current_thread;

/* If nonzero, we must suspend/abort && resume threads
 * when setting or getting the state.
 */
extern int must_suspend_thread;

#define PREPARE_TO_PROCEED(select_it) mach3_prepare_to_proceed(select_it)

/* Try to get the privileged host port for authentication to machid
 *
 * If you can get this, you may debug anything on this host.
 *
 * If you can't, gdb gives it's own task port as the
 * authentication port
 */
#define  mach_privileged_host_port() task_by_pid(-1)

/*
 * This is the MIG ID number of the emulator/server bsd_execve() RPC call.
 *
 * It SHOULD never change, but if it does, gdb `run'
 * command won't work until you fix this define.
 * 
 */
#define MIG_EXEC_SYSCALL_ID		101000

/* If our_message_port gets a msg with this ID,
 * GDB suspends it's inferior and enters command level.
 * (Useful at least if ^C does not work)
 */
#define GDB_MESSAGE_ID_STOP			0x41151

/* wait3 WNOHANG is defined in <sys/wait.h> but
 * for some reason gdb does not want to include
 * that file.
 *
 * If your system defines WNOHANG differently, this has to be changed.
 */
#define WNOHANG 1

/* Before storing, we need to read all the registers.  */

#define CHILD_PREPARE_TO_STORE() read_register_bytes (0, NULL, REGISTER_BYTES)

/* Check if the inferior exists */
#define MACH_ERROR_NO_INFERIOR \
  do if (!MACH_PORT_VALID (inferior_task)) \
  	error ("Inferior task does not exist."); while(0)

/* Error handler for mach calls */
#define CHK(str,ret)	\
  do if (ret != KERN_SUCCESS) \
       error ("Gdb %s [%d] %s : %s\n",__FILE__,__LINE__,str, \
	      mach_error_string(ret)); while(0)

/* This is from POE9 emulator/emul_stack.h
 */
/*
 * Top of emulator stack holds link and reply port.
 */
struct emul_stack_top {
	struct emul_stack_top	*link;
	mach_port_t		reply_port;
};

#define EMULATOR_STACK_SIZE (4096*4)

#define THREAD_ALLOWED_TO_BREAK(mid) mach_thread_for_breakpoint (mid)

#define THREAD_PARSE_ID(arg) mach_thread_parse_id (arg)

#define THREAD_OUTPUT_ID(mid) mach_thread_output_id (mid)

#define ATTACH_TO_THREAD attach_to_thread

/* Don't do wait_for_inferior on attach.  */
#define ATTACH_NO_WAIT

/* Do Mach 3 dependent operations when ^C or a STOP is requested */
#define DO_QUIT() mach3_quit ()

#if 0
/* This is bogus.  It is NOT OK to quit out of target_wait.  */
/* If in mach_msg() and ^C is typed set immediate_quit */
#define REQUEST_QUIT() mach3_request_quit ()
#endif

#endif /* NM_M3_H */
