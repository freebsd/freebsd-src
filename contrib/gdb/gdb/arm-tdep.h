/* Common target dependent code for GDB on ARM systems.
   Copyright 2002 Free Software Foundation, Inc.

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

/* Register numbers of various important registers.  Note that some of
   these values are "real" register numbers, and correspond to the
   general registers of the machine, and some are "phony" register
   numbers which are too large to be actual register numbers as far as
   the user is concerned but do serve to get the desired values when
   passed to read_register.  */

#define ARM_A1_REGNUM 0		/* first integer-like argument */
#define ARM_A4_REGNUM 3		/* last integer-like argument */
#define ARM_AP_REGNUM 11
#define ARM_SP_REGNUM 13	/* Contains address of top of stack */
#define ARM_LR_REGNUM 14	/* address to return to from a function call */
#define ARM_PC_REGNUM 15	/* Contains program counter */
#define ARM_F0_REGNUM 16	/* first floating point register */
#define ARM_F3_REGNUM 19	/* last floating point argument register */
#define ARM_F7_REGNUM 23	/* last floating point register */
#define ARM_FPS_REGNUM 24	/* floating point status register */
#define ARM_PS_REGNUM 25	/* Contains processor status */

#define ARM_FP_REGNUM 11	/* Frame register in ARM code, if used.  */
#define THUMB_FP_REGNUM 7	/* Frame register in Thumb code, if used.  */

#define ARM_NUM_ARG_REGS 	4
#define ARM_LAST_ARG_REGNUM 	ARM_A4_REGNUM
#define ARM_NUM_FP_ARG_REGS 	4
#define ARM_LAST_FP_ARG_REGNUM	ARM_F3_REGNUM

/* Size of integer registers.  */
#define INT_REGISTER_RAW_SIZE		4
#define INT_REGISTER_VIRTUAL_SIZE	4

/* Say how long FP registers are.  Used for documentation purposes and
   code readability in this header.  IEEE extended doubles are 80
   bits.  DWORD aligned they use 96 bits.  */
#define FP_REGISTER_RAW_SIZE	12

/* GCC doesn't support long doubles (extended IEEE values).  The FP
   register virtual size is therefore 64 bits.  Used for documentation
   purposes and code readability in this header.  */
#define FP_REGISTER_VIRTUAL_SIZE	8

/* Status registers are the same size as general purpose registers.
   Used for documentation purposes and code readability in this
   header.  */
#define STATUS_REGISTER_SIZE	4

/* Number of machine registers.  The only define actually required 
   is NUM_REGS.  The other definitions are used for documentation
   purposes and code readability.  */
/* For 26 bit ARM code, a fake copy of the PC is placed in register 25 (PS)
   (and called PS for processor status) so the status bits can be cleared
   from the PC (register 15).  For 32 bit ARM code, a copy of CPSR is placed
   in PS.  */
#define NUM_FREGS	8	/* Number of floating point registers.  */
#define NUM_SREGS	2	/* Number of status registers.  */
#define NUM_GREGS	16	/* Number of general purpose registers.  */


/* Instruction condition field values.  */
#define INST_EQ		0x0
#define INST_NE		0x1
#define INST_CS		0x2
#define INST_CC		0x3
#define INST_MI		0x4
#define INST_PL		0x5
#define INST_VS		0x6
#define INST_VC		0x7
#define INST_HI		0x8
#define INST_LS		0x9
#define INST_GE		0xa
#define INST_LT		0xb
#define INST_GT		0xc
#define INST_LE		0xd
#define INST_AL		0xe
#define INST_NV		0xf

#define FLAG_N		0x80000000
#define FLAG_Z		0x40000000
#define FLAG_C		0x20000000
#define FLAG_V		0x10000000

/* ABI variants that we know about.  If you add to this enum, please 
   update the table of names in tm-arm.c.  */
enum arm_abi
{
  ARM_ABI_UNKNOWN = 0,
  ARM_ABI_EABI_V1,
  ARM_ABI_EABI_V2,
  ARM_ABI_LINUX,
  ARM_ABI_NETBSD_AOUT,
  ARM_ABI_NETBSD_ELF,
  ARM_ABI_APCS,
  ARM_ABI_FREEBSD,
  ARM_ABI_WINCE,

  ARM_ABI_INVALID	/* Keep this last.  */
};

/* Type of floating-point code in use by inferior.  There are really 3 models
   that are traditionally supported (plus the endianness issue), but gcc can
   only generate 2 of those.  The third is APCS_FLOAT, where arguments to
   functions are passed in floating-point registers.  

   In addition to the traditional models, VFP adds two more.  */

enum arm_float_model
{
  ARM_FLOAT_SOFT,
  ARM_FLOAT_FPA,
  ARM_FLOAT_SOFT_VFP,
  ARM_FLOAT_VFP
};

/* Target-dependent structure in gdbarch.  */
struct gdbarch_tdep
{
  enum arm_abi arm_abi;		/* OS/ABI of inferior.  */
  const char *abi_name;		/* Name of the above.  */

  enum arm_float_model fp_model; /* Floating point calling conventions.  */

  CORE_ADDR lowest_pc;		/* Lowest address at which instructions 
				   will appear.  */

  const char *arm_breakpoint;	/* Breakpoint pattern for an ARM insn.  */
  int arm_breakpoint_size;	/* And its size.  */
  const char *thumb_breakpoint;	/* Breakpoint pattern for an ARM insn.  */
  int thumb_breakpoint_size;	/* And its size.  */

  int jb_pc;			/* Offset to PC value in jump buffer. 
				   If this is negative, longjmp support
				   will be disabled.  */
  size_t jb_elt_size;		/* And the size of each entry in the buf.  */
};

#ifndef LOWEST_PC
#define LOWEST_PC (gdbarch_tdep (current_gdbarch)->lowest_pc)
#endif

/* Prototypes for internal interfaces needed by more than one MD file.  */
int arm_pc_is_thumb_dummy (CORE_ADDR);

int arm_pc_is_thumb (CORE_ADDR);

CORE_ADDR thumb_get_next_pc (CORE_ADDR);

CORE_ADDR arm_get_next_pc (CORE_ADDR);

/* How a OS variant tells the ARM generic code that it can handle an ABI
   type. */
void
arm_gdbarch_register_os_abi (enum arm_abi abi,
			     void (*init_abi)(struct gdbarch_info,
					      struct gdbarch *));
