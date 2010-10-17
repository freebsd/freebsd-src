/* PowerPC traceback table support for BFD.
   Copyright 1993, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

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

/* Originally written by Ira Ruben, 06/28/93 */

/*  This is a compiler independent representation of the AIX Version 3 traceback table (in
    sys/debug.h), which occurs, usually, one per procedure (routine). The table is marked by
    a multiple of 4 32-bit word of zeroes in the instruction space. The traceback table is
    also referred to as "procedure-end table".
 
    The AIX traceback table representation on which this header is based is defined as a
    series of bit field struct specifications. Bit fields are compiler dependent! Thus,
    the definitions presented here follow the original header and the existing documentation
    (such as it is), but define the fields as BIT MASKS and other macros. The mask names,
    however, where chosen as the original field names to give some compatibility with the 
    original header and to agree with the documentation.  */

#ifndef __TRACEBACK__
#define __TRACEBACK__

#define TB_C 0U			/* C */
#define TB_FORTRAN 1U		/* FORTRAN */
#define TB_PASCAL 2U		/* Pascal */
#define TB_ADA 3U		/* ADA */
#define TB_PL1 4U		/* PL1 */
#define TB_BASIC 5U		/* Basic */
#define TB_LISP 6U		/* Lisp */
#define TB_COBOL 7U		/* eCobol */
#define TB_MODULA2 8U		/* Modula2 */
#define TB_CPLUSPLUS 9U		/* C++ */
#define TB_RPG 10U		/* RPG */
#define TB_PL8 11U		/* PL8 */
#define TB_ASM 12U		/* Asm */
 
/* flags 1 */

#define TB_GLOBALLINK 0x80U	/* Routine is Global Linkage.  */
#define TB_is_eprol 0x40U	/* Out-of-line prolog or epilog routine.  */
#define TB_HAS_TBOFF 0x20U	/* tb_offset set (extension field).  */
#define TB_INT_PROC 0x10U	/* Internal leaf routine.  */
#define TB_HAS_CTL 0x08U	/* Has controlled automatic storage.  */
#define TB_TOCLESS 0X04U	/* Routine has no TOC.  */
#define TB_FP_PRESENT 0x02U	/* Routine has floating point ops.  */ 
#define TB_LOG_ABORT 0x01U	/* fp_present && log/abort compiler opt.  */
 
/* flags 2 */

#define TB_INT_HNDL 0x80U	/* Routine is an interrupt handler.  */
#define TB_NAME_PRESENT 0x40U	/* Name_len/name set (extension field).  */
#define TB_USES_ALLOCA 0x20U	/* Uses alloca() to allocate storage.  */
#define TB_CL_DIS_inv 0x1CU	/* On-condition directives (see below).  */
#define TB_SAVES_CR 0x02U	/* Routine saves the CR.  */
#define TB_SAVES_LR 0x01U	/* Routine saves the LR.  */
  
/* cl_dis_inv "on condition" settings: */
 
#define TB_CL_DIS_INV(x) (((x) & cl_dis_inv) >> 2U)

#define TB_WALK_ONCOND 0U	/* Walk stack without restoring state.  */
#define TB_DISCARD_ONCOND 1U	/* Walk stack and discard.  */
#define TB_INVOKE_ONCOND 2U	/* Invoke a specific system routine.  */
 
/* flags 3 */

#define TB_STORES_BC 0x80U	/* Routine saves frame ptr of caller.  */
#define TB_SPARE2 0X40U		/* Spare bit.  */
#define TB_FPR_SAVED 0x3fU	/* Number of FPRs saved (max of 32).  */
				/* (Last reg saved is ALWAYS fpr31).  */

#define TB_NUM_FPR_SAVED(x) ((x) & fpr_saved)
 
/* flags 4 */

#define TB_HAS_VEC_INFO 0x80U	/* Routine uses vectors.  */
#define TB_SPARE3 0X40U		/* Spare bit.  */
#define TB_GPR_SAVED 0x3fU	/* Number of GPRs saved (max of 32).  */
				/* (Last reg saved is ALWAYS gpr31).  */

#define TB_NUM_GPR_SAVED(x) ((x) & gpr_saved)
 
/* flags 5 */

#define TB_FLOATPARAMS 0xfeU	/* Number of floating point parameters.  */
#define TB_PARAMSONSTK 0X01U	/* All parameters are on the stack.  */
 
#define TB_NUM_FLOATPARAMS(X) (((x) & floatparams) >> 1U)

/* traceback_table (fixed portion).  */

struct traceback_table
{
  /* Traceback table layout (fixed portion):  */

  unsigned char version;	/* Traceback format version.  */
  unsigned char lang;		/* Language indicators:  */
  unsigned char flags1;		/* Flag bits #1:  */
  unsigned char flags2;		/* Flag bits #2:  */
  unsigned char flags3;		/* Flag bits #3:  */
  unsigned char flags4;		/* Flag bits #4:  */
  unsigned char fixedparams;	/* Number of fixed point parameters.  */
  unsigned char flags5;		/* Flag bits #5:  */
};

/* traceback_table (optional) extensions.  */

/* Optional portions exist independently in the order presented below,
   not as a structure or a union. Whether or not portions exist is
   determinable from bit-fields within the fixed portion above.  */

/* The following is present only if fixedparams or floatparams are non
   zero and it immediately follows the fixed portion of the traceback
   table...  */

/* Order and type encoding of parameters:  */
struct traceback_table_fixedparams
{
  unsigned long paraminfo;		
};

/* Left-justified bit-encoding as follows:  */
#define FIXED_PARAM 0		/* '0'  ==> fixed param (1 gpr or word).  */
#define SPFP_PARAM 2		/* '10' ==> single-precision float param.  */
#define DPFP_PARAM 3		/* '11' ==> double-precision float param.  */

#define PARAM_ENCODING(x, bit) /* Yields xxx_PARAM as a function of "bit".  */		\
 ((((x)&(1UL<<(31UL-(bit++))))==0UL) /* Values 0:31 (left-to-right). "bit" is */	\
 ? FIXED_PARAM /* an L-value that's left incremented to */				\
 : ((((x)&(1UL<<(31UL-(bit++))))==0)/* the next bit position for the next */		\
 ? SPFP_PARAM /* parameter.  This will be 1 or 2 bit */					\
 : DPFP_PARAM)) /* positions later.  */

/* The following is present only if has_tboff (in flags1) in fixed part is present...  */

/* Offset from start of code to TracebackTbl.  */
struct traceback_table_tboff
{
  unsigned long tb_offset;
};

/* The following is present only if int_hndl (in flags2) in fixed part is present ...  */

/* What interrupts are handled by the routine.  */
struct traceback_table_interrupts
{
  long hand_mask;
};

/* The following are present only if has_ctl (in flags1) in fixed part is present...  */

/* Controlled automatic storage info:  */
struct traceback_table_anchors
{
  unsigned long ctl_info;	/* Number of controlled automatic anchors.  */ 
  long ctl_info_disp[1];	/* Array of stack displacements where each.  */
};				/* Anchor is located (array STARTS here).  */

/* The following are present only if name_present (in flags2) in fixed
   part is present...  */

/* Routine name:  */
struct traceback_table_routine
{
  unsigned short name_len;	/* Length of name that follows.  */
  char name[1];			/* Name starts here (NOT null terminated).  */
};

/* The following are present only if uses_alloca (in flags2) in fixed
   part is present...  */

/* Register auto storage when alloca() is used.  */
struct traceback_table_alloca
{
  char alloca_reg;
};

/* The following are present only if has_vec_info (in flags4) in fixed
   part is present...  */

/* Vector info:  */
struct traceback_table_vector
{
  unsigned char vec_flags1;	/* Vec info bits #1:  */

#define TB_VR_SAVED 0xFCU	/* Number of saved vector registers.  */
#define TB_SAVES_VRSAVE 0x02U	/* Saves VRsave.  */
#define TB_HAS_VARARGS 0x01U	/* Routine has a variable argument list.  */

#define TB_NUM_VR_SAVED(x) (((x) & TB_VR_SAVED) >> 2U)
 
  unsigned char vec_flags2;	/* Vec info bits #2:  */

#define TB_VECTORPARAMS 0xfeU	/* Number of vector parameters.  */
#define TB_VEC_PRESENT 0x01U	/* Routine uses at least one vec instr.  */
 
#define VECPARAMS(x) (((x) & TB_VECTORPARAMS) >> 1U)
};

#endif
