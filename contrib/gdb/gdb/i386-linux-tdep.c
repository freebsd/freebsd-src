/* Target-dependent code for GNU/Linux running on i386's, for GDB.

   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "frame.h"
#include "value.h"
#include "regcache.h"
#include "inferior.h"

/* For i386_linux_skip_solib_resolver.  */
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"

#include "solib-svr4.h"		/* For struct link_map_offsets.  */

/* Return the name of register REG.  */

char *
i386_linux_register_name (int reg)
{
  /* Deal with the extra "orig_eax" pseudo register.  */
  if (reg == I386_LINUX_ORIG_EAX_REGNUM)
    return "orig_eax";

  return i386_register_name (reg);
}

int
i386_linux_register_byte (int reg)
{
  /* Deal with the extra "orig_eax" pseudo register.  */
  if (reg == I386_LINUX_ORIG_EAX_REGNUM)
    return (i386_register_byte (I386_LINUX_ORIG_EAX_REGNUM - 1)
	    + i386_register_raw_size (I386_LINUX_ORIG_EAX_REGNUM - 1));

  return i386_register_byte (reg);
}

int
i386_linux_register_raw_size (int reg)
{
  /* Deal with the extra "orig_eax" pseudo register.  */
  if (reg == I386_LINUX_ORIG_EAX_REGNUM)
    return 4;

  return i386_register_raw_size (reg);
}

/* Recognizing signal handler frames.  */

/* GNU/Linux has two flavors of signals.  Normal signal handlers, and
   "realtime" (RT) signals.  The RT signals can provide additional
   information to the signal handler if the SA_SIGINFO flag is set
   when establishing a signal handler using `sigaction'.  It is not
   unlikely that future versions of GNU/Linux will support SA_SIGINFO
   for normal signals too.  */

/* When the i386 Linux kernel calls a signal handler and the
   SA_RESTORER flag isn't set, the return address points to a bit of
   code on the stack.  This function returns whether the PC appears to
   be within this bit of code.

   The instruction sequence for normal signals is
       pop    %eax
       mov    $0x77,%eax
       int    $0x80
   or 0x58 0xb8 0x77 0x00 0x00 0x00 0xcd 0x80.

   Checking for the code sequence should be somewhat reliable, because
   the effect is to call the system call sigreturn.  This is unlikely
   to occur anywhere other than a signal trampoline.

   It kind of sucks that we have to read memory from the process in
   order to identify a signal trampoline, but there doesn't seem to be
   any other way.  The IN_SIGTRAMP macro in tm-linux.h arranges to
   only call us if no function name could be identified, which should
   be the case since the code is on the stack.

   Detection of signal trampolines for handlers that set the
   SA_RESTORER flag is in general not possible.  Unfortunately this is
   what the GNU C Library has been doing for quite some time now.
   However, as of version 2.1.2, the GNU C Library uses signal
   trampolines (named __restore and __restore_rt) that are identical
   to the ones used by the kernel.  Therefore, these trampolines are
   supported too.  */

#define LINUX_SIGTRAMP_INSN0 (0x58)	/* pop %eax */
#define LINUX_SIGTRAMP_OFFSET0 (0)
#define LINUX_SIGTRAMP_INSN1 (0xb8)	/* mov $NNNN,%eax */
#define LINUX_SIGTRAMP_OFFSET1 (1)
#define LINUX_SIGTRAMP_INSN2 (0xcd)	/* int */
#define LINUX_SIGTRAMP_OFFSET2 (6)

static const unsigned char linux_sigtramp_code[] =
{
  LINUX_SIGTRAMP_INSN0,					/* pop %eax */
  LINUX_SIGTRAMP_INSN1, 0x77, 0x00, 0x00, 0x00,		/* mov $0x77,%eax */
  LINUX_SIGTRAMP_INSN2, 0x80				/* int $0x80 */
};

#define LINUX_SIGTRAMP_LEN (sizeof linux_sigtramp_code)

/* If PC is in a sigtramp routine, return the address of the start of
   the routine.  Otherwise, return 0.  */

static CORE_ADDR
i386_linux_sigtramp_start (CORE_ADDR pc)
{
  unsigned char buf[LINUX_SIGTRAMP_LEN];

  /* We only recognize a signal trampoline if PC is at the start of
     one of the three instructions.  We optimize for finding the PC at
     the start, as will be the case when the trampoline is not the
     first frame on the stack.  We assume that in the case where the
     PC is not at the start of the instruction sequence, there will be
     a few trailing readable bytes on the stack.  */

  if (read_memory_nobpt (pc, (char *) buf, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  if (buf[0] != LINUX_SIGTRAMP_INSN0)
    {
      int adjust;

      switch (buf[0])
	{
	case LINUX_SIGTRAMP_INSN1:
	  adjust = LINUX_SIGTRAMP_OFFSET1;
	  break;
	case LINUX_SIGTRAMP_INSN2:
	  adjust = LINUX_SIGTRAMP_OFFSET2;
	  break;
	default:
	  return 0;
	}

      pc -= adjust;

      if (read_memory_nobpt (pc, (char *) buf, LINUX_SIGTRAMP_LEN) != 0)
	return 0;
    }

  if (memcmp (buf, linux_sigtramp_code, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

/* This function does the same for RT signals.  Here the instruction
   sequence is
       mov    $0xad,%eax
       int    $0x80
   or 0xb8 0xad 0x00 0x00 0x00 0xcd 0x80.

   The effect is to call the system call rt_sigreturn.  */

#define LINUX_RT_SIGTRAMP_INSN0 (0xb8)	/* mov $NNNN,%eax */
#define LINUX_RT_SIGTRAMP_OFFSET0 (0)
#define LINUX_RT_SIGTRAMP_INSN1 (0xcd)	/* int */
#define LINUX_RT_SIGTRAMP_OFFSET1 (5)

static const unsigned char linux_rt_sigtramp_code[] =
{
  LINUX_RT_SIGTRAMP_INSN0, 0xad, 0x00, 0x00, 0x00,	/* mov $0xad,%eax */
  LINUX_RT_SIGTRAMP_INSN1, 0x80				/* int $0x80 */
};

#define LINUX_RT_SIGTRAMP_LEN (sizeof linux_rt_sigtramp_code)

/* If PC is in a RT sigtramp routine, return the address of the start
   of the routine.  Otherwise, return 0.  */

static CORE_ADDR
i386_linux_rt_sigtramp_start (CORE_ADDR pc)
{
  unsigned char buf[LINUX_RT_SIGTRAMP_LEN];

  /* We only recognize a signal trampoline if PC is at the start of
     one of the two instructions.  We optimize for finding the PC at
     the start, as will be the case when the trampoline is not the
     first frame on the stack.  We assume that in the case where the
     PC is not at the start of the instruction sequence, there will be
     a few trailing readable bytes on the stack.  */

  if (read_memory_nobpt (pc, (char *) buf, LINUX_RT_SIGTRAMP_LEN) != 0)
    return 0;

  if (buf[0] != LINUX_RT_SIGTRAMP_INSN0)
    {
      if (buf[0] != LINUX_RT_SIGTRAMP_INSN1)
	return 0;

      pc -= LINUX_RT_SIGTRAMP_OFFSET1;

      if (read_memory_nobpt (pc, (char *) buf, LINUX_RT_SIGTRAMP_LEN) != 0)
	return 0;
    }

  if (memcmp (buf, linux_rt_sigtramp_code, LINUX_RT_SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

/* Return whether PC is in a GNU/Linux sigtramp routine.  */

int
i386_linux_in_sigtramp (CORE_ADDR pc, char *name)
{
  if (name)
    return STREQ ("__restore", name) || STREQ ("__restore_rt", name);
  
  return (i386_linux_sigtramp_start (pc) != 0
	  || i386_linux_rt_sigtramp_start (pc) != 0);
}

/* Assuming FRAME is for a GNU/Linux sigtramp routine, return the
   address of the associated sigcontext structure.  */

CORE_ADDR
i386_linux_sigcontext_addr (struct frame_info *frame)
{
  CORE_ADDR pc;

  pc = i386_linux_sigtramp_start (frame->pc);
  if (pc)
    {
      CORE_ADDR sp;

      if (frame->next)
	/* If this isn't the top frame, the next frame must be for the
	   signal handler itself.  The sigcontext structure lives on
	   the stack, right after the signum argument.  */
	return frame->next->frame + 12;

      /* This is the top frame.  We'll have to find the address of the
	 sigcontext structure by looking at the stack pointer.  Keep
	 in mind that the first instruction of the sigtramp code is
	 "pop %eax".  If the PC is at this instruction, adjust the
	 returned value accordingly.  */
      sp = read_register (SP_REGNUM);
      if (pc == frame->pc)
	return sp + 4;
      return sp;
    }

  pc = i386_linux_rt_sigtramp_start (frame->pc);
  if (pc)
    {
      if (frame->next)
	/* If this isn't the top frame, the next frame must be for the
	   signal handler itself.  The sigcontext structure is part of
	   the user context.  A pointer to the user context is passed
	   as the third argument to the signal handler.  */
	return read_memory_integer (frame->next->frame + 16, 4) + 20;

      /* This is the top frame.  Again, use the stack pointer to find
	 the address of the sigcontext structure.  */
      return read_memory_integer (read_register (SP_REGNUM) + 8, 4) + 20;
    }

  error ("Couldn't recognize signal trampoline.");
  return 0;
}

/* Offset to saved PC in sigcontext, from <asm/sigcontext.h>.  */
#define LINUX_SIGCONTEXT_PC_OFFSET (56)

/* Assuming FRAME is for a GNU/Linux sigtramp routine, return the
   saved program counter.  */

static CORE_ADDR
i386_linux_sigtramp_saved_pc (struct frame_info *frame)
{
  CORE_ADDR addr;
  addr = i386_linux_sigcontext_addr (frame);
  return read_memory_integer (addr + LINUX_SIGCONTEXT_PC_OFFSET, 4);
}

/* Offset to saved SP in sigcontext, from <asm/sigcontext.h>.  */
#define LINUX_SIGCONTEXT_SP_OFFSET (28)

/* Assuming FRAME is for a GNU/Linux sigtramp routine, return the
   saved stack pointer.  */

static CORE_ADDR
i386_linux_sigtramp_saved_sp (struct frame_info *frame)
{
  CORE_ADDR addr;
  addr = i386_linux_sigcontext_addr (frame);
  return read_memory_integer (addr + LINUX_SIGCONTEXT_SP_OFFSET, 4);
}

/* Signal trampolines don't have a meaningful frame.  As in
   "i386/tm-i386.h", the frame pointer value we use is actually the
   frame pointer of the calling frame -- that is, the frame which was
   in progress when the signal trampoline was entered.  GDB mostly
   treats this frame pointer value as a magic cookie.  We detect the
   case of a signal trampoline by looking at the SIGNAL_HANDLER_CALLER
   field, which is set based on IN_SIGTRAMP.

   When a signal trampoline is invoked from a frameless function, we
   essentially have two frameless functions in a row.  In this case,
   we use the same magic cookie for three frames in a row.  We detect
   this case by seeing whether the next frame has
   SIGNAL_HANDLER_CALLER set, and, if it does, checking whether the
   current frame is actually frameless.  In this case, we need to get
   the PC by looking at the SP register value stored in the signal
   context.

   This should work in most cases except in horrible situations where
   a signal occurs just as we enter a function but before the frame
   has been set up.  */

#define FRAMELESS_SIGNAL(frame)					\
  ((frame)->next != NULL					\
   && (frame)->next->signal_handler_caller			\
   && frameless_look_for_prologue (frame))

CORE_ADDR
i386_linux_frame_chain (struct frame_info *frame)
{
  if (frame->signal_handler_caller || FRAMELESS_SIGNAL (frame))
    return frame->frame;

  if (! inside_entry_file (frame->pc))
    return read_memory_unsigned_integer (frame->frame, 4);

  return 0;
}

/* Return the saved program counter for FRAME.  */

CORE_ADDR
i386_linux_frame_saved_pc (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return i386_linux_sigtramp_saved_pc (frame);

  if (FRAMELESS_SIGNAL (frame))
    {
      CORE_ADDR sp = i386_linux_sigtramp_saved_sp (frame->next);
      return read_memory_unsigned_integer (sp, 4);
    }

  return read_memory_unsigned_integer (frame->frame + 4, 4);
}

/* Immediately after a function call, return the saved pc.  */

CORE_ADDR
i386_linux_saved_pc_after_call (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return i386_linux_sigtramp_saved_pc (frame);

  return read_memory_unsigned_integer (read_register (SP_REGNUM), 4);
}

/* Set the program counter for process PTID to PC.  */

void
i386_linux_write_pc (CORE_ADDR pc, ptid_t ptid)
{
  write_register_pid (PC_REGNUM, pc, ptid);

  /* We must be careful with modifying the program counter.  If we
     just interrupted a system call, the kernel might try to restart
     it when we resume the inferior.  On restarting the system call,
     the kernel will try backing up the program counter even though it
     no longer points at the system call.  This typically results in a
     SIGSEGV or SIGILL.  We can prevent this by writing `-1' in the
     "orig_eax" pseudo-register.

     Note that "orig_eax" is saved when setting up a dummy call frame.
     This means that it is properly restored when that frame is
     popped, and that the interrupted system call will be restarted
     when we resume the inferior on return from a function call from
     within GDB.  In all other cases the system call will not be
     restarted.  */
  write_register_pid (I386_LINUX_ORIG_EAX_REGNUM, -1, ptid);
}

/* Calling functions in shared libraries.  */

/* Find the minimal symbol named NAME, and return both the minsym
   struct and its objfile.  This probably ought to be in minsym.c, but
   everything there is trying to deal with things like C++ and
   SOFUN_ADDRESS_MAYBE_TURQUOISE, ...  Since this is so simple, it may
   be considered too special-purpose for general consumption.  */

static struct minimal_symbol *
find_minsym_and_objfile (char *name, struct objfile **objfile_p)
{
  struct objfile *objfile;

  ALL_OBJFILES (objfile)
    {
      struct minimal_symbol *msym;

      ALL_OBJFILE_MSYMBOLS (objfile, msym)
	{
	  if (SYMBOL_NAME (msym)
	      && STREQ (SYMBOL_NAME (msym), name))
	    {
	      *objfile_p = objfile;
	      return msym;
	    }
	}
    }

  return 0;
}

static CORE_ADDR
skip_hurd_resolver (CORE_ADDR pc)
{
  /* The HURD dynamic linker is part of the GNU C library, so many
     GNU/Linux distributions use it.  (All ELF versions, as far as I
     know.)  An unresolved PLT entry points to "_dl_runtime_resolve",
     which calls "fixup" to patch the PLT, and then passes control to
     the function.

     We look for the symbol `_dl_runtime_resolve', and find `fixup' in
     the same objfile.  If we are at the entry point of `fixup', then
     we set a breakpoint at the return address (at the top of the
     stack), and continue.
  
     It's kind of gross to do all these checks every time we're
     called, since they don't change once the executable has gotten
     started.  But this is only a temporary hack --- upcoming versions
     of GNU/Linux will provide a portable, efficient interface for
     debugging programs that use shared libraries.  */

  struct objfile *objfile;
  struct minimal_symbol *resolver 
    = find_minsym_and_objfile ("_dl_runtime_resolve", &objfile);

  if (resolver)
    {
      struct minimal_symbol *fixup
	= lookup_minimal_symbol ("fixup", NULL, objfile);

      if (fixup && SYMBOL_VALUE_ADDRESS (fixup) == pc)
	return (SAVED_PC_AFTER_CALL (get_current_frame ()));
    }

  return 0;
}      

/* See the comments for SKIP_SOLIB_RESOLVER at the top of infrun.c.
   This function:
   1) decides whether a PLT has sent us into the linker to resolve
      a function reference, and 
   2) if so, tells us where to set a temporary breakpoint that will
      trigger when the dynamic linker is done.  */

CORE_ADDR
i386_linux_skip_solib_resolver (CORE_ADDR pc)
{
  CORE_ADDR result;

  /* Plug in functions for other kinds of resolvers here.  */
  result = skip_hurd_resolver (pc);
  if (result)
    return result;

  return 0;
}

/* Fetch (and possibly build) an appropriate link_map_offsets
   structure for native GNU/Linux x86 targets using the struct offsets
   defined in link.h (but without actual reference to that file).

   This makes it possible to access GNU/Linux x86 shared libraries
   from a GDB that was not built on an GNU/Linux x86 host (for cross
   debugging).  */

struct link_map_offsets *
i386_linux_svr4_fetch_link_map_offsets (void)
{
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL)
    {
      lmp = &lmo;

      lmo.r_debug_size = 8;	/* The actual size is 20 bytes, but
				   this is all we need.  */
      lmo.r_map_offset = 4;
      lmo.r_map_size   = 4;

      lmo.link_map_size = 20;	/* The actual size is 552 bytes, but
				   this is all we need.  */
      lmo.l_addr_offset = 0;
      lmo.l_addr_size   = 4;

      lmo.l_name_offset = 4;
      lmo.l_name_size   = 4;

      lmo.l_next_offset = 12;
      lmo.l_next_size   = 4;

      lmo.l_prev_offset = 16;
      lmo.l_prev_size   = 4;
    }

  return lmp;
}
