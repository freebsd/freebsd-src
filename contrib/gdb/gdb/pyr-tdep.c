/* Pyramid target-dependent code for GDB.
   Copyright (C) 1988, 1989, 1991 Free Software Foundation, Inc.

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

/*** Prettier register printing. ***/

/* Print registers in the same format as pyramid's dbx, adb, sdb.  */
pyr_print_registers(reg_buf, regnum)
    long *reg_buf[];
{
  register int regno;
  int usp, ksp;
  struct user u;

  for (regno = 0; regno < 16; regno++) {
    printf_unfiltered/*_filtered*/ ("%6.6s: %8x  %6.6s: %8x  %6s: %8x  %6s: %8x\n",
		     reg_names[regno], reg_buf[regno],
		     reg_names[regno+16], reg_buf[regno+16],
		     reg_names[regno+32], reg_buf[regno+32],
		     reg_names[regno+48], reg_buf[regno+48]);
  }
  usp = ptrace (3, inferior_pid,
		(PTRACE_ARG3_TYPE) ((char *)&u.u_pcb.pcb_usp) -
		((char *)&u), 0);
  ksp = ptrace (3, inferior_pid,
		(PTRACE_ARG3_TYPE) ((char *)&u.u_pcb.pcb_ksp) -
		((char *)&u), 0);
  printf_unfiltered/*_filtered*/ ("\n%6.6s: %8x  %6.6s: %8x (%08x) %6.6s %8x\n",
		   reg_names[CSP_REGNUM],reg_buf[CSP_REGNUM],
		   reg_names[KSP_REGNUM], reg_buf[KSP_REGNUM], ksp,
		   "usp", usp);
}

/* Print the register regnum, or all registers if regnum is -1.
   fpregs is currently ignored.  */

pyr_do_registers_info (regnum, fpregs)
    int regnum;
    int fpregs;
{
  /* On a pyr, we know a virtual register can always fit in an long.
     Here (and elsewhere) we take advantage of that.  Yuk.  */
  long raw_regs[MAX_REGISTER_RAW_SIZE*NUM_REGS];
  register int i;
  
  for (i = 0 ; i < 64 ; i++) {
    read_relative_register_raw_bytes(i, raw_regs+i);
  }
  if (regnum == -1)
    pyr_print_registers (raw_regs, regnum);
  else
    for (i = 0; i < NUM_REGS; i++)
      if (i == regnum) {
	long val = raw_regs[i];
	
	fputs_filtered (reg_names[i], stdout);
	printf_filtered(":");
	print_spaces_filtered (6 - strlen (reg_names[i]), stdout);
	if (val == 0)
	  printf_filtered ("0");
	else
	  printf_filtered ("%s  %d", local_hex_string_custom(val,"08"), val);
	printf_filtered("\n");
      }
}

/*** Debugging editions of various macros from m-pyr.h ****/

CORE_ADDR frame_locals_address (frame)
    struct frame_info *frame;
{
  register int addr = find_saved_register (frame,CFP_REGNUM);
  register int result = read_memory_integer (addr, 4);
#ifdef PYRAMID_CONTROL_FRAME_DEBUGGING
  fprintf_unfiltered (stderr,
	   "\t[[..frame_locals:%8x, %s= %x @%x fcfp= %x foo= %x\n\t gr13=%x pr13=%x tr13=%x @%x]]\n",
	   frame->frame,
	   reg_names[CFP_REGNUM],
	   result, addr,
	   frame->frame_cfp, (CFP_REGNUM),


	   read_register(13), read_register(29), read_register(61),
	   find_saved_register(frame, 61));
#endif /* PYRAMID_CONTROL_FRAME_DEBUGGING */

  /* FIXME: I thought read_register (CFP_REGNUM) should be the right answer;
     or at least CFP_REGNUM relative to FRAME (ie, result).
     There seems to be a bug in the way the innermost frame is set up.  */

    return ((frame->next) ? result: frame->frame_cfp);
}

CORE_ADDR frame_args_addr (frame)
    struct frame_info *frame;
{
  register int addr = find_saved_register (frame,CFP_REGNUM);
  register int result = read_memory_integer (addr, 4);

#ifdef PYRAMID_CONTROL_FRAME_DEBUGGING
  fprintf_unfiltered (stderr,
	   "\t[[..frame_args:%8x, %s= %x @%x fcfp= %x r_r= %x\n\t gr13=%x pr13=%x tr13=%x @%x]]\n",
	   frame->frame,
	   reg_names[CFP_REGNUM],
	   result, addr,
	   frame->frame_cfp, read_register(CFP_REGNUM),

	   read_register(13), read_register(29), read_register(61),
	   find_saved_register(frame, 61));
#endif /*  PYRAMID_CONTROL_FRAME_DEBUGGING */

  /* FIXME: I thought read_register (CFP_REGNUM) should be the right answer;
     or at least CFP_REGNUM relative to FRAME (ie, result).
     There seems to be a bug in the way the innermost frame is set up.  */
    return ((frame->next) ? result: frame->frame_cfp);
}

#include "symtab.h"
#include "opcode/pyr.h"
#include "gdbcore.h"


/*  A couple of functions used for debugging frame-handling on
    Pyramids. (The Pyramid-dependent handling of register values for
    windowed registers is known to be buggy.)

    When debugging, these functions can supplant the normal definitions of some
    of the macros in tm-pyramid.h  The quantity of information produced
    when these functions are used makes the gdb  unusable as a
    debugger for user programs.  */
    
extern unsigned pyr_saved_pc(), pyr_frame_chain();

CORE_ADDR pyr_frame_chain(frame)
    CORE_ADDR frame;
{
    int foo=frame - CONTROL_STACK_FRAME_SIZE;
    /* printf_unfiltered ("...following chain from %x: got %x\n", frame, foo);*/
    return foo;
}

CORE_ADDR pyr_saved_pc(frame)
    CORE_ADDR frame;
{
    int foo=0;
    foo = read_memory_integer (((CORE_ADDR)(frame))+60, 4);
    printf_unfiltered ("..reading pc from frame 0x%0x+%d regs: got %0x\n",
	    frame, 60/4, foo);
    return foo;
}

/* Pyramid instructions are never longer than this many bytes.  */
#define MAXLEN 24

/* Number of elements in the opcode table.  */
/*const*/ static int nopcodes = (sizeof (pyr_opcodes) / sizeof( pyr_opcodes[0]));
#define NOPCODES (nopcodes)

/* Let's be byte-independent so we can use this as a cross-assembler.  */

#define NEXTLONG(p)  \
  (p += 4, (((((p[-4] << 8) + p[-3]) << 8) + p[-2]) << 8) + p[-1])

/* Print one instruction at address MEMADDR in debugged memory,
   on STREAM.  Returns length of the instruction, in bytes.  */

int
pyr_print_insn (memaddr, stream)
     CORE_ADDR memaddr;
     FILE *stream;
{
  unsigned char buffer[MAXLEN];
  register int i, nargs, insn_size =4;
  register unsigned char *p;
  register char *d;
  register int insn_opcode, operand_mode;
  register int index_multiplier, index_reg_regno, op_1_regno, op_2_regno ;
  long insn;			/* first word of the insn, not broken down. */
  pyr_insn_format insn_decode;	/* the same, broken out into op{code,erands} */
  long extra_1, extra_2;

  read_memory (memaddr, buffer, MAXLEN);
  insn_decode = *((pyr_insn_format *) buffer);
  insn = * ((int *) buffer);
  insn_opcode = insn_decode.operator;
  operand_mode = insn_decode.mode;
  index_multiplier = insn_decode.index_scale;
  index_reg_regno = insn_decode.index_reg;
  op_1_regno = insn_decode.operand_1;
  op_2_regno = insn_decode.operand_2;
  
  
  if (*((int *)buffer) == 0x0) {
    /* "halt" looks just like an invalid "jump" to the insn decoder,
       so is dealt with as a special case */
    fprintf_unfiltered (stream, "halt");
    return (4);
  }

  for (i = 0; i < NOPCODES; i++)
	  if (pyr_opcodes[i].datum.code == insn_opcode)
		  break;

  if (i == NOPCODES)
	  /* FIXME: Handle unrecognised instructions better.  */
	  fprintf_unfiltered (stream, "???\t#%08x\t(op=%x mode =%x)",
		   insn, insn_decode.operator, insn_decode.mode);
  else
    {
      /* Print the mnemonic for the instruction.  Pyramid insn operands
         are so regular that we can deal with almost all of them
         separately.
	 Unconditional branches are an exception: they are encoded as
	 conditional branches (branch if false condition, I think)
	 with no condition specified. The average user will not be
	 aware of this. To maintain their illusion that an
	 unconditional branch insn exists, we will have to FIXME to
	 treat the insn mnemnonic of all branch instructions here as a
	 special case: check the operands of branch insn and print an
	 appropriate mnemonic. */ 

      fprintf_unfiltered (stream, "%s\t", pyr_opcodes[i].name);

    /* Print the operands of the insn (as specified in
       insn.operand_mode). 
       Branch operands of branches are a special case: they are a word
       offset, not a byte offset. */
  
    if (insn_decode.operator == 0x01 || insn_decode.operator == 0x02) {
      register int bit_codes=(insn >> 16)&0xf;
      register int i;
      register int displacement = (insn & 0x0000ffff) << 2;

      static char cc_bit_names[] = "cvzn";	/* z,n,c,v: strange order? */

      /* Is bfc and no bits specified an unconditional branch?*/
      for (i=0;i<4;i++) {
	if ((bit_codes) & 0x1)
		fputc_unfiltered (cc_bit_names[i], stream);
	bit_codes >>= 1;
      }

      fprintf_unfiltered (stream, ",%0x",
	       displacement + memaddr);
      return (insn_size);
    }

      switch (operand_mode) {
      case 0:
	fprintf_unfiltered (stream, "%s,%s",
		 reg_names [op_1_regno],
		 reg_names [op_2_regno]);
	break;
	    
      case 1:
	fprintf_unfiltered (stream, " 0x%0x,%s",
		 op_1_regno,
		 reg_names [op_2_regno]);
	break;
	
      case 2:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream, " $0x%0x,%s",
		 extra_1,
		 reg_names [op_2_regno]);
	break;
      case 3:
	fprintf_unfiltered (stream, " (%s),%s",
		 reg_names [op_1_regno],
		 reg_names [op_2_regno]);
	break;
	
      case 4:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream, " 0x%0x(%s),%s",
		 extra_1,
		 reg_names [op_1_regno],
		 reg_names [op_2_regno]);
	break;
	
	/* S1 destination mode */
      case 5:
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? "%s,(%s)[%s*%1d]" : "%s,(%s)"),
		 reg_names [op_1_regno],
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
      case 6:
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? " $%#0x,(%s)[%s*%1d]"
		  : " $%#0x,(%s)"),
		 op_1_regno,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
      case 7:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? " $%#0x,(%s)[%s*%1d]"
		  : " $%#0x,(%s)"),
		 extra_1,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
      case 8:
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? " (%s),(%s)[%s*%1d]" : " (%s),(%s)"),
		 reg_names [op_1_regno],
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
      case 9:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno)
		  ? "%#0x(%s),(%s)[%s*%1d]"
		  : "%#0x(%s),(%s)"),
		 extra_1,
		 reg_names [op_1_regno],
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
	/* S2 destination mode */
      case 10:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? "%s,%#0x(%s)[%s*%1d]" : "%s,%#0x(%s)"),
		 reg_names [op_1_regno],
		 extra_1,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
      case 11:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ?
		  " $%#0x,%#0x(%s)[%s*%1d]" : " $%#0x,%#0x(%s)"),
		 op_1_regno,
		 extra_1,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
      case 12:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	read_memory (memaddr+8, buffer, MAXLEN);
	insn_size += 4;
	extra_2 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ?
		  " $%#0x,%#0x(%s)[%s*%1d]" : " $%#0x,%#0x(%s)"),
		 extra_1,
		 extra_2,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
      case 13:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno)
		  ? " (%s),%#0x(%s)[%s*%1d]" 
		  : " (%s),%#0x(%s)"),
		 reg_names [op_1_regno],
		 extra_1,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
      case 14:
	read_memory (memaddr+4, buffer, MAXLEN);
	insn_size += 4;
	extra_1 = * ((int *) buffer);
	read_memory (memaddr+8, buffer, MAXLEN);
	insn_size += 4;
	extra_2 = * ((int *) buffer);
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? "%#0x(%s),%#0x(%s)[%s*%1d]"
		  : "%#0x(%s),%#0x(%s) "),
		 extra_1,
		 reg_names [op_1_regno],
		 extra_2,
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	break;
	
      default:
	fprintf_unfiltered (stream,
		 ((index_reg_regno) ? "%s,%s [%s*%1d]" : "%s,%s"),
		 reg_names [op_1_regno],
		 reg_names [op_2_regno],
		 reg_names [index_reg_regno],
		 index_multiplier);
	fprintf_unfiltered (stream,
		 "\t\t# unknown mode in %08x",
		 insn);
	break;
      } /* switch */
    }
  
  {
    return insn_size;
  }
  abort ();
}
