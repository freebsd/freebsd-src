/* This file defines the interface between the simulator and gdb.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.

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

#if !defined (REMOTE_SIM_H)
#define REMOTE_SIM_H 1

#include "callback.h"
/* This file is used when building stand-alone simulators, so isolate this
   file from gdb.  */

/* Pick up CORE_ADDR_TYPE if defined (from gdb), otherwise use same value as
   gdb does (unsigned int - from defs.h).  */

#ifndef CORE_ADDR_TYPE
typedef unsigned int SIM_ADDR;
#else
typedef CORE_ADDR_TYPE SIM_ADDR;
#endif

/* Callbacks.
   The simulator may use the following callbacks (gdb routines) which the
   standalone program must provide.

   void printf_filtered (char *msg, ...);
   void error /-* noreturn *-/ (char *msg, ...);
   void *xmalloc (long size);
   int sim_callback_write_stdout (char *, int len);

   The new way of doing I/O is to use the pointer provided by GDB
   via the sim_set_callbacks call, look in callbacks.c to see what
   can be done.
*/

/* Main simulator entry points ...

   All functions that can get an error must call the gdb routine `error',
   they can only return upon success.  */

/* Initialize the simulator.  This function is called when the simulator
   is selected from the command line. ARGS is passed from the command line
   and can be used to select whatever run time options the simulator provides.
   ARGS is the raw character string and must be parsed by the simulator,
   which is trivial to do with the buildargv function in libiberty.
   It is ok to do nothing.  */

void sim_open PARAMS ((char *args));

/* Terminate usage of the simulator.  This may involve freeing target memory
   and closing any open files and mmap'd areas.  You cannot assume sim_kill
   has already been called.
   QUITTING is non-zero if we cannot hang on errors.  */

void sim_close PARAMS ((int quitting));

/* Load program PROG into the simulator.
   Return non-zero if you wish the caller to handle it
   (it is done this way because most simulators can use gr_load_image,
   but defining it as a callback seems awkward).  */

int sim_load PARAMS ((char *prog, int from_tty));

/* Prepare to run the simulated program.
   START_ADDRESS is, yes, you guessed it, the start address of the program.
   ARGV and ENV are NULL terminated lists of pointers.
   Gdb will set the start address via sim_store_register as well, but
   standalone versions of existing simulators are not set up to cleanly call
   sim_store_register, so the START_ADDRESS argument is there as a
   workaround.  */

void sim_create_inferior PARAMS ((SIM_ADDR start_address,
				  char **argv, char **env));

/* Kill the running program.
   This may involve closing any open files and deleting any mmap'd areas.  */

void sim_kill PARAMS ((void));

/* Read LENGTH bytes of the simulated program's memory and store in BUF.
   Result is number of bytes read, or zero if error.  */

int sim_read PARAMS ((SIM_ADDR mem, unsigned char *buf, int length));

/* Store LENGTH bytes from BUF in the simulated program's memory.
   Result is number of bytes write, or zero if error.  */

int sim_write PARAMS ((SIM_ADDR mem, unsigned char *buf, int length));

/* Fetch register REGNO and store the raw value in BUF.  */

void sim_fetch_register PARAMS ((int regno, unsigned char *buf));

/* Store register REGNO from BUF (in raw format).  */

void sim_store_register PARAMS ((int regno, unsigned char *buf));

/* Print some interesting information about the simulator.
   VERBOSE is non-zero for the wordy version.  */

void sim_info PARAMS ((int verbose));

/* Fetch why the program stopped.
   SIGRC will contain either the argument to exit() or the signal number.  */

enum sim_stop { sim_exited, sim_stopped, sim_signalled };

void sim_stop_reason PARAMS ((enum sim_stop *reason, int *sigrc));

/* Run (or resume) the program.  */

void sim_resume PARAMS ((int step, int siggnal));

/* Passthru for other commands that the simulator might support. */

void sim_do_command PARAMS ((char *cmd));


/* Callbacks for the simulator to use. */

int sim_callback_write_stdout PARAMS ((char *, int));

/* Provide simulator with a standard host_callback_struct. */

void sim_set_callbacks PARAMS ((struct host_callback_struct *));


#endif /* !defined (REMOTE_SIM_H) */
