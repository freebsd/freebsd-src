/* Definitions for dealing with stack frames, for GDB, the GNU debugger.
   Copyright 1986, 1989, 1991, 1992 Free Software Foundation, Inc.

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

#if !defined (FRAME_H)
#define FRAME_H 1

/* We keep a cache of stack frames, each of which is a "struct
   frame_info".  The innermost one gets allocated (in
   wait_for_inferior) each time the inferior stops; current_frame
   points to it.  Additional frames get allocated (in
   get_prev_frame_info) as needed, and are chained through the next
   and prev fields.  Any time that the frame cache becomes invalid
   (most notably when we execute something, but also if we change how
   we interpret the frames (e.g. "set heuristic-fence-post" in
   mips-tdep.c, or anything which reads new symbols)), we should call
   reinit_frame_cache.  */

struct frame_info
  {
    /* Nominal address of the frame described.  See comments at FRAME_FP
       about what this means outside the *FRAME* macros; in the *FRAME*
       macros, it can mean whatever makes most sense for this machine.  */
    CORE_ADDR frame;

    /* Address at which execution is occurring in this frame.
       For the innermost frame, it's the current pc.
       For other frames, it is a pc saved in the next frame.  */
    CORE_ADDR pc;

    /* Nonzero if this is a frame associated with calling a signal handler.

       Set by machine-dependent code.  On some machines, if
       the machine-dependent code fails to check for this, the backtrace
       will look relatively normal.  For example, on the i386
         #3  0x158728 in sighold ()
       On other machines (e.g. rs6000), the machine-dependent code better
       set this to prevent us from trying to print it like a normal frame.  */
    int signal_handler_caller;

    /* Anything extra for this structure that may have been defined
       in the machine dependent files. */
#ifdef EXTRA_FRAME_INFO
    EXTRA_FRAME_INFO
#endif

    /* We should probably also store a "struct frame_saved_regs" here.
       This is already done by some machines (e.g. config/m88k/tm-m88k.h)
       but there is no reason it couldn't be general.  */

    /* Pointers to the next and previous frame_info's in the frame cache.  */
   struct frame_info *next, *prev;
  };

/* Describe the saved registers of a frame.  */

struct frame_saved_regs
  {

    /* For each register, address of where it was saved on entry to
       the frame, or zero if it was not saved on entry to this frame.
       This includes special registers such as pc and fp saved in
       special ways in the stack frame.  The SP_REGNUM is even more
       special, the address here is the sp for the next frame, not the
       address where the sp was saved.  */

    CORE_ADDR regs[NUM_REGS];
  };

/* Return the frame address from FR.  Except in the machine-dependent
   *FRAME* macros, a frame address has no defined meaning other than
   as a magic cookie which identifies a frame over calls to the
   inferior.  The only known exception is inferior.h
   (PC_IN_CALL_DUMMY) [ON_STACK]; see comments there.  You cannot
   assume that a frame address contains enough information to
   reconstruct the frame; if you want more than just to identify the
   frame (e.g. be able to fetch variables relative to that frame),
   then save the whole struct frame_info (and the next struct
   frame_info, since the latter is used for fetching variables on some
   machines).  */

#define FRAME_FP(fi) ((fi)->frame)

/* Define a default FRAME_CHAIN_VALID, in the form that is suitable for most
   targets.  If FRAME_CHAIN_VALID returns zero it means that the given frame
   is the outermost one and has no caller.

   If a particular target needs a different definition, then it can override
   the definition here by providing one in the tm file. */

#if !defined (FRAME_CHAIN_VALID)

#if defined (FRAME_CHAIN_VALID_ALTERNATE)

/* Use the alternate method of avoiding running up off the end of the frame
   chain or following frames back into the startup code.  See the comments
   in objfiles.h. */
   
#define FRAME_CHAIN_VALID(chain, thisframe)	\
  ((chain) != 0					\
   && !inside_main_func ((thisframe) -> pc)	\
   && !inside_entry_func ((thisframe) -> pc))

#else

#define FRAME_CHAIN_VALID(chain, thisframe)	\
  ((chain) != 0					\
   && !inside_entry_file (FRAME_SAVED_PC (thisframe)))

#endif	/* FRAME_CHAIN_VALID_ALTERNATE */

#endif	/* FRAME_CHAIN_VALID */

/* The stack frame that the user has specified for commands to act on.
   Note that one cannot assume this is the address of valid data.  */

extern struct frame_info *selected_frame;

/* Level of the selected frame:
   0 for innermost, 1 for its caller, ...
   or -1 for frame specified by address with no defined level.  */

extern int selected_frame_level;

extern struct frame_info *get_prev_frame_info PARAMS ((struct frame_info *));

extern struct frame_info *create_new_frame PARAMS ((CORE_ADDR, CORE_ADDR));

extern void flush_cached_frames PARAMS ((void));

extern void reinit_frame_cache PARAMS ((void));

extern void get_frame_saved_regs PARAMS ((struct frame_info *,
					  struct frame_saved_regs *));

extern void set_current_frame PARAMS ((struct frame_info *));

extern struct frame_info *get_prev_frame PARAMS ((struct frame_info *));

extern struct frame_info *get_current_frame PARAMS ((void));

extern struct frame_info *get_next_frame PARAMS ((struct frame_info *));

extern struct block *get_frame_block PARAMS ((struct frame_info *));

extern struct block *get_current_block PARAMS ((void));

extern struct block *get_selected_block PARAMS ((void));

extern struct symbol *get_frame_function PARAMS ((struct frame_info *));

extern CORE_ADDR get_frame_pc PARAMS ((struct frame_info *));

extern CORE_ADDR get_pc_function_start PARAMS ((CORE_ADDR));

extern struct block * block_for_pc PARAMS ((CORE_ADDR));

extern int frameless_look_for_prologue PARAMS ((struct frame_info *));

extern void print_frame_args PARAMS ((struct symbol *, struct frame_info *,
				      int, GDB_FILE *));

extern struct frame_info *find_relative_frame PARAMS ((struct frame_info *, int*));

extern void print_stack_frame PARAMS ((struct frame_info *, int, int));

extern void select_frame PARAMS ((struct frame_info *, int));

extern void record_selected_frame PARAMS ((CORE_ADDR *, int *));

extern void print_frame_info PARAMS ((struct frame_info *, int, int, int));

extern CORE_ADDR find_saved_register PARAMS ((struct frame_info *, int));

extern struct frame_info *block_innermost_frame PARAMS ((struct block *));

extern struct frame_info *find_frame_addr_in_frame_chain PARAMS ((CORE_ADDR));

extern CORE_ADDR sigtramp_saved_pc PARAMS ((struct frame_info *));

#endif /* !defined (FRAME_H)  */
