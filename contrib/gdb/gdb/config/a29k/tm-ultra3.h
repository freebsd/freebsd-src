/* Parameters for NYU Ultracomputer 29000 target, for GDB, the GNU debugger.
   Copyright 1990, 1991 Free Software Foundation, Inc.
   Contributed by David Wood @ New York University (wood@nyu.edu). 
   
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

/* This file includes tm-a29k.h, but predefines REGISTER_NAMES and
   related macros.  The file supports a a29k running our flavor of
   Unix on our Ultra3 PE Boards.  */

/* Byte order is configurable, but this machine runs big-endian.  */
#define TARGET_BYTE_ORDER BIG_ENDIAN

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.
 */
#define NUM_REGS   (EXO_REGNUM + 1)	

#define REGISTER_NAMES { 						 \
 "gr1",									 \
 "gr64", "gr65", "gr66", "gr67", "gr68", "gr69", "gr70", "gr71", "gr72", \
 "gr73", "gr74", "gr75", "gr76", "gr77", "gr78", "gr79", "gr80", "gr81", \
 "gr82", "gr83", "gr84", "gr85", "gr86", "gr87", "gr88", "gr89", "gr90", \
 "gr91", "gr92", "gr93", "gr94", "gr95",				 \
 "gr96", "gr97", "gr98", "gr99", "gr100", "gr101", "gr102", "gr103", "gr104", \
 "gr105", "gr106", "gr107", "gr108", "gr109", "gr110", "gr111", "gr112", \
 "gr113", "gr114", "gr115", "gr116", "gr117", "gr118", "gr119", "gr120", \
 "gr121", "gr122", "gr123", "gr124", "gr125", "gr126", "gr127",		 \
 "lr0", "lr1", "lr2", "lr3", "lr4", "lr5", "lr6", "lr7", "lr8", "lr9",   \
 "lr10", "lr11", "lr12", "lr13", "lr14", "lr15", "lr16", "lr17", "lr18", \
 "lr19", "lr20", "lr21", "lr22", "lr23", "lr24", "lr25", "lr26", "lr27", \
 "lr28", "lr29", "lr30", "lr31", "lr32", "lr33", "lr34", "lr35", "lr36", \
 "lr37", "lr38", "lr39", "lr40", "lr41", "lr42", "lr43", "lr44", "lr45", \
 "lr46", "lr47", "lr48", "lr49", "lr50", "lr51", "lr52", "lr53", "lr54", \
 "lr55", "lr56", "lr57", "lr58", "lr59", "lr60", "lr61", "lr62", "lr63", \
 "lr64", "lr65", "lr66", "lr67", "lr68", "lr69", "lr70", "lr71", "lr72", \
 "lr73", "lr74", "lr75", "lr76", "lr77", "lr78", "lr79", "lr80", "lr81", \
 "lr82", "lr83", "lr84", "lr85", "lr86", "lr87", "lr88", "lr89", "lr90", \
 "lr91", "lr92", "lr93", "lr94", "lr95", "lr96", "lr97", "lr98", "lr99", \
 "lr100", "lr101", "lr102", "lr103", "lr104", "lr105", "lr106", "lr107", \
 "lr108", "lr109", "lr110", "lr111", "lr112", "lr113", "lr114", "lr115", \
 "lr116", "lr117", "lr118", "lr119", "lr120", "lr121", "lr122", "lr123", \
 "lr124", "lr125", "lr126", "lr127",					 \
 "vab", "ops", "cps", "cfg", "cha", "chd", "chc", "rbp", "tmc", "tmr",	 \
 "pc0", "pc1", "pc2", "mmu", "lru",					 \
 "ipc", "ipa", "ipb", "q", "alu", "bp", "fc", "cr",			 \
 "fpe", "int", "fps", "exo" }


#ifdef KERNEL_DEBUGGING
# define	PADDR_U_REGNUM	22		/* gr86 */
# define	RETURN_REGNUM	GR64_REGNUM	
#else
# define	RETURN_REGNUM	GR96_REGNUM	
#endif	/* KERNEL_DEBUGGING */


/* Should rename all GR96_REGNUM to RETURN_REGNUM */ 
#define GR1_REGNUM 	(0)
#define GR64_REGNUM	1
#define GR96_REGNUM 	(GR64_REGNUM + 32)
/* This needs to be the memory stack pointer, not the register stack pointer,
   to make call_function work right.  */
#define SP_REGNUM 	MSP_REGNUM

#define FP_REGNUM 	(LR0_REGNUM + 1)	/* lr1 */
/* Large Return Pointer  */
#define LRP_REGNUM (123 - 96 + RETURN_REGNUM)
/* Static link pointer   */
#define SLP_REGNUM (124 - 96 + RETURN_REGNUM)
/* Memory Stack Pointer.  */
#define MSP_REGNUM (125 - 96 + RETURN_REGNUM)
/* Register allocate bound.  */
#define RAB_REGNUM (126 - 96 + RETURN_REGNUM)
/* Register Free Bound.  */
#define RFB_REGNUM (127 - 96 + RETURN_REGNUM)
/* Register Stack Pointer.  */
#define RSP_REGNUM GR1_REGNUM
#define LR0_REGNUM ( 32 +  GR96_REGNUM) 

/* Protected Special registers */
#define VAB_REGNUM (LR0_REGNUM +  128)
#define OPS_REGNUM (VAB_REGNUM + 1)  
#define CPS_REGNUM (VAB_REGNUM + 2)  
#define CFG_REGNUM (VAB_REGNUM + 3)  
#define CHA_REGNUM (VAB_REGNUM + 4)  
#define CHD_REGNUM (VAB_REGNUM + 5)  
#define CHC_REGNUM (VAB_REGNUM + 6)  
#define RBP_REGNUM (VAB_REGNUM + 7)  
#define TMC_REGNUM (VAB_REGNUM + 8)  
#define TMR_REGNUM (VAB_REGNUM + 9)  
#define NPC_REGNUM (VAB_REGNUM + 10)	/* pc0 */
#define PC_REGNUM  (VAB_REGNUM + 11)  	/* pc1 */
#define PC2_REGNUM (VAB_REGNUM + 12)  	/* pc2 */
#define MMU_REGNUM (VAB_REGNUM + 13)
#define LRU_REGNUM (VAB_REGNUM + 14)
	/* Register sequence gap */
/* Unprotected Special registers */
#define IPC_REGNUM (LRU_REGNUM + 1) 
#define IPA_REGNUM (IPC_REGNUM + 1) 
#define IPB_REGNUM (IPC_REGNUM + 2) 
#define Q_REGNUM   (IPC_REGNUM + 3) 
#define ALU_REGNUM (IPC_REGNUM + 4) 
#define PS_REGNUM  ALU_REGNUM
#define BP_REGNUM  (IPC_REGNUM + 5) 
#define FC_REGNUM  (IPC_REGNUM + 6) 
#define CR_REGNUM  (IPC_REGNUM + 7) 
	/* Register sequence gap */
#define FPE_REGNUM (CR_REGNUM  + 1) 
#define INT_REGNUM (FPE_REGNUM + 1) 
#define FPS_REGNUM (FPE_REGNUM + 2) 
	/* Register sequence gap */
#define EXO_REGNUM (FPS_REGNUM + 1) 

/* Special register #x.  */
#define SR_REGNUM(x) \
  ((x) < 15  ? VAB_REGNUM + (x)					 \
   : (x) >= 128 && (x) < 136 ? IPC_REGNUM + (x-128)		 \
   : (x) >= 160 && (x) < 163 ? FPE_REGNUM + (x-160)		 \
   : (x) == 164 ? EXO_REGNUM                                     \
   : (error ("Internal error in SR_REGNUM"), 0))

#ifndef KERNEL_DEBUGGING
/*
 * This macro defines the register numbers (from REGISTER_NAMES) that
 * are effectively unavailable to the user through ptrace().  It allows 
 * us to include the whole register set in REGISTER_NAMES (inorder to 
 * better support remote debugging).  If it is used in 
 * fetch/store_inferior_registers() gdb will not complain about I/O errors 
 * on fetching these registers.  If all registers in REGISTER_NAMES
 * are available, then return false (0).
 */
#define CANNOT_STORE_REGISTER(regno)		\
                  (((regno)>=GR64_REGNUM && (regno)<GR64_REGNUM+32) ||	\
                   ((regno)==VAB_REGNUM) ||	\
		   ((regno)==OPS_REGNUM) ||	\
                   ((regno)>=CFG_REGNUM && (regno)<=TMR_REGNUM)     ||	\
                   ((regno)==MMU_REGNUM) ||	\
		   ((regno)==LRU_REGNUM) ||	\
                   ((regno)>=ALU_REGNUM) ||	\
                   ((regno)==CR_REGNUM)  ||	\
		   ((regno)==EXO_REGNUM))
#define CANNOT_FETCH_REGISTER(regno)	CANNOT_STORE_REGISTER(regno)
#endif /* KERNEL_DEBUGGING */

/*
 * Converts an sdb register number to an internal gdb register number.
 * Currently under gcc, gr96->0...gr128->31...lr0->32...lr127->159, or...
 * 		  	gr64->0...gr95->31, lr0->32...lr127->159.
 */
#define SDB_REG_TO_REGNUM(value) (((value)<32) ? ((value)+RETURN_REGNUM) : \
		 		                 ((value)-32+LR0_REGNUM))

#ifdef KERNEL_DEBUGGING
  /* ublock virtual address as defined in our sys/param.h */
  /* FIXME: Should get this from sys/param.h */
# define UVADDR	((32*0x100000)-8192)    
#endif

/*
 * Are we in sigtramp(), needed in infrun.c.  Specific to ultra3, because
 * we take off the leading '_'.
 */
#if !defined(KERNEL_DEBUGGING)
#ifdef SYM1
# define IN_SIGTRAMP(pc, name) (name && STREQ ("sigtramp", name))
#else
        Need to define IN_SIGTRAMP() for sym2.
#endif
#endif /* !KERNEL_DEBUGGING */

#include "a29k/tm-a29k.h"

/**** The following are definitions that override those in tm-a29k.h ****/

/* This sequence of words is the instructions
   mtsrim cr, 15
   loadm 0, 0, lr2, msp     ; load first 16 words of arguments into registers
   add msp, msp, 16 * 4     ; point to the remaining arguments
  CONST_INSN:
   const gr96,inf
   consth gr96,inf
   calli lr0, gr96
   aseq 0x40,gr1,gr1   ; nop
   asneq 0x50,gr1,gr1  ; breakpoint
   When KERNEL_DEBUGGIN is defined, msp -> gr93, gr96 -> gr64,
                                    7d  -> 5d,    60  -> 40
   */

/* Position of the "const" instruction within CALL_DUMMY in bytes.  */
#undef CALL_DUMMY
#if TARGET_BYTE_ORDER == HOST_BYTE_ORDER
#ifdef KERNEL_DEBUGGING /* gr96 -> gr64 */
#  define CALL_DUMMY {0x0400870f, 0x3600825d, 0x155d5d40, 0x03ff40ff,    \
                    0x02ff40ff, 0xc8008040, 0x70400101, 0x72500101}
#else
#  define CALL_DUMMY {0x0400870f, 0x3600827d, 0x157d7d40, 0x03ff60ff,    \
                    0x02ff60ff, 0xc8008060, 0x70400101, 0x72500101}
#endif /* KERNEL_DEBUGGING */
#else /* Byte order differs.  */
  you lose
#endif /* Byte order differs.  */

#if !defined(KERNEL_DEBUGGING)
# ifdef SYM1
#  undef  DECR_PC_AFTER_BREAK
#  define DECR_PC_AFTER_BREAK 0	/* Sym1 kernel does the decrement */
# else
    ->"ULTRA3 running other than sym1 OS"!;
# endif
#endif /* !KERNEL_DEBUGGING */

