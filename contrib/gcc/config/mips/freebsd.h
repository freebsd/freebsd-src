/* Definitions for MIPS varients running FreeBSD with ELF format
   Copyright (C) 2008 Free Software Foundation, Inc.
   Continued by David O'Brien <obrien@freebsd.org>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* $FreeBSD$ */

/* This defines which switch letters take arguments.  -G is a MIPS
   special.  */

#undef  SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR)		\
  (FBSD_SWITCH_TAKES_ARG (CHAR)		\
   || (CHAR) == 'R'			\
   || (CHAR) == 'G')

#undef  SUBTARGET_EXTRA_SPECS	/* mips.h bogusly defines it.  */
#define SUBTARGET_EXTRA_SPECS \
  { "fbsd_dynamic_linker",	FBSD_DYNAMIC_LINKER}, \
  { "fbsd_link_spec",		FBSD_LINK_SPEC }

/* config/mips/mips.h defines CC1_SPEC,
   but gives us an "out" with SUBTARGET_CC1_SPEC.  */
#undef  SUBTARGET_CC1_SPEC
#define SUBTARGET_CC1_SPEC "%{profile:-p}"

/* Provide a LINK_SPEC appropriate for FreeBSD.  Here we provide support
   for the special GCC options -static and -shared, which allow us to
   link things in one of these three modes by applying the appropriate
   combinations of options at link-time. We like to support here for
   as many of the other GNU linker options as possible. But I don't
   have the time to search for those flags. I am sure how to add
   support for -soname shared_object_name. H.J.

   When the -shared link option is used a final link is not being
   done.  */

#define FBSD_LINK_SPEC "\
    %{p:%nconsider using `-pg' instead of `-p' with gprof(1) } \
    %{v:-V} \
    %{assert*} %{R*} %{rpath*} %{defsym*} \
    %{shared:-Bshareable %{h*} %{soname*}} \
    %{!shared: \
      %{!static: \
	%{rdynamic: -export-dynamic} \
	%{!dynamic-linker:-dynamic-linker %(fbsd_dynamic_linker) }} \
      %{static:-Bstatic}} \
    %{symbolic:-Bsymbolic} "

#undef	LINK_SPEC
#define LINK_SPEC "\
    %{G*} %{mips1} %{mips2} %{mips3} %{mips4} %{mips32} %{mips32r2} %{mips64} \
    %{bestGnum} %{call_shared} %{no_archive} %{exact_version} \
    %(fbsd_link_spec) "
#if 0
    %(endian_spec)
#endif
    
/* Reset our STARTFILE_SPEC which was properly set in config/freebsd.h
   but trashed by config/mips/elf.h.  */
#undef  STARTFILE_SPEC
#define STARTFILE_SPEC	FBSD_STARTFILE_SPEC

/* Provide an ENDFILE_SPEC appropriate for FreeBSD/i386.  */
#undef  ENDFILE_SPEC
#define ENDFILE_SPEC	FBSD_ENDFILE_SPEC

/* Reset our LIB_SPEC which was properly set in config/freebsd.h
   but trashed by config/mips/elf.h.  */
#undef  LIB_SPEC
#define LIB_SPEC	FBSD_LIB_SPEC

/* config/mips/mips.h defines CPP_SPEC, and it expects SUBTARGET_CPP_SPEC.  */
#undef  SUBTARGET_CPP_SPEC
#define SUBTARGET_CPP_SPEC FBSD_CPP_SPEC


/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

#undef TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_ABICALLS | MASK_SOFT_FLOAT)

#if TARGET_ENDIAN_DEFAULT != 0
#define TARGET_VERSION	fprintf (stderr, " (FreeBSD/mips)");
#else
#define TARGET_VERSION	fprintf (stderr, " (FreeBSD/mipsel)");
#endif

/* The generic MIPS TARGET_CPU_CPP_BUILTINS are incorrect for FreeBSD.
   Specifically, they define too many namespace-invasive macros.  Override
   them here.  Note this is structured for easy comparison to the version
   in mips.h.  */

#undef  TARGET_CPU_CPP_BUILTINS
#define TARGET_CPU_CPP_BUILTINS()				\
  do								\
    {								\
      builtin_assert ("machine=mips");				\
      builtin_assert ("cpu=mips");				\
      builtin_define ("__mips__");				\
								\
      if (TARGET_64BIT)						\
	builtin_define ("__mips64__");				\
								\
      if (TARGET_FLOAT64)					\
	builtin_define ("__mips_fpr=64");			\
      else							\
	builtin_define ("__mips_fpr=32");			\
								\
      if (TARGET_MIPS16)					\
	builtin_define ("__mips16");				\
								\
      MIPS_CPP_SET_PROCESSOR ("_MIPS_ARCH", mips_arch_info);	\
      MIPS_CPP_SET_PROCESSOR ("_MIPS_TUNE", mips_tune_info);	\
								\
      if (ISA_MIPS3)						\
	builtin_define ("__mips=3");				\
      else if (ISA_MIPS4)					\
	builtin_define ("__mips=4");				\
      else if (ISA_MIPS32)					\
	{							\
	  builtin_define ("__mips=32");				\
	  builtin_define ("__mips_isa_rev=1");			\
	}							\
      else if (ISA_MIPS32R2)					\
	{							\
	  builtin_define ("__mips=32");				\
	  builtin_define ("__mips_isa_rev=2");			\
	}							\
      else if (ISA_MIPS64)					\
	{							\
	  builtin_define ("__mips=64");				\
	  builtin_define ("__mips_isa_rev=1");			\
	}							\
								\
      if (TARGET_HARD_FLOAT)					\
	builtin_define ("__mips_hard_float");			\
      else if (TARGET_SOFT_FLOAT)				\
	builtin_define ("__mips_soft_float");			\
								\
      if (TARGET_SINGLE_FLOAT)					\
	builtin_define ("__mips_single_float");			\
								\
      if (TARGET_BIG_ENDIAN)					\
	builtin_define ("__MIPSEB__");				\
      else							\
	builtin_define ("__MIPSEL__");				\
								\
      /* No language dialect defines.  */			\
								\
      if (mips_abi == ABI_EABI)					\
	builtin_define ("__mips_eabi");				\
      else if (mips_abi == ABI_N32)				\
	builtin_define ("__mips_n32");				\
      else if (mips_abi == ABI_64)				\
	builtin_define ("__mips_n64");				\
      else if (mips_abi == ABI_O64)				\
	builtin_define ("__mips_o64");				\
      else							\
	builtin_define ("__mips_o32");				\
								\
      if (TARGET_ABICALLS)					\
	builtin_define ("__ABICALLS__");			\
    }								\
  while (0)

/* Default to the mips32 ISA */
#undef  DRIVER_SELF_SPECS
#define DRIVER_SELF_SPECS \
  "%{!march=*: -march=mips32}"
#if 0
  "%{!EB:%{!EL:%(endian_spec)}}", 
#endif

#if 0
/* Don't default to pcc-struct-return, we want to retain compatibility with
   older gcc versions AND pcc-struct-return is nonreentrant.
   (even though the SVR4 ABI for the i386 says that records and unions are
   returned in memory).  */

#undef  DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0
#endif


/************************[  Assembler stuff  ]********************************/

#undef  SUBTARGET_ASM_SPEC
#define SUBTARGET_ASM_SPEC \
  "%{!mno-abicalls: %{!fno-PIC:%{!fno-pic:-KPIC}}}"

/* -G is incompatible with -KPIC which is the default, so only allow objects
   in the small data section if the user explicitly asks for it.  */

#undef  MIPS_DEFAULT_GVALUE
#define MIPS_DEFAULT_GVALUE 0

/* If defined, a C expression whose value is a string containing the
   assembler operation to identify the following data as
   uninitialized global data.  If not defined, and neither
   `ASM_OUTPUT_BSS' nor `ASM_OUTPUT_ALIGNED_BSS' are defined,
   uninitialized global data will be output in the data section if
   `-fno-common' is passed, otherwise `ASM_OUTPUT_COMMON' will be
   used.  */
#undef  BSS_SECTION_ASM_OP
#define BSS_SECTION_ASM_OP "\t.section\t.bss"

/* Like `ASM_OUTPUT_BSS' except takes the required alignment as a
   separate, explicit argument.  If you define this macro, it is used
   in place of `ASM_OUTPUT_BSS', and gives you more flexibility in
   handling the required alignment of the variable.  The alignment is
   specified as the number of bits.

   Try to use function `asm_output_aligned_bss' defined in file
   `varasm.c' when defining this macro.  */
#undef  ASM_OUTPUT_ALIGNED_BSS
#define ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN) \
  asm_output_aligned_bss (FILE, DECL, NAME, SIZE, ALIGN)

/* Standard AT&T UNIX 'as' local label spelling.  */
#undef  LOCAL_LABEL_PREFIX
#define LOCAL_LABEL_PREFIX "."


/************************[  Debugger stuff  ]*********************************/

