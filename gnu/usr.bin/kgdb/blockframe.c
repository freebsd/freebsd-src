/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 */

#ifndef lint
static char sccsid[] = "@(#)blockframe.c	6.4 (Berkeley) 5/11/91";
#endif /* not lint */

/* Get info from stack frames;
   convert between frames, blocks, functions and pc values.
   Copyright (C) 1986, 1987, 1988, 1989 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "frame.h"

#include <obstack.h>

#if defined(NEWVM) && defined(KERNELDEBUG)
#include <sys/param.h>	/* XXX for FRAME_CHAIN_VALID */
#endif

/* Start and end of object file containing the entry point.
   STARTUP_FILE_END is the first address of the next file.
   This file is assumed to be a startup file
   and frames with pc's inside it
   are treated as nonexistent.

   Setting these variables is necessary so that backtraces do not fly off
   the bottom of the stack.  */
CORE_ADDR startup_file_start;
CORE_ADDR startup_file_end;

/* Is ADDR outside the startup file?  */
int
outside_startup_file (addr)
     CORE_ADDR addr;
{
  return !(addr >= startup_file_start && addr < startup_file_end);
}

/* Address of innermost stack frame (contents of FP register) */

static FRAME current_frame;

struct block *block_for_pc ();
CORE_ADDR get_pc_function_start ();

/*
 * Cache for frame addresses already read by gdb.  Valid only while
 * inferior is stopped.  Control variables for the frame cache should
 * be local to this module.
 */
struct obstack frame_cache_obstack;

/* Return the innermost (currently executing) stack frame.  */

FRAME
get_current_frame ()
{
  /* We assume its address is kept in a general register;
     param.h says which register.  */

  return current_frame;
}

void
set_current_frame (frame)
     FRAME frame;
{
  current_frame = frame;
}

FRAME
create_new_frame (addr, pc)
     FRAME_ADDR addr;
     CORE_ADDR pc;
{
  struct frame_info *fci;	/* Same type as FRAME */

  fci = (struct frame_info *)
    obstack_alloc (&frame_cache_obstack,
		   sizeof (struct frame_info));

  /* Arbitrary frame */
  fci->next = (struct frame_info *) 0;
  fci->prev = (struct frame_info *) 0;
  fci->frame = addr;
  fci->next_frame = 0;		/* Since arbitrary */
  fci->pc = pc;

#ifdef INIT_EXTRA_FRAME_INFO
  INIT_EXTRA_FRAME_INFO (fci);
#endif

  return fci;
}

/* Return the frame that called FRAME.
   If FRAME is the original frame (it has no caller), return 0.  */

FRAME
get_prev_frame (frame)
     FRAME frame;
{
  /* We're allowed to know that FRAME and "struct frame_info *" are
     the same */
  return get_prev_frame_info (frame);
}

/* Return the frame that FRAME calls (0 if FRAME is the innermost
   frame).  */

FRAME
get_next_frame (frame)
     FRAME frame;
{
  /* We're allowed to know that FRAME and "struct frame_info *" are
     the same */
  return frame->next;
}

/*
 * Flush the entire frame cache.
 */
void
flush_cached_frames ()
{
  /* Since we can't really be sure what the first object allocated was */
  obstack_free (&frame_cache_obstack, 0);
  obstack_init (&frame_cache_obstack);

  current_frame = (struct frame_info *) 0; /* Invalidate cache */
}

/* Return a structure containing various interesting information
   about a specified stack frame.  */
/* How do I justify including this function?  Well, the FRAME
   identifier format has gone through several changes recently, and
   it's not completely inconceivable that it could happen again.  If
   it does, have this routine around will help */

struct frame_info *
get_frame_info (frame)
     FRAME frame;
{
  return frame;
}

/* If a machine allows frameless functions, it should define a macro
   FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) in param.h.  FI is the struct
   frame_info for the frame, and FRAMELESS should be set to nonzero
   if it represents a frameless function invocation.  */

/* Many machines which allow frameless functions can detect them using
   this macro.  Such machines should define FRAMELESS_FUNCTION_INVOCATION
   to just call this macro.  */
#define FRAMELESS_LOOK_FOR_PROLOGUE(FI, FRAMELESS) \
{      	       	       	       	       	       	       	       	       	 \
  CORE_ADDR func_start, after_prologue;			                 \
  func_start = (get_pc_function_start ((FI)->pc) +	                 \
		FUNCTION_START_OFFSET);			                 \
  if (func_start)                                                        \
    {									 \
      after_prologue = func_start;					 \
      SKIP_PROLOGUE (after_prologue);					 \
      (FRAMELESS) = (after_prologue == func_start);			 \
    }									 \
  else									 \
    /* If we can't find the start of the function, we don't really */    \
    /* know whether the function is frameless, but we should be	   */    \
    /* able to get a reasonable (i.e. best we can do under the	   */    \
    /* circumstances) backtrace by saying that it isn't.  */	         \
    (FRAMELESS) = 0;							 \
}

/* Return a structure containing various interesting information
   about the frame that called NEXT_FRAME.  Returns NULL
   if there is no such frame.  */

struct frame_info *
get_prev_frame_info (next_frame)
     FRAME next_frame;
{
  FRAME_ADDR address;
  struct frame_info *prev;
  int fromleaf = 0;

  /* If the requested entry is in the cache, return it.
     Otherwise, figure out what the address should be for the entry
     we're about to add to the cache. */

  if (!next_frame)
    {
      if (!current_frame)
	{
	  if (!have_inferior_p () && !have_core_file_p ())
	    fatal ("get_prev_frame_info: Called before cache primed.  \"Shouldn't happen.\"");
	  else
	    error ("No inferior or core file.");
	}

      return current_frame;
    }

  /* If we have the prev one, return it */
  if (next_frame->prev)
    return next_frame->prev;

  /* On some machines it is possible to call a function without
     setting up a stack frame for it.  On these machines, we
     define this macro to take two args; a frameinfo pointer
     identifying a frame and a variable to set or clear if it is
     or isn't leafless.  */
#ifdef FRAMELESS_FUNCTION_INVOCATION
  /* Still don't want to worry about this except on the innermost
     frame.  This macro will set FROMLEAF if NEXT_FRAME is a
     frameless function invocation.  */
  if (!(next_frame->next))
    {
      FRAMELESS_FUNCTION_INVOCATION (next_frame, fromleaf);
      if (fromleaf)
	address = next_frame->frame;
    }
#endif

  if (!fromleaf)
    {
      /* Two macros defined in param.h specify the machine-dependent
	 actions to be performed here.
	 First, get the frame's chain-pointer.
	 If that is zero, the frame is the outermost frame or a leaf
	 called by the outermost frame.  This means that if start
	 calls main without a frame, we'll return 0 (which is fine
	 anyway).

	 Nope; there's a problem.  This also returns when the current
	 routine is a leaf of main.  This is unacceptable.  We move
	 this to after the ffi test; I'd rather have backtraces from
	 start go curfluy than have an abort called from main not show
	 main.  */
      address = FRAME_CHAIN (next_frame);
      if (!FRAME_CHAIN_VALID (address, next_frame))
	return 0;
      /* If this frame is a leaf, this will be superceeded by the
	 code below.  */
      address = FRAME_CHAIN_COMBINE (address, next_frame);
    }
  if (address == 0)
    return 0;

  prev = (struct frame_info *)
    obstack_alloc (&frame_cache_obstack,
		   sizeof (struct frame_info));

  if (next_frame)
    next_frame->prev = prev;
  prev->next = next_frame;
  prev->prev = (struct frame_info *) 0;
  prev->frame = address;
  prev->next_frame = prev->next ? prev->next->frame : 0;

#ifdef INIT_EXTRA_FRAME_INFO
  INIT_EXTRA_FRAME_INFO(prev);
#endif

  /* This entry is in the frame queue now, which is good since
     FRAME_SAVED_PC may use that queue to figure out it's value
     (see m-sparc.h).  We want the pc saved in the inferior frame. */
  prev->pc = (fromleaf ? SAVED_PC_AFTER_CALL (next_frame) :
	      next_frame ? FRAME_SAVED_PC (next_frame) : read_pc ());

  return prev;
}

CORE_ADDR
get_frame_pc (frame)
     FRAME frame;
{
  struct frame_info *fi;
  fi = get_frame_info (frame);
  return fi->pc;
}

/* Find the addresses in which registers are saved in FRAME.  */

void
get_frame_saved_regs (frame_info_addr, saved_regs_addr)
     struct frame_info *frame_info_addr;
     struct frame_saved_regs *saved_regs_addr;
{
  FRAME_FIND_SAVED_REGS (frame_info_addr, *saved_regs_addr);
}

/* Return the innermost lexical block in execution
   in a specified stack frame.  The frame address is assumed valid.  */

struct block *
get_frame_block (frame)
     FRAME frame;
{
  struct frame_info *fi;
  CORE_ADDR pc;

  fi = get_frame_info (frame);

  pc = fi->pc;
  if (fi->next_frame != 0)
    /* We are not in the innermost frame.  We need to subtract one to
       get the correct block, in case the call instruction was the
       last instruction of the block.  If there are any machines on
       which the saved pc does not point to after the call insn, we
       probably want to make fi->pc point after the call insn anyway.  */
    --pc;
  return block_for_pc (pc);
}

struct block *
get_current_block ()
{
  return block_for_pc (read_pc ());
}

CORE_ADDR
get_pc_function_start (pc)
     CORE_ADDR pc;
{
  register struct block *bl = block_for_pc (pc);
  register struct symbol *symbol;
  if (bl == 0 || (symbol = block_function (bl)) == 0)
    {
      register int misc_index = find_pc_misc_function (pc);
      if (misc_index >= 0)
	return misc_function_vector[misc_index].address;
      return 0;
    }
  bl = SYMBOL_BLOCK_VALUE (symbol);
  return BLOCK_START (bl);
}

/* Return the symbol for the function executing in frame FRAME.  */

struct symbol *
get_frame_function (frame)
     FRAME frame;
{
  register struct block *bl = get_frame_block (frame);
  if (bl == 0)
    return 0;
  return block_function (bl);
}

/* Return the innermost lexical block containing the specified pc value,
   or 0 if there is none.  */

extern struct symtab *psymtab_to_symtab ();

struct block *
block_for_pc (pc)
     register CORE_ADDR pc;
{
  register struct block *b;
  register int bot, top, half;
  register struct symtab *s;
  register struct partial_symtab *ps;
  struct blockvector *bl;

  /* First search all symtabs for one whose file contains our pc */

  for (s = symtab_list; s; s = s->next)
    {
      bl = BLOCKVECTOR (s);
      b = BLOCKVECTOR_BLOCK (bl, 0);
      if (BLOCK_START (b) <= pc
	  && BLOCK_END (b) > pc)
	break;
    }

  if (s == 0)
    for (ps = partial_symtab_list; ps; ps = ps->next)
      {
	if (ps->textlow <= pc
	    && ps->texthigh > pc)
	  {
	    if (ps->readin)
	      fatal ("Internal error: pc found in readin psymtab and not in any symtab.");
	    s = psymtab_to_symtab (ps);
	    bl = BLOCKVECTOR (s);
	    b = BLOCKVECTOR_BLOCK (bl, 0);
	    break;
	  }
      }

  if (s == 0)
    return 0;

  /* Then search that symtab for the smallest block that wins.  */
  /* Use binary search to find the last block that starts before PC.  */

  bot = 0;
  top = BLOCKVECTOR_NBLOCKS (bl);

  while (top - bot > 1)
    {
      half = (top - bot + 1) >> 1;
      b = BLOCKVECTOR_BLOCK (bl, bot + half);
      if (BLOCK_START (b) <= pc)
	bot += half;
      else
	top = bot + half;
    }

  /* Now search backward for a block that ends after PC.  */

  while (bot >= 0)
    {
      b = BLOCKVECTOR_BLOCK (bl, bot);
      if (BLOCK_END (b) > pc)
	return b;
      bot--;
    }

  return 0;
}

/* Return the function containing pc value PC.
   Returns 0 if function is not known.  */

struct symbol *
find_pc_function (pc)
     CORE_ADDR pc;
{
  register struct block *b = block_for_pc (pc);
  if (b == 0)
    return 0;
  return block_function (b);
}

/* Finds the "function" (text symbol) that is smaller than PC
   but greatest of all of the potential text symbols.  Sets
   *NAME and/or *ADDRESS conditionally if that pointer is non-zero.
   Returns 0 if it couldn't find anything, 1 if it did.  On a zero
   return, *NAME and *ADDRESS are always set to zero.  On a 1 return,
   *NAME and *ADDRESS contain real information.  */

int
find_pc_partial_function (pc, name, address)
     CORE_ADDR pc;
     char **name;
     CORE_ADDR *address;
{
  struct partial_symtab *pst = find_pc_psymtab (pc);
  struct symbol *f;
  int miscfunc;
  struct partial_symbol *psb;

  if (pst)
    {
      if (pst->readin)
	{
	  /* The information we want has already been read in.
	     We can go to the already readin symbols and we'll get
	     the best possible answer.  */
	  f = find_pc_function (pc);
	  if (!f)
	    {
	    return_error:
	      /* No availible symbol.  */
	      if (name != 0)
		*name = 0;
	      if (address != 0)
		*address = 0;
	      return 0;
	    }

	  if (name)
	    *name = SYMBOL_NAME (f);
	  if (address)
	    *address = BLOCK_START (SYMBOL_BLOCK_VALUE (f));
	  return 1;
	}

      /* Get the information from a combination of the pst
	 (static symbols), and the misc function vector (extern
	 symbols).  */
      miscfunc = find_pc_misc_function (pc);
      psb = find_pc_psymbol (pst, pc);

      if (!psb && miscfunc == -1)
	{
	  goto return_error;
	}
      if (!psb
	  || (miscfunc != -1
	      && (SYMBOL_VALUE(psb)
		  < misc_function_vector[miscfunc].address)))
	{
	  if (address)
	    *address = misc_function_vector[miscfunc].address;
	  if (name)
	    *name = misc_function_vector[miscfunc].name;
	  return 1;
	}
      else
	{
	  if (address)
	    *address = SYMBOL_VALUE (psb);
	  if (name)
	    *name = SYMBOL_NAME (psb);
	  return 1;
	}
    }
  else
    /* Must be in the misc function stuff.  */
    {
      miscfunc = find_pc_misc_function (pc);
      if (miscfunc == -1)
	goto return_error;
      if (address)
	*address = misc_function_vector[miscfunc].address;
      if (name)
	*name = misc_function_vector[miscfunc].name;
      return 1;
    }
}

/* Find the misc function whose address is the largest
   while being less than PC.  Return its index in misc_function_vector.
   Returns -1 if PC is not in suitable range.  */

int
find_pc_misc_function (pc)
     register CORE_ADDR pc;
{
  register int lo = 0;
  register int hi = misc_function_count-1;
  register int new;
  register int distance;

  /* Note that the last thing in the vector is always _etext.  */
  /* Actually, "end", now that non-functions
     go on the misc_function_vector.  */

  /* Above statement is not *always* true - fix for case where there are */
  /* no misc functions at all (ie no symbol table has been read). */
  if (hi < 0) return -1;        /* no misc functions recorded */

  /* trivial reject range test */
  if (pc < misc_function_vector[0].address ||
      pc > misc_function_vector[hi].address)
    return -1;

  /* Note that the following search will not return hi if
     pc == misc_function_vector[hi].address.  If "end" points to the
     first unused location, this is correct and the above test
     simply needs to be changed to
     "pc >= misc_function_vector[hi].address".  */
  do {
    new = (lo + hi) >> 1;
    distance = misc_function_vector[new].address - pc;
    if (distance == 0)
      return new;		/* an exact match */
    else if (distance > 0)
      hi = new;
    else
      lo = new;
  } while (hi-lo != 1);

  /* if here, we had no exact match, so return the lower choice */
  return lo;
}

/* Return the innermost stack frame executing inside of the specified block,
   or zero if there is no such frame.  */

FRAME
block_innermost_frame (block)
     struct block *block;
{
  struct frame_info *fi;
  register FRAME frame;
  register CORE_ADDR start = BLOCK_START (block);
  register CORE_ADDR end = BLOCK_END (block);

  frame = 0;
  while (1)
    {
      frame = get_prev_frame (frame);
      if (frame == 0)
	return 0;
      fi = get_frame_info (frame);
      if (fi->pc >= start && fi->pc < end)
	return frame;
    }
}

void
_initialize_blockframe ()
{
  obstack_init (&frame_cache_obstack);
}
