/* Intel 386 target-dependent stuff.
   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997,
   1998, 1999, 2000, 2001
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdb_string.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include "floatformat.h"
#include "symtab.h"
#include "gdbcmd.h"
#include "command.h"
#include "arch-utils.h"
#include "regcache.h"
#include "doublest.h"
#include "value.h"
#include "gdb_assert.h"

#include "elf-bfd.h"

#include "i386-tdep.h"

#undef XMALLOC
#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

/* Names of the registers.  The first 10 registers match the register
   numbering scheme used by GCC for stabs and DWARF.  */
static char *i386_register_names[] =
{
  "eax",   "ecx",    "edx",   "ebx",
  "esp",   "ebp",    "esi",   "edi",
  "eip",   "eflags", "cs",    "ss",
  "ds",    "es",     "fs",    "gs",
  "st0",   "st1",    "st2",   "st3",
  "st4",   "st5",    "st6",   "st7",
  "fctrl", "fstat",  "ftag",  "fiseg",
  "fioff", "foseg",  "fooff", "fop",
  "xmm0",  "xmm1",   "xmm2",  "xmm3",
  "xmm4",  "xmm5",   "xmm6",  "xmm7",
  "mxcsr"
};

/* i386_register_offset[i] is the offset into the register file of the
   start of register number i.  We initialize this from
   i386_register_size.  */
static int i386_register_offset[MAX_NUM_REGS];

/* i386_register_size[i] is the number of bytes of storage in GDB's
   register array occupied by register i.  */
static int i386_register_size[MAX_NUM_REGS] = {
   4,  4,  4,  4,
   4,  4,  4,  4,
   4,  4,  4,  4,
   4,  4,  4,  4,
  10, 10, 10, 10,
  10, 10, 10, 10,
   4,  4,  4,  4,
   4,  4,  4,  4,
  16, 16, 16, 16,
  16, 16, 16, 16,
   4
};

/* Return the name of register REG.  */

char *
i386_register_name (int reg)
{
  if (reg < 0)
    return NULL;
  if (reg >= sizeof (i386_register_names) / sizeof (*i386_register_names))
    return NULL;

  return i386_register_names[reg];
}

/* Return the offset into the register array of the start of register
   number REG.  */
int
i386_register_byte (int reg)
{
  return i386_register_offset[reg];
}

/* Return the number of bytes of storage in GDB's register array
   occupied by register REG.  */

int
i386_register_raw_size (int reg)
{
  return i386_register_size[reg];
}

/* Return the size in bytes of the virtual type of register REG.  */

int
i386_register_virtual_size (int reg)
{
  return TYPE_LENGTH (REGISTER_VIRTUAL_TYPE (reg));
}

/* Convert stabs register number REG to the appropriate register
   number used by GDB.  */

int
i386_stab_reg_to_regnum (int reg)
{
  /* This implements what GCC calls the "default" register map.  */
  if (reg >= 0 && reg <= 7)
    {
      /* General registers.  */
      return reg;
    }
  else if (reg >= 12 && reg <= 19)
    {
      /* Floating-point registers.  */
      return reg - 12 + FP0_REGNUM;
    }
  else if (reg >= 21 && reg <= 28)
    {
      /* SSE registers.  */
      return reg - 21 + XMM0_REGNUM;
    }
  else if (reg >= 29 && reg <= 36)
    {
      /* MMX registers.  */
      /* FIXME: kettenis/2001-07-28: Should we have the MMX registers
         as pseudo-registers?  */
      return reg - 29 + FP0_REGNUM;
    }

  /* This will hopefully provoke a warning.  */
  return NUM_REGS + NUM_PSEUDO_REGS;
}

/* Convert Dwarf register number REG to the appropriate register
   number used by GDB.  */

int
i386_dwarf_reg_to_regnum (int reg)
{
  /* The DWARF register numbering includes %eip and %eflags, and
     numbers the floating point registers differently.  */
  if (reg >= 0 && reg <= 9)
    {
      /* General registers.  */
      return reg;
    }
  else if (reg >= 11 && reg <= 18)
    {
      /* Floating-point registers.  */
      return reg - 11 + FP0_REGNUM;
    }
  else if (reg >= 21)
    {
      /* The SSE and MMX registers have identical numbers as in stabs.  */
      return i386_stab_reg_to_regnum (reg);
    }

  /* This will hopefully provoke a warning.  */
  return NUM_REGS + NUM_PSEUDO_REGS;
}


/* This is the variable that is set with "set disassembly-flavor", and
   its legitimate values.  */
static const char att_flavor[] = "att";
static const char intel_flavor[] = "intel";
static const char *valid_flavors[] =
{
  att_flavor,
  intel_flavor,
  NULL
};
static const char *disassembly_flavor = att_flavor;

/* Stdio style buffering was used to minimize calls to ptrace, but
   this buffering did not take into account that the code section
   being accessed may not be an even number of buffers long (even if
   the buffer is only sizeof(int) long).  In cases where the code
   section size happened to be a non-integral number of buffers long,
   attempting to read the last buffer would fail.  Simply using
   target_read_memory and ignoring errors, rather than read_memory, is
   not the correct solution, since legitimate access errors would then
   be totally ignored.  To properly handle this situation and continue
   to use buffering would require that this code be able to determine
   the minimum code section size granularity (not the alignment of the
   section itself, since the actual failing case that pointed out this
   problem had a section alignment of 4 but was not a multiple of 4
   bytes long), on a target by target basis, and then adjust it's
   buffer size accordingly.  This is messy, but potentially feasible.
   It probably needs the bfd library's help and support.  For now, the
   buffer size is set to 1.  (FIXME -fnf) */

#define CODESTREAM_BUFSIZ 1	/* Was sizeof(int), see note above.  */
static CORE_ADDR codestream_next_addr;
static CORE_ADDR codestream_addr;
static unsigned char codestream_buf[CODESTREAM_BUFSIZ];
static int codestream_off;
static int codestream_cnt;

#define codestream_tell() (codestream_addr + codestream_off)
#define codestream_peek() \
  (codestream_cnt == 0 ? \
   codestream_fill(1) : codestream_buf[codestream_off])
#define codestream_get() \
  (codestream_cnt-- == 0 ? \
   codestream_fill(0) : codestream_buf[codestream_off++])

static unsigned char
codestream_fill (int peek_flag)
{
  codestream_addr = codestream_next_addr;
  codestream_next_addr += CODESTREAM_BUFSIZ;
  codestream_off = 0;
  codestream_cnt = CODESTREAM_BUFSIZ;
  read_memory (codestream_addr, (char *) codestream_buf, CODESTREAM_BUFSIZ);

  if (peek_flag)
    return (codestream_peek ());
  else
    return (codestream_get ());
}

static void
codestream_seek (CORE_ADDR place)
{
  codestream_next_addr = place / CODESTREAM_BUFSIZ;
  codestream_next_addr *= CODESTREAM_BUFSIZ;
  codestream_cnt = 0;
  codestream_fill (1);
  while (codestream_tell () != place)
    codestream_get ();
}

static void
codestream_read (unsigned char *buf, int count)
{
  unsigned char *p;
  int i;
  p = buf;
  for (i = 0; i < count; i++)
    *p++ = codestream_get ();
}


/* If the next instruction is a jump, move to its target.  */

static void
i386_follow_jump (void)
{
  unsigned char buf[4];
  long delta;

  int data16;
  CORE_ADDR pos;

  pos = codestream_tell ();

  data16 = 0;
  if (codestream_peek () == 0x66)
    {
      codestream_get ();
      data16 = 1;
    }

  switch (codestream_get ())
    {
    case 0xe9:
      /* Relative jump: if data16 == 0, disp32, else disp16.  */
      if (data16)
	{
	  codestream_read (buf, 2);
	  delta = extract_signed_integer (buf, 2);

	  /* Include the size of the jmp instruction (including the
             0x66 prefix).  */
	  pos += delta + 4;
	}
      else
	{
	  codestream_read (buf, 4);
	  delta = extract_signed_integer (buf, 4);

	  pos += delta + 5;
	}
      break;
    case 0xeb:
      /* Relative jump, disp8 (ignore data16).  */
      codestream_read (buf, 1);
      /* Sign-extend it.  */
      delta = extract_signed_integer (buf, 1);

      pos += delta + 2;
      break;
    }
  codestream_seek (pos);
}

/* Find & return the amount a local space allocated, and advance the
   codestream to the first register push (if any).

   If the entry sequence doesn't make sense, return -1, and leave
   codestream pointer at a random spot.  */

static long
i386_get_frame_setup (CORE_ADDR pc)
{
  unsigned char op;

  codestream_seek (pc);

  i386_follow_jump ();

  op = codestream_get ();

  if (op == 0x58)		/* popl %eax */
    {
      /* This function must start with

	    popl %eax             0x58
            xchgl %eax, (%esp)    0x87 0x04 0x24
         or xchgl %eax, 0(%esp)   0x87 0x44 0x24 0x00

	 (the System V compiler puts out the second `xchg'
	 instruction, and the assembler doesn't try to optimize it, so
	 the 'sib' form gets generated).  This sequence is used to get
	 the address of the return buffer for a function that returns
	 a structure.  */
      int pos;
      unsigned char buf[4];
      static unsigned char proto1[3] = { 0x87, 0x04, 0x24 };
      static unsigned char proto2[4] = { 0x87, 0x44, 0x24, 0x00 };

      pos = codestream_tell ();
      codestream_read (buf, 4);
      if (memcmp (buf, proto1, 3) == 0)
	pos += 3;
      else if (memcmp (buf, proto2, 4) == 0)
	pos += 4;

      codestream_seek (pos);
      op = codestream_get ();	/* Update next opcode.  */
    }

  if (op == 0x68 || op == 0x6a)
    {
      /* This function may start with

            pushl constant
            call _probe
	    addl $4, %esp
	   
	 followed by

            pushl %ebp

	 etc.  */
      int pos;
      unsigned char buf[8];

      /* Skip past the `pushl' instruction; it has either a one-byte 
         or a four-byte operand, depending on the opcode.  */
      pos = codestream_tell ();
      if (op == 0x68)
	pos += 4;
      else
	pos += 1;
      codestream_seek (pos);

      /* Read the following 8 bytes, which should be "call _probe" (6
         bytes) followed by "addl $4,%esp" (2 bytes).  */
      codestream_read (buf, sizeof (buf));
      if (buf[0] == 0xe8 && buf[6] == 0xc4 && buf[7] == 0x4)
	pos += sizeof (buf);
      codestream_seek (pos);
      op = codestream_get ();	/* Update next opcode.  */
    }

  if (op == 0x55)		/* pushl %ebp */
    {
      /* Check for "movl %esp, %ebp" -- can be written in two ways.  */
      switch (codestream_get ())
	{
	case 0x8b:
	  if (codestream_get () != 0xec)
	    return -1;
	  break;
	case 0x89:
	  if (codestream_get () != 0xe5)
	    return -1;
	  break;
	default:
	  return -1;
	}
      /* Check for stack adjustment 

           subl $XXX, %esp

	 NOTE: You can't subtract a 16 bit immediate from a 32 bit
	 reg, so we don't have to worry about a data16 prefix.  */
      op = codestream_peek ();
      if (op == 0x83)
	{
	  /* `subl' with 8 bit immediate.  */
	  codestream_get ();
	  if (codestream_get () != 0xec)
	    /* Some instruction starting with 0x83 other than `subl'.  */
	    {
	      codestream_seek (codestream_tell () - 2);
	      return 0;
	    }
	  /* `subl' with signed byte immediate (though it wouldn't
	     make sense to be negative).  */
	  return (codestream_get ());
	}
      else if (op == 0x81)
	{
	  char buf[4];
	  /* Maybe it is `subl' with a 32 bit immedediate.  */
	  codestream_get ();
	  if (codestream_get () != 0xec)
	    /* Some instruction starting with 0x81 other than `subl'.  */
	    {
	      codestream_seek (codestream_tell () - 2);
	      return 0;
	    }
	  /* It is `subl' with a 32 bit immediate.  */
	  codestream_read ((unsigned char *) buf, 4);
	  return extract_signed_integer (buf, 4);
	}
      else
	{
	  return 0;
	}
    }
  else if (op == 0xc8)
    {
      char buf[2];
      /* `enter' with 16 bit unsigned immediate.  */
      codestream_read ((unsigned char *) buf, 2);
      codestream_get ();	/* Flush final byte of enter instruction.  */
      return extract_unsigned_integer (buf, 2);
    }
  return (-1);
}

/* Return the chain-pointer for FRAME.  In the case of the i386, the
   frame's nominal address is the address of a 4-byte word containing
   the calling frame's address.  */

CORE_ADDR
i386_frame_chain (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return frame->frame;

  if (! inside_entry_file (frame->pc))
    return read_memory_unsigned_integer (frame->frame, 4);

  return 0;
}

/* Determine whether the function invocation represented by FRAME does
   not have a from on the stack associated with it.  If it does not,
   return non-zero, otherwise return zero.  */

int
i386_frameless_function_invocation (struct frame_info *frame)
{
  if (frame->signal_handler_caller)
    return 0;

  return frameless_look_for_prologue (frame);
}

/* Return the saved program counter for FRAME.  */

CORE_ADDR
i386_frame_saved_pc (struct frame_info *frame)
{
  /* FIXME: kettenis/2001-05-09: Conditionalizing the next bit of code
     on SIGCONTEXT_PC_OFFSET and I386V4_SIGTRAMP_SAVED_PC should be
     considered a temporary hack.  I plan to come up with something
     better when we go multi-arch.  */
#if defined (SIGCONTEXT_PC_OFFSET) || defined (I386V4_SIGTRAMP_SAVED_PC)
  if (frame->signal_handler_caller)
    return sigtramp_saved_pc (frame);
#endif

  return read_memory_unsigned_integer (frame->frame + 4, 4);
}

CORE_ADDR
i386go32_frame_saved_pc (struct frame_info *frame)
{
  return read_memory_integer (frame->frame + 4, 4);
}

/* Immediately after a function call, return the saved pc.  */

CORE_ADDR
i386_saved_pc_after_call (struct frame_info *frame)
{
  return read_memory_unsigned_integer (read_register (SP_REGNUM), 4);
}

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

int
i386_frame_num_args (struct frame_info *fi)
{
#if 1
  return -1;
#else
  /* This loses because not only might the compiler not be popping the
     args right after the function call, it might be popping args from
     both this call and a previous one, and we would say there are
     more args than there really are.  */

  int retpc;
  unsigned char op;
  struct frame_info *pfi;

  /* On the i386, the instruction following the call could be:
     popl %ecx        -  one arg
     addl $imm, %esp  -  imm/4 args; imm may be 8 or 32 bits
     anything else    -  zero args.  */

  int frameless;

  frameless = FRAMELESS_FUNCTION_INVOCATION (fi);
  if (frameless)
    /* In the absence of a frame pointer, GDB doesn't get correct
       values for nameless arguments.  Return -1, so it doesn't print
       any nameless arguments.  */
    return -1;

  pfi = get_prev_frame (fi);
  if (pfi == 0)
    {
      /* NOTE: This can happen if we are looking at the frame for
         main, because FRAME_CHAIN_VALID won't let us go into start.
         If we have debugging symbols, that's not really a big deal;
         it just means it will only show as many arguments to main as
         are declared.  */
      return -1;
    }
  else
    {
      retpc = pfi->pc;
      op = read_memory_integer (retpc, 1);
      if (op == 0x59)		/* pop %ecx */
	return 1;
      else if (op == 0x83)
	{
	  op = read_memory_integer (retpc + 1, 1);
	  if (op == 0xc4)
	    /* addl $<signed imm 8 bits>, %esp */
	    return (read_memory_integer (retpc + 2, 1) & 0xff) / 4;
	  else
	    return 0;
	}
      else if (op == 0x81)	/* `add' with 32 bit immediate.  */
	{
	  op = read_memory_integer (retpc + 1, 1);
	  if (op == 0xc4)
	    /* addl $<imm 32>, %esp */
	    return read_memory_integer (retpc + 2, 4) / 4;
	  else
	    return 0;
	}
      else
	{
	  return 0;
	}
    }
#endif
}

/* Parse the first few instructions the function to see what registers
   were stored.
   
   We handle these cases:

   The startup sequence can be at the start of the function, or the
   function can start with a branch to startup code at the end.

   %ebp can be set up with either the 'enter' instruction, or "pushl
   %ebp, movl %esp, %ebp" (`enter' is too slow to be useful, but was
   once used in the System V compiler).

   Local space is allocated just below the saved %ebp by either the
   'enter' instruction, or by "subl $<size>, %esp".  'enter' has a 16
   bit unsigned argument for space to allocate, and the 'addl'
   instruction could have either a signed byte, or 32 bit immediate.

   Next, the registers used by this function are pushed.  With the
   System V compiler they will always be in the order: %edi, %esi,
   %ebx (and sometimes a harmless bug causes it to also save but not
   restore %eax); however, the code below is willing to see the pushes
   in any order, and will handle up to 8 of them.
 
   If the setup sequence is at the end of the function, then the next
   instruction will be a branch back to the start.  */

void
i386_frame_init_saved_regs (struct frame_info *fip)
{
  long locals = -1;
  unsigned char op;
  CORE_ADDR dummy_bottom;
  CORE_ADDR addr;
  CORE_ADDR pc;
  int i;

  if (fip->saved_regs)
    return;

  frame_saved_regs_zalloc (fip);

  /* If the frame is the end of a dummy, compute where the beginning
     would be.  */
  dummy_bottom = fip->frame - 4 - REGISTER_BYTES - CALL_DUMMY_LENGTH;

  /* Check if the PC points in the stack, in a dummy frame.  */
  if (dummy_bottom <= fip->pc && fip->pc <= fip->frame)
    {
      /* All registers were saved by push_call_dummy.  */
      addr = fip->frame;
      for (i = 0; i < NUM_REGS; i++)
	{
	  addr -= REGISTER_RAW_SIZE (i);
	  fip->saved_regs[i] = addr;
	}
      return;
    }

  pc = get_pc_function_start (fip->pc);
  if (pc != 0)
    locals = i386_get_frame_setup (pc);

  if (locals >= 0)
    {
      addr = fip->frame - 4 - locals;
      for (i = 0; i < 8; i++)
	{
	  op = codestream_get ();
	  if (op < 0x50 || op > 0x57)
	    break;
#ifdef I386_REGNO_TO_SYMMETRY
	  /* Dynix uses different internal numbering.  Ick.  */
	  fip->saved_regs[I386_REGNO_TO_SYMMETRY (op - 0x50)] = addr;
#else
	  fip->saved_regs[op - 0x50] = addr;
#endif
	  addr -= 4;
	}
    }

  fip->saved_regs[PC_REGNUM] = fip->frame + 4;
  fip->saved_regs[FP_REGNUM] = fip->frame;
}

/* Return PC of first real instruction.  */

int
i386_skip_prologue (int pc)
{
  unsigned char op;
  int i;
  static unsigned char pic_pat[6] =
  { 0xe8, 0, 0, 0, 0,		/* call   0x0 */
    0x5b,			/* popl   %ebx */
  };
  CORE_ADDR pos;

  if (i386_get_frame_setup (pc) < 0)
    return (pc);

  /* Found valid frame setup -- codestream now points to start of push
     instructions for saving registers.  */

  /* Skip over register saves.  */
  for (i = 0; i < 8; i++)
    {
      op = codestream_peek ();
      /* Break if not `pushl' instrunction.  */
      if (op < 0x50 || op > 0x57)
	break;
      codestream_get ();
    }

  /* The native cc on SVR4 in -K PIC mode inserts the following code
     to get the address of the global offset table (GOT) into register
     %ebx
     
        call	0x0
	popl    %ebx
        movl    %ebx,x(%ebp)    (optional)
        addl    y,%ebx

     This code is with the rest of the prologue (at the end of the
     function), so we have to skip it to get to the first real
     instruction at the start of the function.  */

  pos = codestream_tell ();
  for (i = 0; i < 6; i++)
    {
      op = codestream_get ();
      if (pic_pat[i] != op)
	break;
    }
  if (i == 6)
    {
      unsigned char buf[4];
      long delta = 6;

      op = codestream_get ();
      if (op == 0x89)		/* movl %ebx, x(%ebp) */
	{
	  op = codestream_get ();
	  if (op == 0x5d)	/* One byte offset from %ebp.  */
	    {
	      delta += 3;
	      codestream_read (buf, 1);
	    }
	  else if (op == 0x9d)	/* Four byte offset from %ebp.  */
	    {
	      delta += 6;
	      codestream_read (buf, 4);
	    }
	  else			/* Unexpected instruction.  */
	    delta = -1;
	  op = codestream_get ();
	}
      /* addl y,%ebx */
      if (delta > 0 && op == 0x81 && codestream_get () == 0xc3)
	{
	  pos += delta + 6;
	}
    }
  codestream_seek (pos);

  i386_follow_jump ();

  return (codestream_tell ());
}

void
i386_push_dummy_frame (void)
{
  CORE_ADDR sp = read_register (SP_REGNUM);
  CORE_ADDR fp;
  int regnum;
  char regbuf[MAX_REGISTER_RAW_SIZE];

  sp = push_word (sp, read_register (PC_REGNUM));
  sp = push_word (sp, read_register (FP_REGNUM));
  fp = sp;
  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      read_register_gen (regnum, regbuf);
      sp = push_bytes (sp, regbuf, REGISTER_RAW_SIZE (regnum));
    }
  write_register (SP_REGNUM, sp);
  write_register (FP_REGNUM, fp);
}

/* Insert the (relative) function address into the call sequence
   stored at DYMMY.  */

void
i386_fix_call_dummy (char *dummy, CORE_ADDR pc, CORE_ADDR fun, int nargs,
		     struct value **args, struct type *type, int gcc_p)
{
  int from, to, delta, loc;

  loc = (int)(read_register (SP_REGNUM) - CALL_DUMMY_LENGTH);
  from = loc + 5;
  to = (int)(fun);
  delta = to - from;

  *((char *)(dummy) + 1) = (delta & 0xff);
  *((char *)(dummy) + 2) = ((delta >> 8) & 0xff);
  *((char *)(dummy) + 3) = ((delta >> 16) & 0xff);
  *((char *)(dummy) + 4) = ((delta >> 24) & 0xff);
}

void
i386_pop_frame (void)
{
  struct frame_info *frame = get_current_frame ();
  CORE_ADDR fp;
  int regnum;
  char regbuf[MAX_REGISTER_RAW_SIZE];

  fp = FRAME_FP (frame);
  i386_frame_init_saved_regs (frame);

  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      CORE_ADDR addr;
      addr = frame->saved_regs[regnum];
      if (addr)
	{
	  read_memory (addr, regbuf, REGISTER_RAW_SIZE (regnum));
	  write_register_bytes (REGISTER_BYTE (regnum), regbuf,
				REGISTER_RAW_SIZE (regnum));
	}
    }
  write_register (FP_REGNUM, read_memory_integer (fp, 4));
  write_register (PC_REGNUM, read_memory_integer (fp + 4, 4));
  write_register (SP_REGNUM, fp + 8);
  flush_cached_frames ();
}


#ifdef GET_LONGJMP_TARGET

/* FIXME: Multi-arching does not set JB_PC and JB_ELEMENT_SIZE yet.  
   Fill in with dummy value to enable compilation.  */
#ifndef JB_PC
#define JB_PC 0
#endif /* JB_PC */

#ifndef JB_ELEMENT_SIZE
#define JB_ELEMENT_SIZE 4
#endif /* JB_ELEMENT_SIZE */

/* Figure out where the longjmp will land.  Slurp the args out of the
   stack.  We expect the first arg to be a pointer to the jmp_buf
   structure from which we extract the pc (JB_PC) that we will land
   at.  The pc is copied into PC.  This routine returns true on
   success.  */

int
get_longjmp_target (CORE_ADDR *pc)
{
  char buf[TARGET_PTR_BIT / TARGET_CHAR_BIT];
  CORE_ADDR sp, jb_addr;

  sp = read_register (SP_REGNUM);

  if (target_read_memory (sp + SP_ARG0,	/* Offset of first arg on stack.  */
			  buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  jb_addr = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  if (target_read_memory (jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			  TARGET_PTR_BIT / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_address (buf, TARGET_PTR_BIT / TARGET_CHAR_BIT);

  return 1;
}

#endif /* GET_LONGJMP_TARGET */


CORE_ADDR
i386_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
  sp = default_push_arguments (nargs, args, sp, struct_return, struct_addr);
  
  if (struct_return)
    {
      char buf[4];

      sp -= 4;
      store_address (buf, 4, struct_addr);
      write_memory (sp, buf, 4);
    }

  return sp;
}

void
i386_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  /* Do nothing.  Everything was already done by i386_push_arguments.  */
}

/* These registers are used for returning integers (and on some
   targets also for returning `struct' and `union' values when their
   size and alignment match an integer type).  */
#define LOW_RETURN_REGNUM 0	/* %eax */
#define HIGH_RETURN_REGNUM 2	/* %edx */

/* Extract from an array REGBUF containing the (raw) register state, a
   function return value of TYPE, and copy that, in virtual format,
   into VALBUF.  */

void
i386_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      && TYPE_NFIELDS (type) == 1)
    {
      i386_extract_return_value (TYPE_FIELD_TYPE (type, 0), regbuf, valbuf);
      return;
    }

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      if (NUM_FREGS == 0)
	{
	  warning ("Cannot find floating-point return value.");
	  memset (valbuf, 0, len);
	  return;
	}

      /* Floating-point return values can be found in %st(0).  Convert
	 its contents to the desired type.  This is probably not
	 exactly how it would happen on the target itself, but it is
	 the best we can do.  */
      convert_typed_floating (&regbuf[REGISTER_BYTE (FP0_REGNUM)],
			      builtin_type_i387_ext, valbuf, type);
    }
  else
    {
      int low_size = REGISTER_RAW_SIZE (LOW_RETURN_REGNUM);
      int high_size = REGISTER_RAW_SIZE (HIGH_RETURN_REGNUM);

      if (len <= low_size)
	memcpy (valbuf, &regbuf[REGISTER_BYTE (LOW_RETURN_REGNUM)], len);
      else if (len <= (low_size + high_size))
	{
	  memcpy (valbuf,
		  &regbuf[REGISTER_BYTE (LOW_RETURN_REGNUM)], low_size);
	  memcpy (valbuf + low_size,
		  &regbuf[REGISTER_BYTE (HIGH_RETURN_REGNUM)], len - low_size);
	}
      else
	internal_error (__FILE__, __LINE__,
			"Cannot extract return value of %d bytes long.", len);
    }
}

/* Write into the appropriate registers a function return value stored
   in VALBUF of type TYPE, given in virtual format.  */

void
i386_store_return_value (struct type *type, char *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      && TYPE_NFIELDS (type) == 1)
    {
      i386_store_return_value (TYPE_FIELD_TYPE (type, 0), valbuf);
      return;
    }

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      unsigned int fstat;
      char buf[FPU_REG_RAW_SIZE];

      if (NUM_FREGS == 0)
	{
	  warning ("Cannot set floating-point return value.");
	  return;
	}

      /* Returning floating-point values is a bit tricky.  Apart from
         storing the return value in %st(0), we have to simulate the
         state of the FPU at function return point.  */

      /* Convert the value found in VALBUF to the extended
	 floating-point format used by the FPU.  This is probably
	 not exactly how it would happen on the target itself, but
	 it is the best we can do.  */
      convert_typed_floating (valbuf, type, buf, builtin_type_i387_ext);
      write_register_bytes (REGISTER_BYTE (FP0_REGNUM), buf,
			    FPU_REG_RAW_SIZE);

      /* Set the top of the floating-point register stack to 7.  The
         actual value doesn't really matter, but 7 is what a normal
         function return would end up with if the program started out
         with a freshly initialized FPU.  */
      fstat = read_register (FSTAT_REGNUM);
      fstat |= (7 << 11);
      write_register (FSTAT_REGNUM, fstat);

      /* Mark %st(1) through %st(7) as empty.  Since we set the top of
         the floating-point register stack to 7, the appropriate value
         for the tag word is 0x3fff.  */
      write_register (FTAG_REGNUM, 0x3fff);
    }
  else
    {
      int low_size = REGISTER_RAW_SIZE (LOW_RETURN_REGNUM);
      int high_size = REGISTER_RAW_SIZE (HIGH_RETURN_REGNUM);

      if (len <= low_size)
	write_register_bytes (REGISTER_BYTE (LOW_RETURN_REGNUM), valbuf, len);
      else if (len <= (low_size + high_size))
	{
	  write_register_bytes (REGISTER_BYTE (LOW_RETURN_REGNUM),
				valbuf, low_size);
	  write_register_bytes (REGISTER_BYTE (HIGH_RETURN_REGNUM),
				valbuf + low_size, len - low_size);
	}
      else
	internal_error (__FILE__, __LINE__,
			"Cannot store return value of %d bytes long.", len);
    }
}

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR.  */

CORE_ADDR
i386_extract_struct_value_address (char *regbuf)
{
  return extract_address (&regbuf[REGISTER_BYTE (LOW_RETURN_REGNUM)],
			  REGISTER_RAW_SIZE (LOW_RETURN_REGNUM));
}


/* Return the GDB type object for the "standard" data type of data in
   register REGNUM.  Perhaps %esi and %edi should go here, but
   potentially they could be used for things other than address.  */

struct type *
i386_register_virtual_type (int regnum)
{
  if (regnum == PC_REGNUM || regnum == FP_REGNUM || regnum == SP_REGNUM)
    return lookup_pointer_type (builtin_type_void);

  if (IS_FP_REGNUM (regnum))
    return builtin_type_i387_ext;

  if (IS_SSE_REGNUM (regnum))
    return builtin_type_v4sf;

  return builtin_type_int;
}

/* Return true iff register REGNUM's virtual format is different from
   its raw format.  Note that this definition assumes that the host
   supports IEEE 32-bit floats, since it doesn't say that SSE
   registers need conversion.  Even if we can't find a counterexample,
   this is still sloppy.  */

int
i386_register_convertible (int regnum)
{
  return IS_FP_REGNUM (regnum);
}

/* Convert data from raw format for register REGNUM in buffer FROM to
   virtual format with type TYPE in buffer TO.  */

void
i386_register_convert_to_virtual (int regnum, struct type *type,
				  char *from, char *to)
{
  gdb_assert (IS_FP_REGNUM (regnum));

  /* We only support floating-point values.  */
  if (TYPE_CODE (type) != TYPE_CODE_FLT)
    {
      warning ("Cannot convert floating-point register value "
	       "to non-floating-point type.");
      memset (to, 0, TYPE_LENGTH (type));
      return;
    }

  /* Convert to TYPE.  This should be a no-op if TYPE is equivalent to
     the extended floating-point format used by the FPU.  */
  convert_typed_floating (from, builtin_type_i387_ext, to, type);
}

/* Convert data from virtual format with type TYPE in buffer FROM to
   raw format for register REGNUM in buffer TO.  */

void
i386_register_convert_to_raw (struct type *type, int regnum,
			      char *from, char *to)
{
  gdb_assert (IS_FP_REGNUM (regnum));

  /* We only support floating-point values.  */
  if (TYPE_CODE (type) != TYPE_CODE_FLT)
    {
      warning ("Cannot convert non-floating-point type "
	       "to floating-point register value.");
      memset (to, 0, TYPE_LENGTH (type));
      return;
    }

  /* Convert from TYPE.  This should be a no-op if TYPE is equivalent
     to the extended floating-point format used by the FPU.  */
  convert_typed_floating (from, type, to, builtin_type_i387_ext);
}
     

#ifdef I386V4_SIGTRAMP_SAVED_PC
/* Get saved user PC for sigtramp from the pushed ucontext on the
   stack for all three variants of SVR4 sigtramps.  */

CORE_ADDR
i386v4_sigtramp_saved_pc (struct frame_info *frame)
{
  CORE_ADDR saved_pc_offset = 4;
  char *name = NULL;

  find_pc_partial_function (frame->pc, &name, NULL, NULL);
  if (name)
    {
      if (STREQ (name, "_sigreturn"))
	saved_pc_offset = 132 + 14 * 4;
      else if (STREQ (name, "_sigacthandler"))
	saved_pc_offset = 80 + 14 * 4;
      else if (STREQ (name, "sigvechandler"))
	saved_pc_offset = 120 + 14 * 4;
    }

  if (frame->next)
    return read_memory_integer (frame->next->frame + saved_pc_offset, 4);
  return read_memory_integer (read_register (SP_REGNUM) + saved_pc_offset, 4);
}
#endif /* I386V4_SIGTRAMP_SAVED_PC */


#ifdef STATIC_TRANSFORM_NAME
/* SunPRO encodes the static variables.  This is not related to C++
   mangling, it is done for C too.  */

char *
sunpro_static_transform_name (char *name)
{
  char *p;
  if (IS_STATIC_TRANSFORM_NAME (name))
    {
      /* For file-local statics there will be a period, a bunch of
         junk (the contents of which match a string given in the
         N_OPT), a period and the name.  For function-local statics
         there will be a bunch of junk (which seems to change the
         second character from 'A' to 'B'), a period, the name of the
         function, and the name.  So just skip everything before the
         last period.  */
      p = strrchr (name, '.');
      if (p != NULL)
	name = p + 1;
    }
  return name;
}
#endif /* STATIC_TRANSFORM_NAME */


/* Stuff for WIN32 PE style DLL's but is pretty generic really.  */

CORE_ADDR
skip_trampoline_code (CORE_ADDR pc, char *name)
{
  if (pc && read_memory_unsigned_integer (pc, 2) == 0x25ff) /* jmp *(dest) */
    {
      unsigned long indirect = read_memory_unsigned_integer (pc + 2, 4);
      struct minimal_symbol *indsym =
	indirect ? lookup_minimal_symbol_by_pc (indirect) : 0;
      char *symname = indsym ? SYMBOL_NAME (indsym) : 0;

      if (symname)
	{
	  if (strncmp (symname, "__imp_", 6) == 0
	      || strncmp (symname, "_imp_", 5) == 0)
	    return name ? 1 : read_memory_unsigned_integer (indirect, 4);
	}
    }
  return 0;			/* Not a trampoline.  */
}


/* We have two flavours of disassembly.  The machinery on this page
   deals with switching between those.  */

static int
gdb_print_insn_i386 (bfd_vma memaddr, disassemble_info *info)
{
  if (disassembly_flavor == att_flavor)
    return print_insn_i386_att (memaddr, info);
  else if (disassembly_flavor == intel_flavor)
    return print_insn_i386_intel (memaddr, info);
  /* Never reached -- disassembly_flavour is always either att_flavor
     or intel_flavor.  */
  internal_error (__FILE__, __LINE__, "failed internal consistency check");
}


static void
process_note_abi_tag_sections (bfd *abfd, asection *sect, void *obj)
{
  int *os_ident_ptr = obj;
  const char *name;
  unsigned int sect_size;

  name = bfd_get_section_name (abfd, sect);
  sect_size = bfd_section_size (abfd, sect);
  if (strcmp (name, ".note.ABI-tag") == 0 && sect_size > 0)
    {
      unsigned int name_length, data_length, note_type;
      char *note = alloca (sect_size);

      bfd_get_section_contents (abfd, sect, note,
                                (file_ptr) 0, (bfd_size_type) sect_size);

      name_length = bfd_h_get_32 (abfd, note);
      data_length = bfd_h_get_32 (abfd, note + 4);
      note_type = bfd_h_get_32 (abfd, note + 8);

      if (name_length == 4 && data_length == 16 && note_type == 1
          && strcmp (note + 12, "GNU") == 0)
        {
          int os_number = bfd_h_get_32 (abfd, note + 16);

          /* The case numbers are from abi-tags in glibc.  */
          switch (os_number)
            {
            case 0:
              *os_ident_ptr = ELFOSABI_LINUX;
              break;
            case 1:
              *os_ident_ptr = ELFOSABI_HURD;
              break;
            case 2:
              *os_ident_ptr = ELFOSABI_SOLARIS;
              break;
            default:
              internal_error (__FILE__, __LINE__,
                              "process_note_abi_sections: "
                              "unknown OS number %d", os_number);
              break;
            }
        }
    }
}

struct gdbarch *
i386_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  int os_ident;

  if (info.abfd != NULL
      && bfd_get_flavour (info.abfd) == bfd_target_elf_flavour)
    {
      os_ident = elf_elfheader (info.abfd)->e_ident[EI_OSABI];

      /* If os_ident is 0, it is not necessarily the case that we're
         on a SYSV system.  (ELFOSABI_NONE is defined to be 0.)
         GNU/Linux uses a note section to record OS/ABI info, but
         leaves e_ident[EI_OSABI] zero.  So we have to check for note
         sections too.  */
      if (os_ident == ELFOSABI_NONE)
	bfd_map_over_sections (info.abfd,
			       process_note_abi_tag_sections,
			       &os_ident);
	  
      /* If that didn't help us, revert to some non-standard checks.  */
      if (os_ident == ELFOSABI_NONE)
	{
	  /* FreeBSD folks are naughty; they stored the string
	     "FreeBSD" in the padding of the e_ident field of the ELF
	     header.  */
	  if (strcmp (&elf_elfheader (info.abfd)->e_ident[8], "FreeBSD") == 0)
	    os_ident = ELFOSABI_FREEBSD;
        }
    }
  else
    os_ident = -1;

  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      tdep = gdbarch_tdep (arches->gdbarch);
      if (tdep && tdep->os_ident == os_ident)
        return arches->gdbarch;
    }

  /* Allocate space for the new architecture.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  tdep->os_ident = os_ident;

  /* FIXME: kettenis/2001-11-24: Although not all IA-32 processors
     have the SSE registers, it's easier to set the default to 8.  */
  tdep->num_xmm_regs = 8;

  set_gdbarch_use_generic_dummy_frames (gdbarch, 0);

  /* Call dummy code.  */
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_call_dummy_breakpoint_offset (gdbarch, 5);
  set_gdbarch_call_dummy_breakpoint_offset_p (gdbarch, 1);
  set_gdbarch_call_dummy_p (gdbarch, 1);
  set_gdbarch_call_dummy_stack_adjust_p (gdbarch, 0);

  set_gdbarch_get_saved_register (gdbarch, generic_get_saved_register);
  set_gdbarch_push_arguments (gdbarch, i386_push_arguments);

  set_gdbarch_pc_in_call_dummy (gdbarch, pc_in_call_dummy_on_stack);

  /* NOTE: tm-i386nw.h and tm-i386v4.h override this.  */
  set_gdbarch_frame_chain_valid (gdbarch, file_frame_chain_valid);

  /* NOTE: tm-i386aix.h, tm-i386bsd.h, tm-i386os9k.h, tm-linux.h,
     tm-ptx.h, tm-symmetry.h currently override this.  Sigh.  */
  set_gdbarch_num_regs (gdbarch, NUM_GREGS + NUM_FREGS + NUM_SSE_REGS);

  return gdbarch;
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_i386_tdep (void);

void
_initialize_i386_tdep (void)
{
  register_gdbarch_init (bfd_arch_i386, i386_gdbarch_init);

  /* Initialize the table saying where each register starts in the
     register file.  */
  {
    int i, offset;

    offset = 0;
    for (i = 0; i < MAX_NUM_REGS; i++)
      {
	i386_register_offset[i] = offset;
	offset += i386_register_size[i];
      }
  }

  tm_print_insn = gdb_print_insn_i386;
  tm_print_insn_info.mach = bfd_lookup_arch (bfd_arch_i386, 0)->mach;

  /* Add the variable that controls the disassembly flavor.  */
  {
    struct cmd_list_element *new_cmd;

    new_cmd = add_set_enum_cmd ("disassembly-flavor", no_class,
				valid_flavors,
				&disassembly_flavor,
				"\
Set the disassembly flavor, the valid values are \"att\" and \"intel\", \
and the default value is \"att\".",
				&setlist);
    add_show_from_set (new_cmd, &showlist);
  }
}
