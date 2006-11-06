/* Definitions for StrongARM running FreeBSD using the ELF format
   Copyright (C) 2001, 2004 Free Software Foundation, Inc.
   Contributed by David E. O'Brien <obrien@FreeBSD.org> and BSDi.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */


#undef  SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS \
  { "fbsd_dynamic_linker", FBSD_DYNAMIC_LINKER }, \
  { "subtarget_extra_asm_spec", SUBTARGET_EXTRA_ASM_SPEC }, \
  { "subtarget_asm_float_spec", SUBTARGET_ASM_FLOAT_SPEC }
	

#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC FBSD_CPP_SPEC

#undef	LINK_SPEC
#define LINK_SPEC "							\
  %{p:%nconsider using `-pg' instead of `-p' with gprof(1) }		\
  %{Wl,*:%*}								\
  %{v:-V}								\
  %{assert*} %{R*} %{rpath*} %{defsym*}					\
  %{shared:-Bshareable %{h*} %{soname*}}				\
  %{!shared:								\
    %{!static:								\
      %{rdynamic:-export-dynamic}					\
      %{!dynamic-linker:-dynamic-linker %(fbsd_dynamic_linker) }}	\
    %{static:-Bstatic}}							\
  %{symbolic:-Bsymbolic}						\
  %{mbig-endian:-EB} %{mlittle-endian:-EL}"


/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.  
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

/* arm.h gets this wrong for FreeBSD.  We use the GCC defaults instead.  */

#undef  SIZE_TYPE
#define SIZE_TYPE	"unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE	"int"

/* We use the GCC defaults here.  */
#undef WCHAR_TYPE

#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef  SUBTARGET_CPU_DEFAULT
#define SUBTARGET_CPU_DEFAULT	TARGET_CPU_strongarm

#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (FreeBSD/ARM ELF)");

#ifndef TARGET_ENDIAN_DEFAULT
#define TARGET_ENDIAN_DEFAULT	0
#endif

#undef	TARGET_DEFAULT
#define	TARGET_DEFAULT                  \
  (ARM_FLAG_APCS_32                     \
   | ARM_FLAG_SOFT_FLOAT                \
   | ARM_FLAG_APCS_FRAME                \
   | ARM_FLAG_ATPCS                     \
   | ARM_FLAG_VFP                       \
   | ARM_FLAG_MMU_TRAPS			\
   | TARGET_ENDIAN_DEFAULT)

#undef	TYPE_OPERAND_FMT
#define	TYPE_OPERAND_FMT "%%%s"
        
#undef	SUBTARGET_EXTRA_ASM_SPEC
#define	SUBTARGET_EXTRA_ASM_SPEC        \
  "-matpcs %{fpic|fpie:-k} %{fPIC|fPIE:-k}"
      
  /* Default floating point model is soft-VFP.
   *    FIXME: -mhard-float currently implies FPA.  */
#undef	SUBTARGET_ASM_FLOAT_SPEC
#define	SUBTARGET_ASM_FLOAT_SPEC        \
  "%{mhard-float:-mfpu=fpa} \
  %{msoft-float:-mfpu=softvfp} \
  %{!mhard-float: \
	  %{!msoft-float:-mfpu=softvfp}}"


/* FreeBSD does its profiling differently to the Acorn compiler. We      
   don't need a word following the mcount call; and to skip it
   requires either an assembly stub or use of fomit-frame-pointer when  
   compiling the profiling functions.  Since we break Acorn CC
   compatibility below a little more won't hurt.  */
   
#undef ARM_FUNCTION_PROFILER                                  
#define ARM_FUNCTION_PROFILER(STREAM,LABELNO)		\
{							\
  asm_fprintf (STREAM, "\tmov\t%Rip, %Rlr\n");		\
  asm_fprintf (STREAM, "\tbl\t_mcount%s\n",		\
	       NEED_PLT_RELOC ? "(PLT)" : "");		\
}

/* Emit code to set up a trampoline and synchronize the caches.  */
#undef INITIALIZE_TRAMPOLINE
#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT)			\
do									\
  {									\
    emit_move_insn (gen_rtx (MEM, SImode, plus_constant ((TRAMP), 8)),	\
		    (CXT));						\
    emit_move_insn (gen_rtx (MEM, SImode, plus_constant ((TRAMP), 12)),	\
		    (FNADDR));						\
    emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "__clear_cache"),	\
		       0, VOIDmode, 2, TRAMP, Pmode,			\
		       plus_constant (TRAMP, TRAMPOLINE_SIZE), Pmode);	\
  }									\
while (0)

/* Clear the instruction cache from `BEG' to `END'.  This makes a
   call to the ARM_SYNC_ICACHE architecture specific syscall.  */
#define CLEAR_INSN_CACHE(BEG, END)					\
do									\
  {									\
    extern int sysarch(int number, void *args);				\
    struct								\
      {									\
	unsigned int addr;						\
	int          len;						\
      } s;								\
    s.addr = (unsigned int)(BEG);					\
    s.len = (END) - (BEG);						\
    (void) sysarch (0, &s);						\
  }									\
while (0)
