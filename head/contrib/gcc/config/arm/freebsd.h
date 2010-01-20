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
   the Free Software Foundation, 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC FBSD_CPP_SPEC

#undef  SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS \
  { "subtarget_extra_asm_spec",	SUBTARGET_EXTRA_ASM_SPEC }, \
  { "subtarget_asm_float_spec", SUBTARGET_ASM_FLOAT_SPEC }, \
  { "fbsd_dynamic_linker", FBSD_DYNAMIC_LINKER }

#undef SUBTARGET_EXTRA_ASM_SPEC
#define SUBTARGET_EXTRA_ASM_SPEC	\
  "-matpcs %{fpic|fpie:-k} %{fPIC|fPIE:-k}"

/* Default to full FPA if -mhard-float is specified. */
#undef SUBTARGET_ASM_FLOAT_SPEC
#define SUBTARGET_ASM_FLOAT_SPEC		\
  "%{mhard-float:-mfpu=fpa}			\
   %{mfloat-abi=hard:{!mfpu=*:-mfpu=fpa}}	\
   %{!mhard-float: %{msoft-float:-mfpu=softvfp;:-mfpu=softvfp}}"

#undef	LINK_SPEC
#define LINK_SPEC "							\
  %{p:%nconsider using `-pg' instead of `-p' with gprof(1) }		\
  %{v:-V}								\
  %{assert*} %{R*} %{rpath*} %{defsym*}					\
  %{shared:-Bshareable %{h*} %{soname*}}				\
  %{!shared:								\
    %{!static:								\
      %{rdynamic:-export-dynamic}					\
      %{!dynamic-linker:-dynamic-linker %(fbsd_dynamic_linker) }}	\
    %{static:-Bstatic}}							\
  %{symbolic:-Bsymbolic}						\
  -X %{mbig-endian:-EB} %{mlittle-endian:-EL}"

/************************[  Target stuff  ]***********************************/

#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (FreeBSD/StrongARM ELF)");

#ifndef TARGET_ENDIAN_DEFAULT
#define TARGET_ENDIAN_DEFAULT 0
#endif

/* Default it to use ATPCS with soft-VFP.  */
#undef TARGET_DEFAULT
#define TARGET_DEFAULT			\
  (MASK_APCS_FRAME			\
   | TARGET_ENDIAN_DEFAULT)

#undef ARM_DEFAULT_ABI
#define ARM_DEFAULT_ABI ARM_ABI_ATPCS

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

#undef  SUBTARGET_CPU_DEFAULT
#define SUBTARGET_CPU_DEFAULT	TARGET_CPU_strongarm

/* FreeBSD does its profiling differently to the Acorn compiler. We
   don't need a word following the mcount call; and to skip it
   requires either an assembly stub or use of fomit-frame-pointer when
   compiling the profiling functions.  Since we break Acorn CC
   compatibility below a little more won't hurt.  */

#undef ARM_FUNCTION_PROFILER
#define ARM_FUNCTION_PROFILER(STREAM,LABELNO)		\
{							\
  asm_fprintf (STREAM, "\tmov\t%Rip, %Rlr\n");		\
  asm_fprintf (STREAM, "\tbl\t__mcount%s\n",		\
	       (TARGET_ARM && NEED_PLT_RELOC)		\
	       ? "(PLT)" : "");				\
}

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

#undef FPUTYPE_DEFAULT
#define FPUTYPE_DEFAULT FPUTYPE_VFP
