/* Target-dependent code for Mitsubishi D10V, for GDB.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

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

/*  Contributed by Martin Hunt, hunt@cygnus.com */

#include "defs.h"
#include "frame.h"
#include "obstack.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "value.h"
#include "inferior.h"
#include "dis-asm.h"  
#include "symfile.h"
#include "objfiles.h"

void d10v_frame_find_saved_regs PARAMS ((struct frame_info *fi,
					 struct frame_saved_regs *fsr));

int
d10v_frame_chain_valid (chain, frame)
     CORE_ADDR chain;
     struct frame_info *frame;      /* not used here */
{
  return ((chain) != 0 && (frame) != 0 && (frame)->pc > IMEM_START);
}


/* Should we use EXTRACT_STRUCT_VALUE_ADDRESS instead of
   EXTRACT_RETURN_VALUE?  GCC_P is true if compiled with gcc
   and TYPE is the type (which is known to be struct, union or array).

   The d10v returns anything less than 8 bytes in size in
   registers. */

int
d10v_use_struct_convention (gcc_p, type)
     int gcc_p;
     struct type *type;
{
  return (TYPE_LENGTH (type) > 8);
}


/* Discard from the stack the innermost frame, restoring all saved
   registers.  */

void
d10v_pop_frame (frame)
     struct frame_info *frame;
{
  CORE_ADDR fp;
  int regnum;
  struct frame_saved_regs fsr;
  char raw_buffer[8];

  fp = FRAME_FP (frame);
  /* fill out fsr with the address of where each */
  /* register was stored in the frame */
  get_frame_saved_regs (frame, &fsr);
  
  /* now update the current registers with the old values */
  for (regnum = A0_REGNUM; regnum < A0_REGNUM+2 ; regnum++)
    {
      if (fsr.regs[regnum])
	{
	  read_memory (fsr.regs[regnum], raw_buffer,  REGISTER_RAW_SIZE(regnum));
	  write_register_bytes (REGISTER_BYTE (regnum), raw_buffer,  REGISTER_RAW_SIZE(regnum));
	}
    }
  for (regnum = 0; regnum < SP_REGNUM; regnum++)
    {
      if (fsr.regs[regnum])
	{
	  write_register (regnum, read_memory_unsigned_integer (fsr.regs[regnum], REGISTER_RAW_SIZE(regnum)));
	}
    }
  if (fsr.regs[PSW_REGNUM])
    {
      write_register (PSW_REGNUM, read_memory_unsigned_integer (fsr.regs[PSW_REGNUM], REGISTER_RAW_SIZE(PSW_REGNUM)));
    }

  write_register (PC_REGNUM, read_register (LR_REGNUM));
  write_register (SP_REGNUM, fp + frame->size);
  target_store_registers (-1);
  flush_cached_frames ();
}

static int 
check_prologue (op)
     unsigned short op;
{
  /* st  rn, @-sp */
  if ((op & 0x7E1F) == 0x6C1F)
    return 1;

  /* st2w  rn, @-sp */
  if ((op & 0x7E3F) == 0x6E1F)
    return 1;

  /* subi  sp, n */
  if ((op & 0x7FE1) == 0x01E1)
    return 1;

  /* mv  r11, sp */
  if (op == 0x417E)
    return 1;

  /* nop */
  if (op == 0x5E00)
    return 1;

  /* st  rn, @sp */
  if ((op & 0x7E1F) == 0x681E)
    return 1;

  /* st2w  rn, @sp */
 if ((op & 0x7E3F) == 0x3A1E)
   return 1;

  return 0;
}

CORE_ADDR
d10v_skip_prologue (pc)
     CORE_ADDR pc;
{
  unsigned long op;
  unsigned short op1, op2;
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;

  /* If we have line debugging information, then the end of the */
  /* prologue should the first assembly instruction of  the first source line */
  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);
      if ( sal.end && sal.end < func_end)
	return sal.end;
    }
  
  if (target_read_memory (pc, (char *)&op, 4))
    return pc;			/* Can't access it -- assume no prologue. */

  while (1)
    {
      op = (unsigned long)read_memory_integer (pc, 4);
      if ((op & 0xC0000000) == 0xC0000000)
	{
	  /* long instruction */
	  if ( ((op & 0x3FFF0000) != 0x01FF0000) &&   /* add3 sp,sp,n */
	       ((op & 0x3F0F0000) != 0x340F0000) &&   /* st  rn, @(offset,sp) */
 	       ((op & 0x3F1F0000) != 0x350F0000))     /* st2w  rn, @(offset,sp) */
	    break;
	}
      else
	{
	  /* short instructions */
	  if ((op & 0xC0000000) == 0x80000000)
	    {
	      op2 = (op & 0x3FFF8000) >> 15;
	      op1 = op & 0x7FFF;
	    } 
	  else 
	    {
	      op1 = (op & 0x3FFF8000) >> 15;
	      op2 = op & 0x7FFF;
	    }
	  if (check_prologue(op1))
	    {
	      if (!check_prologue(op2))
		{
		  /* if the previous opcode was really part of the prologue */
		  /* and not just a NOP, then we want to break after both instructions */
		  if (op1 != 0x5E00)
		    pc += 4;
		  break;
		}
	    }
	  else
	    break;
	}
      pc += 4;
    }
  return pc;
}

/* Given a GDB frame, determine the address of the calling function's frame.
   This will be used to create a new GDB frame struct, and then
   INIT_EXTRA_FRAME_INFO and INIT_FRAME_PC will be called for the new frame.
*/

CORE_ADDR
d10v_frame_chain (frame)
     struct frame_info *frame;
{
  struct frame_saved_regs fsr;

  d10v_frame_find_saved_regs (frame, &fsr);

  if (frame->return_pc == IMEM_START || inside_entry_file(frame->return_pc))
    return (CORE_ADDR)0;

  if (!fsr.regs[FP_REGNUM])
    {
      if (!fsr.regs[SP_REGNUM] || fsr.regs[SP_REGNUM] == STACK_START)
	return (CORE_ADDR)0;
      
      return fsr.regs[SP_REGNUM];
    }

  if (!read_memory_unsigned_integer(fsr.regs[FP_REGNUM], REGISTER_RAW_SIZE(FP_REGNUM)))
    return (CORE_ADDR)0;

  return D10V_MAKE_DADDR (read_memory_unsigned_integer (fsr.regs[FP_REGNUM], REGISTER_RAW_SIZE (FP_REGNUM)));
}  

static int next_addr, uses_frame;

static int 
prologue_find_regs (op, fsr, addr)
     unsigned short op;
     struct frame_saved_regs *fsr;
     CORE_ADDR addr;
{
  int n;

  /* st  rn, @-sp */
  if ((op & 0x7E1F) == 0x6C1F)
    {
      n = (op & 0x1E0) >> 5;
      next_addr -= 2;
      fsr->regs[n] = next_addr;
      return 1;
    }

  /* st2w  rn, @-sp */
  else if ((op & 0x7E3F) == 0x6E1F)
    {
      n = (op & 0x1E0) >> 5;
      next_addr -= 4;
      fsr->regs[n] = next_addr;
      fsr->regs[n+1] = next_addr+2;
      return 1;
    }

  /* subi  sp, n */
  if ((op & 0x7FE1) == 0x01E1)
    {
      n = (op & 0x1E) >> 1;
      if (n == 0)
	n = 16;
      next_addr -= n;
      return 1;
    }

  /* mv  r11, sp */
  if (op == 0x417E)
    {
      uses_frame = 1;
      return 1;
    }

  /* nop */
  if (op == 0x5E00)
    return 1;

  /* st  rn, @sp */
  if ((op & 0x7E1F) == 0x681E)
    {
      n = (op & 0x1E0) >> 5;
      fsr->regs[n] = next_addr;
      return 1;
    }

  /* st2w  rn, @sp */
  if ((op & 0x7E3F) == 0x3A1E)
    {
      n = (op & 0x1E0) >> 5;
      fsr->regs[n] = next_addr;
      fsr->regs[n+1] = next_addr+2;
      return 1;
    }

  return 0;
}

/* Put here the code to store, into a struct frame_saved_regs, the
   addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special: the address we
   return for it IS the sp for the next frame. */
void
d10v_frame_find_saved_regs (fi, fsr)
     struct frame_info *fi;
     struct frame_saved_regs *fsr;
{
  CORE_ADDR fp, pc;
  unsigned long op;
  unsigned short op1, op2;
  int i;

  fp = fi->frame;
  memset (fsr, 0, sizeof (*fsr));
  next_addr = 0;

  pc = get_pc_function_start (fi->pc);

  uses_frame = 0;
  while (1)
    {
      op = (unsigned long)read_memory_integer (pc, 4);
      if ((op & 0xC0000000) == 0xC0000000)
	{
	  /* long instruction */
	  if ((op & 0x3FFF0000) == 0x01FF0000)
	    {
	      /* add3 sp,sp,n */
	      short n = op & 0xFFFF;
	      next_addr += n;
	    }
	  else if ((op & 0x3F0F0000) == 0x340F0000)
	    {
	      /* st  rn, @(offset,sp) */
	      short offset = op & 0xFFFF;
	      short n = (op >> 20) & 0xF;
	      fsr->regs[n] = next_addr + offset;
	    }
	  else if ((op & 0x3F1F0000) == 0x350F0000)
	    {
	      /* st2w  rn, @(offset,sp) */
	      short offset = op & 0xFFFF;
	      short n = (op >> 20) & 0xF;
	      fsr->regs[n] = next_addr + offset;
	      fsr->regs[n+1] = next_addr + offset + 2;
	    }
	  else
	    break;
	}
      else
	{
	  /* short instructions */
	  if ((op & 0xC0000000) == 0x80000000)
	    {
	      op2 = (op & 0x3FFF8000) >> 15;
	      op1 = op & 0x7FFF;
	    } 
	  else 
	    {
	      op1 = (op & 0x3FFF8000) >> 15;
	      op2 = op & 0x7FFF;
	    }
	  if (!prologue_find_regs(op1,fsr,pc) || !prologue_find_regs(op2,fsr,pc))
	    break;
	}
      pc += 4;
    }
  
  fi->size = -next_addr;

  if (!(fp & 0xffff))
    fp = D10V_MAKE_DADDR (read_register(SP_REGNUM));

  for (i=0; i<NUM_REGS-1; i++)
    if (fsr->regs[i])
      {
	fsr->regs[i] = fp - (next_addr - fsr->regs[i]); 
      }

  if (fsr->regs[LR_REGNUM])
    {
      CORE_ADDR return_pc = read_memory_unsigned_integer (fsr->regs[LR_REGNUM], REGISTER_RAW_SIZE (LR_REGNUM));
      fi->return_pc = D10V_MAKE_IADDR (return_pc);
    }
  else
    {
      fi->return_pc = D10V_MAKE_IADDR (read_register(LR_REGNUM));
    }
  
  /* th SP is not normally (ever?) saved, but check anyway */
  if (!fsr->regs[SP_REGNUM])
    {
      /* if the FP was saved, that means the current FP is valid, */
      /* otherwise, it isn't being used, so we use the SP instead */
      if (uses_frame)
	fsr->regs[SP_REGNUM] = read_register(FP_REGNUM) + fi->size;
      else
	{
	  fsr->regs[SP_REGNUM] = fp + fi->size;
	  fi->frameless = 1;
	  fsr->regs[FP_REGNUM] = 0;
	}
    }
}

void
d10v_init_extra_frame_info (fromleaf, fi)
     int fromleaf;
     struct frame_info *fi;
{
  fi->frameless = 0;
  fi->size = 0;
  fi->return_pc = 0;

  /* The call dummy doesn't save any registers on the stack, so we can
     return now.  */
  if (PC_IN_CALL_DUMMY (fi->pc, fi->frame, fi->frame))
    {
      return;
    }
  else
    {
      struct frame_saved_regs dummy;
      d10v_frame_find_saved_regs (fi, &dummy);
    }
}

static void
show_regs (args, from_tty)
     char *args;
     int from_tty;
{
  int a;
  printf_filtered ("PC=%04x (0x%x) PSW=%04x RPT_S=%04x RPT_E=%04x RPT_C=%04x\n",
                   read_register (PC_REGNUM), D10V_MAKE_IADDR (read_register (PC_REGNUM)),
                   read_register (PSW_REGNUM),
                   read_register (24),
                   read_register (25),
                   read_register (23));
  printf_filtered ("R0-R7  %04x %04x %04x %04x %04x %04x %04x %04x\n",
                   read_register (0),
                   read_register (1),
                   read_register (2),
                   read_register (3),
                   read_register (4),
                   read_register (5),
                   read_register (6),
                   read_register (7));
  printf_filtered ("R8-R15 %04x %04x %04x %04x %04x %04x %04x %04x\n",
                   read_register (8), 
                   read_register (9),
                   read_register (10),
                   read_register (11),
                   read_register (12),
                   read_register (13),
                   read_register (14),
                   read_register (15));
  printf_filtered ("IMAP0 %04x    IMAP1 %04x    DMAP %04x\n",
                   read_register (IMAP0_REGNUM),
                   read_register (IMAP1_REGNUM),
                   read_register (DMAP_REGNUM));
  printf_filtered ("A0-A1");
  for (a = A0_REGNUM; a <= A0_REGNUM + 1; a++)
    {
      char num[MAX_REGISTER_RAW_SIZE];
      int i;
      printf_filtered ("  ");
      read_register_gen (a, (char *)&num);
      for (i = 0; i < MAX_REGISTER_RAW_SIZE; i++)
	{
	  printf_filtered ("%02x", (num[i] & 0xff));
	}
    }
  printf_filtered ("\n");
}

CORE_ADDR
d10v_read_pc (pid)
     int pid;
{
  int save_pid;
  CORE_ADDR pc;
  CORE_ADDR retval;

  save_pid = inferior_pid;
  inferior_pid = pid;
  pc = (int) read_register (PC_REGNUM);
  inferior_pid = save_pid;
  retval = D10V_MAKE_IADDR (pc);
  return retval;
}

void
d10v_write_pc (val, pid)
     CORE_ADDR val;
     int pid;
{
  int save_pid;

  save_pid = inferior_pid;
  inferior_pid = pid;
  write_register (PC_REGNUM, D10V_CONVERT_IADDR_TO_RAW (val));
  inferior_pid = save_pid;
}

CORE_ADDR
d10v_read_sp ()
{
  return (D10V_MAKE_DADDR (read_register (SP_REGNUM)));
}

void
d10v_write_sp (val)
     CORE_ADDR val;
{
  write_register (SP_REGNUM, D10V_CONVERT_DADDR_TO_RAW (val));
}

void
d10v_write_fp (val)
     CORE_ADDR val;
{
  write_register (FP_REGNUM, D10V_CONVERT_DADDR_TO_RAW (val));
}

CORE_ADDR
d10v_read_fp ()
{
  return (D10V_MAKE_DADDR (read_register(FP_REGNUM)));
}

/* Function: push_return_address (pc)
   Set up the return address for the inferior function call.
   Needed for targets where we don't actually execute a JSR/BSR instruction */
 
CORE_ADDR
d10v_push_return_address (pc, sp)
     CORE_ADDR pc;
     CORE_ADDR sp;
{
  write_register (LR_REGNUM, D10V_CONVERT_IADDR_TO_RAW (CALL_DUMMY_ADDRESS ()));
  return sp;
}
 

CORE_ADDR
d10v_push_arguments (nargs, args, sp, struct_return, struct_addr)
     int nargs;
     value_ptr *args;
     CORE_ADDR sp;
     int struct_return;
     CORE_ADDR struct_addr;
{
  int i;
  int regnum = ARG1_REGNUM;
  
  /* Fill in registers and arg lists */
  for (i = 0; i < nargs; i++)
    {
      value_ptr arg = args[i];
      struct type *type = check_typedef (VALUE_TYPE (arg));
      char *contents = VALUE_CONTENTS (arg);
      int len = TYPE_LENGTH (type);
      /* printf ("push: type=%d len=%d\n", type->code, len); */
      if (TYPE_CODE (type) == TYPE_CODE_PTR)
	{
	  /* pointers require special handling - first convert and
	     then store */
	  long val = extract_signed_integer (contents, len);
	  len = 2;
	  if (TYPE_TARGET_TYPE (type)
	      && (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC))
	    {
	      /* function pointer */
	      val = D10V_CONVERT_IADDR_TO_RAW (val);
	    }
	  else if (D10V_IADDR_P (val))
	    {
	      /* also function pointer! */
	      val = D10V_CONVERT_DADDR_TO_RAW (val);
	    }
	  else
	    {
	      /* data pointer */
	      val &= 0xFFFF;
	    }
	  if (regnum <= ARGN_REGNUM)
	    write_register (regnum++, val & 0xffff);
	  else
	    {
	      char ptr[2];
	      sp -= 2;
	      store_address (ptr, val & 0xffff, 2);
	      write_memory (sp, ptr, 2);
	    }
	}
      else
	{
	  int aligned_regnum = (regnum + 1) & ~1;
	  if (len <= 2 && regnum <= ARGN_REGNUM)
	    /* fits in a single register, do not align */
	    {
	      long val = extract_unsigned_integer (contents, len);
	      write_register (regnum++, val);
	    }
	  else if (len <= (ARGN_REGNUM - aligned_regnum + 1) * 2)
	    /* value fits in remaining registers, store keeping left
               aligned */
	    {
	      int b;
	      regnum = aligned_regnum;
	      for (b = 0; b < (len & ~1); b += 2)
		{
		  long val = extract_unsigned_integer (&contents[b], 2);
		  write_register (regnum++, val);
		}
	      if (b < len)
		{
		  long val = extract_unsigned_integer (&contents[b], 1);
		  write_register (regnum++, (val << 8));
		}
	    }
	  else
	    {
	      /* arg goes straight on stack */
	      regnum = ARGN_REGNUM + 1;
	      sp = (sp - len) & ~1;
	      write_memory (sp, contents, len);
	    }
	}
    }
  return sp;
}


/* Given a return value in `regbuf' with a type `valtype', 
   extract and copy its value into `valbuf'.  */

void
d10v_extract_return_value (type, regbuf, valbuf)
     struct type *type;
     char regbuf[REGISTER_BYTES];
     char *valbuf;
{
  int len;
  /*    printf("RET: TYPE=%d len=%d r%d=0x%x\n",type->code, TYPE_LENGTH (type), RET1_REGNUM - R0_REGNUM, (int) extract_unsigned_integer (regbuf + REGISTER_BYTE(RET1_REGNUM), REGISTER_RAW_SIZE (RET1_REGNUM)));  */
  if (TYPE_CODE (type) == TYPE_CODE_PTR
      && TYPE_TARGET_TYPE (type)
      && (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC))
    {
      /* pointer to function */
      int num;
      short snum;
      snum = extract_address (regbuf + REGISTER_BYTE (RET1_REGNUM), REGISTER_RAW_SIZE (RET1_REGNUM));
      store_address ( valbuf, 4, D10V_MAKE_IADDR(snum));
    }
  else if (TYPE_CODE(type) == TYPE_CODE_PTR)
    {
      /* pointer to data */
      int num;
      short snum;
      snum = extract_address (regbuf + REGISTER_BYTE (RET1_REGNUM), REGISTER_RAW_SIZE (RET1_REGNUM));
      store_address ( valbuf, 4, D10V_MAKE_DADDR(snum));
    }
  else
    {
      len = TYPE_LENGTH (type);
      if (len == 1)
	{
	  unsigned short c = extract_unsigned_integer (regbuf + REGISTER_BYTE (RET1_REGNUM), REGISTER_RAW_SIZE (RET1_REGNUM));
	  store_unsigned_integer (valbuf, 1, c);
	}
      else if ((len & 1) == 0)
	memcpy (valbuf, regbuf + REGISTER_BYTE (RET1_REGNUM), len);
      else
	{
	  /* For return values of odd size, the first byte is in the
             least significant part of the first register.  The
             remaining bytes in remaining registers. Interestingly,
             when such values are passed in, the last byte is in the
             most significant byte of that same register - wierd. */
	  memcpy (valbuf, regbuf + REGISTER_BYTE (RET1_REGNUM) + 1, len);
	}
    }
}

/* The following code implements access to, and display of, the D10V's
   instruction trace buffer.  The buffer consists of 64K or more
   4-byte words of data, of which each words includes an 8-bit count,
   an 8-bit segment number, and a 16-bit instruction address.

   In theory, the trace buffer is continuously capturing instruction
   data that the CPU presents on its "debug bus", but in practice, the
   ROMified GDB stub only enables tracing when it continues or steps
   the program, and stops tracing when the program stops; so it
   actually works for GDB to read the buffer counter out of memory and
   then read each trace word.  The counter records where the tracing
   stops, but there is no record of where it started, so we remember
   the PC when we resumed and then search backwards in the trace
   buffer for a word that includes that address.  This is not perfect,
   because you will miss trace data if the resumption PC is the target
   of a branch.  (The value of the buffer counter is semi-random, any
   trace data from a previous program stop is gone.)  */

/* The address of the last word recorded in the trace buffer.  */

#define DBBC_ADDR (0xd80000)

/* The base of the trace buffer, at least for the "Board_0".  */

#define TRACE_BUFFER_BASE (0xf40000)

static void trace_command PARAMS ((char *, int));

static void untrace_command PARAMS ((char *, int));

static void trace_info PARAMS ((char *, int));

static void tdisassemble_command PARAMS ((char *, int));

static void display_trace PARAMS ((int, int));

/* True when instruction traces are being collected.  */

static int tracing;

/* Remembered PC.  */

static CORE_ADDR last_pc;

/* True when trace output should be displayed whenever program stops.  */

static int trace_display;

/* True when trace listing should include source lines.  */

static int default_trace_show_source = 1;

struct trace_buffer {
  int size;
  short *counts;
  CORE_ADDR *addrs;
} trace_data;

static void
trace_command (args, from_tty)
     char *args;
     int from_tty;
{
  /* Clear the host-side trace buffer, allocating space if needed.  */
  trace_data.size = 0;
  if (trace_data.counts == NULL)
    trace_data.counts = (short *) xmalloc (65536 * sizeof(short));
  if (trace_data.addrs == NULL)
    trace_data.addrs = (CORE_ADDR *) xmalloc (65536 * sizeof(CORE_ADDR));

  tracing = 1;

  printf_filtered ("Tracing is now on.\n");
}

static void
untrace_command (args, from_tty)
     char *args;
     int from_tty;
{
  tracing = 0;

  printf_filtered ("Tracing is now off.\n");
}

static void
trace_info (args, from_tty)
     char *args;
     int from_tty;
{
  int i;

  if (trace_data.size)
    {
      printf_filtered ("%d entries in trace buffer:\n", trace_data.size);

      for (i = 0; i < trace_data.size; ++i)
	{
	  printf_filtered ("%d: %d instruction%s at 0x%x\n",
			   i, trace_data.counts[i],
			   (trace_data.counts[i] == 1 ? "" : "s"),
			   trace_data.addrs[i]);
	}
    }
  else
    printf_filtered ("No entries in trace buffer.\n");

  printf_filtered ("Tracing is currently %s.\n", (tracing ? "on" : "off"));
}

/* Print the instruction at address MEMADDR in debugged memory,
   on STREAM.  Returns length of the instruction, in bytes.  */

static int
print_insn (memaddr, stream)
     CORE_ADDR memaddr;
     GDB_FILE *stream;
{
  /* If there's no disassembler, something is very wrong.  */
  if (tm_print_insn == NULL)
    abort ();

  if (TARGET_BYTE_ORDER == BIG_ENDIAN)
    tm_print_insn_info.endian = BFD_ENDIAN_BIG;
  else
    tm_print_insn_info.endian = BFD_ENDIAN_LITTLE;
  return (*tm_print_insn) (memaddr, &tm_print_insn_info);
}

void
d10v_eva_prepare_to_trace ()
{
  if (!tracing)
    return;

  last_pc = read_register (PC_REGNUM);
}

/* Collect trace data from the target board and format it into a form
   more useful for display.  */

void
d10v_eva_get_trace_data ()
{
  int count, i, j, oldsize;
  int trace_addr, trace_seg, trace_cnt, next_cnt;
  unsigned int last_trace, trace_word, next_word;
  unsigned int *tmpspace;

  if (!tracing)
    return;

  tmpspace = xmalloc (65536 * sizeof(unsigned int));

  last_trace = read_memory_unsigned_integer (DBBC_ADDR, 2) << 2;

  /* Collect buffer contents from the target, stopping when we reach
     the word recorded when execution resumed.  */

  count = 0;
  while (last_trace > 0)
    {
      QUIT;
      trace_word =
	read_memory_unsigned_integer (TRACE_BUFFER_BASE + last_trace, 4);
      trace_addr = trace_word & 0xffff;
      last_trace -= 4;
      /* Ignore an apparently nonsensical entry.  */
      if (trace_addr == 0xffd5)
	continue;
      tmpspace[count++] = trace_word;
      if (trace_addr == last_pc)
	break;
      if (count > 65535)
	break;
    }

  /* Move the data to the host-side trace buffer, adjusting counts to
     include the last instruction executed and transforming the address
     into something that GDB likes.  */

  for (i = 0; i < count; ++i)
    {
      trace_word = tmpspace[i];
      next_word = ((i == 0) ? 0 : tmpspace[i - 1]);
      trace_addr = trace_word & 0xffff;
      next_cnt = (next_word >> 24) & 0xff;
      j = trace_data.size + count - i - 1;
      trace_data.addrs[j] = (trace_addr << 2) + 0x1000000;
      trace_data.counts[j] = next_cnt + 1;
    }

  oldsize = trace_data.size;
  trace_data.size += count;

  free (tmpspace);

  if (trace_display)
    display_trace (oldsize, trace_data.size);
}

static void
tdisassemble_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  int i, count;
  CORE_ADDR low, high;
  char *space_index;

  if (!arg)
    {
      low = 0;
      high = trace_data.size;
    }
  else if (!(space_index = (char *) strchr (arg, ' ')))
    {
      low = parse_and_eval_address (arg);
      high = low + 5;
    }
  else
    {
      /* Two arguments.  */
      *space_index = '\0';
      low = parse_and_eval_address (arg);
      high = parse_and_eval_address (space_index + 1);
      if (high < low)
	high = low;
    }

  printf_filtered ("Dump of trace from %d to %d:\n", low, high);

  display_trace (low, high);

  printf_filtered ("End of trace dump.\n");
  gdb_flush (gdb_stdout);
}

static void
display_trace (low, high)
     int low, high;
{
  int i, count, trace_show_source, first, suppress;
  CORE_ADDR next_address;

  trace_show_source = default_trace_show_source;
  if (!have_full_symbols () && !have_partial_symbols())
    {
      trace_show_source = 0;
      printf_filtered ("No symbol table is loaded.  Use the \"file\" command.\n");
      printf_filtered ("Trace will not display any source.\n");
    }

  first = 1;
  suppress = 0;
  for (i = low; i < high; ++i)
    {
      next_address = trace_data.addrs[i];
      count = trace_data.counts[i]; 
      while (count-- > 0)
	{
	  QUIT;
	  if (trace_show_source)
	    {
	      struct symtab_and_line sal, sal_prev;

	      sal_prev = find_pc_line (next_address - 4, 0);
	      sal = find_pc_line (next_address, 0);

	      if (sal.symtab)
		{
		  if (first || sal.line != sal_prev.line)
		    print_source_lines (sal.symtab, sal.line, sal.line + 1, 0);
		  suppress = 0;
		}
	      else
		{
		  if (!suppress)
		    /* FIXME-32x64--assumes sal.pc fits in long.  */
		    printf_filtered ("No source file for address %s.\n",
				     local_hex_string((unsigned long) sal.pc));
		  suppress = 1;
		}
	    }
	  first = 0;
	  print_address (next_address, gdb_stdout);
	  printf_filtered (":");
	  printf_filtered ("\t");
	  wrap_here ("    ");
	  next_address = next_address + print_insn (next_address, gdb_stdout);
	  printf_filtered ("\n");
	  gdb_flush (gdb_stdout);
	}
    }
}

extern void (*target_resume_hook) PARAMS ((void));
extern void (*target_wait_loop_hook) PARAMS ((void));

void
_initialize_d10v_tdep ()
{
  tm_print_insn = print_insn_d10v;

  target_resume_hook = d10v_eva_prepare_to_trace;
  target_wait_loop_hook = d10v_eva_get_trace_data;

  add_com ("regs", class_vars, show_regs, "Print all registers");

  add_com ("trace", class_support, trace_command,
	   "Enable tracing of instruction execution.");

  add_com ("untrace", class_support, untrace_command,
	   "Disable tracing of instruction execution.");

  add_com ("tdisassemble", class_vars, tdisassemble_command,
	   "Disassemble the trace buffer.\n\
Two optional arguments specify a range of trace buffer entries\n\
as reported by info trace (NOT addresses!).");

  add_info ("trace", trace_info,
	    "Display info about the trace data buffer.");

  add_show_from_set (add_set_cmd ("tracedisplay", no_class,
				  var_integer, (char *)&trace_display,
				  "Set automatic display of trace.\n", &setlist),
		     &showlist);
  add_show_from_set (add_set_cmd ("tracesource", no_class,
				  var_integer, (char *)&default_trace_show_source,
				  "Set display of source code with trace.\n", &setlist),
		     &showlist);

} 
