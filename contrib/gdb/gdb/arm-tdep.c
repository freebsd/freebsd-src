/* Target-dependent code for the Acorn Risc Machine, for GDB, the GNU Debugger.
   Copyright 1988, 1989, 1991, 1992, 1993, 1995 Free Software Foundation, Inc.

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

#include "defs.h"
#include "frame.h"
#include "inferior.h"

#if 0
#include "gdbcore.h"
#include <sys/param.h>
#include <sys/dir.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#define N_TXTADDR(hdr) 0x8000
#define N_DATADDR(hdr) (hdr.a_text + 0x8000)

#include <sys/user.h>		/* After a.out.h  */
#include <sys/file.h>
#include "gdb_stat.h"

#include <errno.h>
#endif


#if 0
/* Work with core dump and executable files, for GDB. 
   This code would be in corefile.c if it weren't machine-dependent. */

/* Structure to describe the chain of shared libraries used
   by the execfile.
   e.g. prog shares Xt which shares X11 which shares c. */

struct shared_library {
    struct exec_header header;
    char name[SHLIBLEN];
    CORE_ADDR text_start;	/* CORE_ADDR of 1st byte of text, this file */
    long data_offset;		/* offset of data section in file */
    int chan;			/* file descriptor for the file */
    struct shared_library *shares; /* library this one shares */
};
static struct shared_library *shlib = 0;

/* Hook for `exec_file_command' command to call.  */

extern void (*exec_file_display_hook) ();
   
static CORE_ADDR unshared_text_start;

/* extended header from exec file (for shared library info) */

static struct exec_header exec_header;

void
exec_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  int val;

  /* Eliminate all traces of old exec file.
     Mark text segment as empty.  */

  if (execfile)
    free (execfile);
  execfile = 0;
  data_start = 0;
  data_end -= exec_data_start;
  text_start = 0;
  unshared_text_start = 0;
  text_end = 0;
  exec_data_start = 0;
  exec_data_end = 0;
  if (execchan >= 0)
    close (execchan);
  execchan = -1;
  if (shlib) {
      close_shared_library(shlib);
      shlib = 0;
  }

  /* Now open and digest the file the user requested, if any.  */

  if (filename)
    {
      filename = tilde_expand (filename);
      make_cleanup (free, filename);

      execchan = openp (getenv ("PATH"), 1, filename, O_RDONLY, 0,
			&execfile);
      if (execchan < 0)
	perror_with_name (filename);

      {
	struct stat st_exec;

#ifdef HEADER_SEEK_FD
	HEADER_SEEK_FD (execchan);
#endif
	
	val = myread (execchan, &exec_header, sizeof exec_header);
	exec_aouthdr = exec_header.a_exec;

	if (val < 0)
	  perror_with_name (filename);

	text_start = 0x8000;

	/* Look for shared library if needed */
	if (exec_header.a_exec.a_magic & MF_USES_SL)
	    shlib = open_shared_library(exec_header.a_shlibname, text_start);

	text_offset = N_TXTOFF (exec_aouthdr);
	exec_data_offset = N_TXTOFF (exec_aouthdr) + exec_aouthdr.a_text;

	if (shlib) {
	    unshared_text_start = shared_text_end(shlib) & ~0x7fff;
	    stack_start = shlib->header.a_exec.a_sldatabase;
	    stack_end = STACK_END_ADDR;
	} else
	    unshared_text_start = 0x8000;
	text_end = unshared_text_start + exec_aouthdr.a_text;

	exec_data_start = unshared_text_start + exec_aouthdr.a_text;
        exec_data_end = exec_data_start + exec_aouthdr.a_data;

	data_start = exec_data_start;
	data_end += exec_data_start;

	fstat (execchan, &st_exec);
	exec_mtime = st_exec.st_mtime;
      }

      validate_files ();
    }
  else if (from_tty)
    printf ("No exec file now.\n");

  /* Tell display code (if any) about the changed file name.  */
  if (exec_file_display_hook)
    (*exec_file_display_hook) (filename);
}
#endif

#if 0
/* Read from the program's memory (except for inferior processes).
   This function is misnamed, since it only reads, never writes; and
   since it will use the core file and/or executable file as necessary.

   It should be extended to write as well as read, FIXME, for patching files.

   Return 0 if address could be read, EIO if addresss out of bounds.  */

int
xfer_core_file (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  register int i;
  register int val;
  int xferchan;
  char **xferfile;
  int fileptr;
  int returnval = 0;

  while (len > 0)
    {
      xferfile = 0;
      xferchan = 0;

      /* Determine which file the next bunch of addresses reside in,
	 and where in the file.  Set the file's read/write pointer
	 to point at the proper place for the desired address
	 and set xferfile and xferchan for the correct file.

	 If desired address is nonexistent, leave them zero.

	 i is set to the number of bytes that can be handled
	 along with the next address.

	 We put the most likely tests first for efficiency.  */

      /* Note that if there is no core file
	 data_start and data_end are equal.  */
      if (memaddr >= data_start && memaddr < data_end)
	{
	  i = min (len, data_end - memaddr);
	  fileptr = memaddr - data_start + data_offset;
	  xferfile = &corefile;
	  xferchan = corechan;
	}
      /* Note that if there is no core file
	 stack_start and stack_end define the shared library data.  */
      else if (memaddr >= stack_start && memaddr < stack_end)
	{
	    if (corechan < 0) {
		struct shared_library *lib;
		for (lib = shlib; lib; lib = lib->shares)
		    if (memaddr >= lib->header.a_exec.a_sldatabase &&
			memaddr < lib->header.a_exec.a_sldatabase +
			  lib->header.a_exec.a_data)
			break;
		if (lib) {
		    i = min (len, lib->header.a_exec.a_sldatabase +
			     lib->header.a_exec.a_data - memaddr);
		    fileptr = lib->data_offset + memaddr -
			lib->header.a_exec.a_sldatabase;
		    xferfile = execfile;
		    xferchan = lib->chan;
		}
	    } else {
		i = min (len, stack_end - memaddr);
		fileptr = memaddr - stack_start + stack_offset;
		xferfile = &corefile;
		xferchan = corechan;
	    }
	}
      else if (corechan < 0
	       && memaddr >= exec_data_start && memaddr < exec_data_end)
	{
	  i = min (len, exec_data_end - memaddr);
	  fileptr = memaddr - exec_data_start + exec_data_offset;
	  xferfile = &execfile;
	  xferchan = execchan;
	}
      else if (memaddr >= text_start && memaddr < text_end)
	{
	    struct shared_library *lib;
	    for (lib = shlib; lib; lib = lib->shares)
		if (memaddr >= lib->text_start &&
		    memaddr < lib->text_start + lib->header.a_exec.a_text)
		    break;
	    if (lib) {
		i = min (len, lib->header.a_exec.a_text +
			 lib->text_start - memaddr);
		fileptr = memaddr - lib->text_start + text_offset;
		xferfile = &execfile;
		xferchan = lib->chan;
	    } else {
		i = min (len, text_end - memaddr);
		fileptr = memaddr - unshared_text_start + text_offset;
		xferfile = &execfile;
		xferchan = execchan;
	    }
	}
      else if (memaddr < text_start)
	{
	  i = min (len, text_start - memaddr);
	}
      else if (memaddr >= text_end
	       && memaddr < (corechan >= 0? data_start : exec_data_start))
	{
	  i = min (len, data_start - memaddr);
	}
      else if (corechan >= 0
	       && memaddr >= data_end && memaddr < stack_start)
	{
	  i = min (len, stack_start - memaddr);
	}
      else if (corechan < 0 && memaddr >= exec_data_end)
	{
	  i = min (len, - memaddr);
	}
      else if (memaddr >= stack_end && stack_end != 0)
	{
	  i = min (len, - memaddr);
	}
      else
	{
	  /* Address did not classify into one of the known ranges.
	     This shouldn't happen; we catch the endpoints.  */
	  fatal ("Internal: Bad case logic in xfer_core_file.");
	}

      /* Now we know which file to use.
	 Set up its pointer and transfer the data.  */
      if (xferfile)
	{
	  if (*xferfile == 0)
	    if (xferfile == &execfile)
	      error ("No program file to examine.");
	    else
	      error ("No core dump file or running program to examine.");
	  val = lseek (xferchan, fileptr, 0);
	  if (val < 0)
	    perror_with_name (*xferfile);
	  val = myread (xferchan, myaddr, i);
	  if (val < 0)
	    perror_with_name (*xferfile);
	}
      /* If this address is for nonexistent memory,
	 read zeros if reading, or do nothing if writing.
	 Actually, we never right.  */
      else
	{
	  memset (myaddr, '\0', i);
	  returnval = EIO;
	}

      memaddr += i;
      myaddr += i;
      len -= i;
    }
  return returnval;
}
#endif

/* APCS (ARM procedure call standard) defines the following prologue:

   mov		ip, sp
  [stmfd	sp!, {a1,a2,a3,a4}]
   stmfd	sp!, {...,fp,ip,lr,pc}
  [stfe		f7, [sp, #-12]!]
  [stfe		f6, [sp, #-12]!]
  [stfe		f5, [sp, #-12]!]
  [stfe		f4, [sp, #-12]!]
   sub		fp, ip, #nn	// nn == 20 or 4 depending on second ins
*/

CORE_ADDR
skip_prologue(pc)
CORE_ADDR pc;
{
    CORE_ADDR skip_pc = pc;
#if 0
    union insn_fmt op;

    op.ins = read_memory_integer(skip_pc, 4);
    /* look for the "mov ip,sp" */
    if (op.generic.type != TYPE_ARITHMETIC ||
	op.arith.opcode != OPCODE_MOV ||
	op.arith.dest != SPTEMP ||
	op.arith.operand2 != SP) return pc;
    skip_pc += 4;
    /* skip the "stmfd sp!,{a1,a2,a3,a4}" if its there */
    op.ins = read_memory_integer(skip_pc, 4);
    if (op.generic.type == TYPE_BLOCK_BRANCH &&
	op.generic.subtype == SUBTYPE_BLOCK &&
	op.block.mask == 0xf &&
	op.block.base == SP &&
	op.block.is_load == 0 &&
	op.block.writeback == 1 &&
	op.block.increment == 0 &&
	op.block.before == 1) skip_pc += 4;
    /* skip the "stmfd sp!,{...,fp,ip,lr,pc} */
    op.ins = read_memory_integer(skip_pc, 4);
    if (op.generic.type != TYPE_BLOCK_BRANCH ||
	op.generic.subtype != SUBTYPE_BLOCK ||
	/* the mask should look like 110110xxxxxx0000 */
	(op.block.mask & 0xd800) != 0xd800 ||
	op.block.base != SP ||
	op.block.is_load != 0 ||
	op.block.writeback != 1 ||
	op.block.increment != 0 ||
	op.block.before != 1) return pc;
    skip_pc += 4;
    /* check for "sub fp,ip,#nn" */
    op.ins = read_memory_integer(skip_pc, 4);
    if (op.generic.type != TYPE_ARITHMETIC ||
	op.arith.opcode != OPCODE_SUB ||
	op.arith.dest != FP ||
	op.arith.operand1 != SPTEMP) return pc;
#endif
    return skip_pc + 4;
}

void
arm_frame_find_saved_regs (frame_info, saved_regs_addr)
     struct frame_info *frame_info;
     struct frame_saved_regs *saved_regs_addr;
{
  register int regnum;
  register int frame;
  register int next_addr;
  register int return_data_save;
  register int saved_register_mask;

  memset (saved_regs_addr, '\0', sizeof (*saved_regs_addr));
  frame = frame_info->frame;
  return_data_save = read_memory_integer (frame, 4) & 0x03fffffc - 12;
  saved_register_mask = read_memory_integer (return_data_save, 4);
  next_addr = frame - 12;
  for (regnum = 4; regnum < 10; regnum++)
    if (saved_register_mask & (1 << regnum))
      {
	next_addr -= 4;
	saved_regs_addr->regs[regnum] = next_addr;
      }
  if (read_memory_integer (return_data_save + 4, 4) == 0xed6d7103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 7] = next_addr;
    }
  if (read_memory_integer (return_data_save + 8, 4) == 0xed6d6103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 6] = next_addr;
    }
  if (read_memory_integer (return_data_save + 12, 4) == 0xed6d5103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 5] = next_addr;
    }
  if (read_memory_integer(return_data_save + 16, 4) == 0xed6d4103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 4] = next_addr;
    }
  saved_regs_addr->regs[SP_REGNUM] = next_addr;
  saved_regs_addr->regs[PC_REGNUM] = frame - 4;
  saved_regs_addr->regs[PS_REGNUM] = frame - 4;
  saved_regs_addr->regs[FP_REGNUM] = frame - 12;
}

static void
print_fpu_flags(flags)
int flags;
{
    if (flags & (1 << 0)) fputs("IVO ", stdout);
    if (flags & (1 << 1)) fputs("DVZ ", stdout);
    if (flags & (1 << 2)) fputs("OFL ", stdout);
    if (flags & (1 << 3)) fputs("UFL ", stdout);
    if (flags & (1 << 4)) fputs("INX ", stdout);
    putchar('\n');
}

void
arm_float_info()
{
    register unsigned long status = read_register(FPS_REGNUM);
    int type;

    type = (status >> 24) & 127;
    printf("%s FPU type %d\n",
	   (status & (1<<31)) ? "Hardware" : "Software",
	   type);
    fputs("mask: ", stdout);
    print_fpu_flags(status >> 16);
    fputs("flags: ", stdout);
    print_fpu_flags(status);
}


static void arm_othernames()
{
  static int toggle;
  static char *original[] = ORIGINAL_REGISTER_NAMES;
  static char *extra_crispy[] = ADDITIONAL_REGISTER_NAMES;

  memcpy (reg_names, toggle ? extra_crispy : original, sizeof(original));
  toggle = !toggle;
}
void
_initialize_arm_tdep ()
{
  tm_print_insn = print_insn_little_arm;
  add_com ("othernames", class_obscure, arm_othernames);
}

/* FIXME:  Fill in with the 'right thing', see asm 
   template in arm-convert.s */

void 
convert_from_extended (ptr, dbl)
void *ptr;
double *dbl;
{
  *dbl = *(double*)ptr;
}


void 
convert_to_extended (dbl, ptr)
void *ptr;
double *dbl;
{
  *(double*)ptr = *dbl;
}


int
arm_nullified_insn (inst)
     unsigned long inst;
{
  unsigned long cond = inst & 0xf0000000;
  unsigned long status_reg;

  if (cond == INST_AL || cond == INST_NV)
    return 0;

  status_reg = read_register (PS_REGNUM);

  switch (cond)
    {
    case INST_EQ:
      return ((status_reg & FLAG_Z) == 0);
    case INST_NE:
      return ((status_reg & FLAG_Z) != 0);
    case INST_CS:
      return ((status_reg & FLAG_C) == 0);
    case INST_CC:
      return ((status_reg & FLAG_C) != 0);
    case INST_MI:
      return ((status_reg & FLAG_N) == 0);
    case INST_PL:
      return ((status_reg & FLAG_N) != 0);
    case INST_VS:
      return ((status_reg & FLAG_V) == 0);
    case INST_VC:
      return ((status_reg & FLAG_V) != 0);
    case INST_HI:
      return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
    case INST_LS:
      return (((status_reg & (FLAG_C | FLAG_Z)) ^ FLAG_C) == 0);
    case INST_GE:
      return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
    case INST_LT:
      return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_GT:
      return (((status_reg & FLAG_Z) != 0) ||
	      (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
    case INST_LE:
      return (((status_reg & FLAG_Z) == 0) &&
	      (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    }
  return 0;
}



/* taken from remote-arm.c .. */

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) & (1L << (st))) >> st)
#define bits(obj,st,fn) \
  (((obj) & submask (fn) & ~ submask ((st) - 1)) >> (st))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1

static unsigned long
shifted_reg_val (inst, carry, pc_val)
     unsigned long inst;
     int carry;
     unsigned long pc_val;
{
  unsigned long res, shift;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);
 
  if (bit(inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);
 
  res = (rm == 15 
	 ? ((pc_val | (ARM_PC_32 ? 0 : read_register (PS_REGNUM)))
	    + (bit (inst, 4) ? 12 : 8)) 
	 : read_register (rm));

  switch (shifttype)
    {
    case 0: /* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;
      
    case 1: /* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2: /* ASR */
      if (shift >= 32) shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3: /* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32-shift));
      break;
    }

  return res & 0xffffffff;
}


CORE_ADDR
arm_get_next_pc (pc)
     CORE_ADDR pc;
{
  unsigned long pc_val = (unsigned long) pc;
  unsigned long this_instr = read_memory_integer (pc, 4);
  unsigned long status = read_register (PS_REGNUM);
  CORE_ADDR nextpc = (CORE_ADDR) (pc_val + 4);  /* Default case */

  if (! arm_nullified_insn (this_instr))
    {
      switch (bits(this_instr, 24, 27))
	{
	case 0x0: case 0x1: /* data processing */
	case 0x2: case 0x3:
	  {
	    unsigned long operand1, operand2, result = 0;
	    unsigned long rn;
	    int c;
 
	    if (bits(this_instr, 12, 15) != 15)
	      break;

	    if (bits (this_instr, 22, 25) == 0
		&& bits (this_instr, 4, 7) == 9)  /* multiply */
	      error ("Illegal update to pc in instruction");

	    /* Multiply into PC */
	    c = (status & FLAG_C) ? 1 : 0;
	    rn = bits (this_instr, 16, 19);
	    operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);
 
	    if (bit (this_instr, 25))
	      {
		unsigned long immval = bits (this_instr, 0, 7);
		unsigned long rotate = 2 * bits (this_instr, 8, 11);
		operand2 = ((immval >> rotate) | (immval << (32-rotate))
			    & 0xffffffff);
	      }
	    else  /* operand 2 is a shifted register */
	      operand2 = shifted_reg_val (this_instr, c, pc_val);
 
	    switch (bits (this_instr, 21, 24))
	      {
	      case 0x0: /*and*/
		result = operand1 & operand2;
		break;

	      case 0x1: /*eor*/
		result = operand1 ^ operand2;
		break;

	      case 0x2: /*sub*/
		result = operand1 - operand2;
		break;

	      case 0x3: /*rsb*/
		result = operand2 - operand1;
		break;

	      case 0x4:  /*add*/
		result = operand1 + operand2;
		break;

	      case 0x5: /*adc*/
		result = operand1 + operand2 + c;
		break;

	      case 0x6: /*sbc*/
		result = operand1 - operand2 + c;
		break;

	      case 0x7: /*rsc*/
		result = operand2 - operand1 + c;
		break;

	      case 0x8: case 0x9: case 0xa: case 0xb: /* tst, teq, cmp, cmn */
		result = (unsigned long) nextpc;
		break;

	      case 0xc: /*orr*/
		result = operand1 | operand2;
		break;

	      case 0xd: /*mov*/
		/* Always step into a function.  */
		result = operand2;
                break;

	      case 0xe: /*bic*/
		result = operand1 & ~operand2;
		break;

	      case 0xf: /*mvn*/
		result = ~operand2;
		break;
	      }
	    nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }
 
	case 0x4: case 0x5: /* data transfer */
	case 0x6: case 0x7:
	  if (bit (this_instr, 20))
	    {
	      /* load */
	      if (bits (this_instr, 12, 15) == 15)
		{
		  /* rd == pc */
		  unsigned long  rn;
		  unsigned long base;
 
		  if (bit (this_instr, 22))
		    error ("Illegal update to pc in instruction");

		  /* byte write to PC */
		  rn = bits (this_instr, 16, 19);
		  base = (rn == 15) ? pc_val + 8 : read_register (rn);
		  if (bit (this_instr, 24))
		    {
		      /* pre-indexed */
		      int c = (status & FLAG_C) ? 1 : 0;
		      unsigned long offset =
			(bit (this_instr, 25)
			 ? shifted_reg_val (this_instr, c, pc_val)
			 : bits (this_instr, 0, 11));

		      if (bit (this_instr, 23))
			base += offset;
		      else
			base -= offset;
		    }
		  nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) base, 
							    4);
 
		  nextpc = ADDR_BITS_REMOVE (nextpc);

		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;
 
	case 0x8: case 0x9: /* block transfer */
	  if (bit (this_instr, 20))
	    {
	      /* LDM */
	      if (bit (this_instr, 15))
		{
		  /* loading pc */
		  int offset = 0;

		  if (bit (this_instr, 23))
		    {
		      /* up */
		      unsigned long reglist = bits (this_instr, 0, 14);
		      unsigned long regbit;

		      for (; reglist != 0; reglist &= ~regbit)
			{
			  regbit = reglist & (-reglist);
			  offset += 4;
			}

		      if (bit (this_instr, 24)) /* pre */
			offset += 4;
		    }
		  else if (bit (this_instr, 24))
		    offset = -4;
 
		  {
		    unsigned long rn_val = 
		      read_register (bits (this_instr, 16, 19));
		    nextpc =
		      (CORE_ADDR) read_memory_integer ((CORE_ADDR) (rn_val
								    + offset),
						       4);
		  }
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;
 
	case 0xb:           /* branch & link */
	case 0xa:           /* branch */
	  {
	    nextpc = BranchDest (pc, this_instr);

	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }
 
	case 0xc: case 0xd:
	case 0xe:           /* coproc ops */
	case 0xf:           /* SWI */
	  break;

	default:
	  fprintf (stderr, "Bad bit-field extraction\n");
	  return (pc);
	}
    }

  return nextpc;
}

