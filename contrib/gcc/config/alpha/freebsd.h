/* Definitions for DEC AXP Alpha running FreeBSD using the ELF format
   Copyright (C) 2000 Free Software Foundation, Inc.
   Contributed by David O'Brien <obrien@FreeBSD.org>

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* This is used on Alpha platforms that use the ELF format.
   This was taken from the NetBSD configuration, and modified
   for FreeBSD/Alpha by Hidetoshi Shimokawa <simokawa@FreeBSD.ORG> */

/* $FreeBSD$ */


/* Names to predefine in the preprocessor for this target machine.
   XXX FreeBSD, by convention, shouldn't do __alpha, but lots of applications
   expect it because that's what OSF/1 does.  */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES							\
  "-D__alpha__ -D__alpha -Acpu(alpha) -Amachine(alpha)"			\
  FBSD_CPP_PREDEFINES

#undef LINK_SPEC
#define LINK_SPEC "-m elf64alpha					\
  %{p:%e`-p' not supported; use `-pg' and gprof(1)}			\
  %{Wl,*:%*}								\
  %{assert*} %{R*} %{rpath*} %{defsym*}					\
  %{shared:-Bshareable %{h*} %{soname*}}				\
  %{symbolic:-Bsymbolic}						\
  %{!shared:								\
    %{!static:								\
      %{rdynamic:-export-dynamic}					\
      %{!dynamic-linker:-dynamic-linker /usr/libexec/ld-elf.so.1}}	\
    %{static:-Bstatic}}"

/* Provide an ASM_SPEC appropriate for a FreeBSD/Alpha target.  This differs
   from the generic FreeBSD ASM_SPEC in that no special handling of PIC is
   necessary on the Alpha.  */

#undef ASM_SPEC
#define ASM_SPEC " %| %{mcpu=*:-m%*}"

#undef ASM_FINAL_SPEC

/* Provide a STARTFILE_SPEC for FreeBSD that is compatible with the
   non-aout version used on i386.  */

#undef	STARTFILE_SPEC
#define STARTFILE_SPEC							\
  "%{!shared: %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}}	\
     crti.o%s %{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"

/* Provide a ENDFILE_SPEC appropriate for ELF.  Here we tack on the
   magical crtend.o file which provides part of the support for
   getting C++ file-scope static object constructed before entering
   `main', followed by a normal ELF "finalizer" file, `crtn.o'.  */

#undef	ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{!shared:crtend.o%s} %{shared:crtendS.o%s} crtn.o%s"


/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.  
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

/* alpha.h gets this wrong for FreeBSD.  We use the GCC defaults instead.  */
#undef WCHAR_TYPE

/* Handle cross-compilation on 32-bits machines (such as i386) for 64-bits
   machines (Alpha in this case).  */

#if defined(__i386__)
#undef  HOST_BITS_PER_LONG
#define HOST_BITS_PER_LONG	32
#undef  HOST_WIDE_INT
#define HOST_WIDE_INT		long long
#undef  HOST_BITS_PER_WIDE_INT
#define HOST_BITS_PER_WIDE_INT	64
#endif

/* This is the pseudo-op used to generate a 64-bit word of data with a
   specific value in some section.  */

#undef  INT_ASM_OP
#define INT_ASM_OP		".quad"

#undef  TARGET_VERSION
#define TARGET_VERSION	fprintf (stderr, " (FreeBSD/Alpha ELF)");

#define TARGET_AOUT		(0)
#define TARGET_ELF		(1)
#define TARGET_UNDERSCORES	(0)

#undef OBJECT_FORMAT_COFF
#undef EXTENDED_COFF

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT	(MASK_FP | MASK_FPREGS | MASK_GAS)

#undef HAS_INIT_SECTION

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  Under FreeBSD/Alpha, the assembler does
   nothing special with -pg.  */

#undef  FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)				\
	fputs ("\tjsr $28,_mcount\n", (FILE))  /* at */

/* Show that we need a GP when profiling.  */
#undef  TARGET_PROFILING_NEEDS_GP
#define TARGET_PROFILING_NEEDS_GP

/* We always use gas here, so we don't worry about ECOFF assembler problems.  */
#undef  TARGET_GAS
#define TARGET_GAS	1


/************************[  Assembler stuff  ]********************************/

/* This is how to begin an assembly language file.
   ELF also needs a .version.  */

#undef  ASM_FILE_START
#define ASM_FILE_START(FILE)						\
  {									\
    alpha_write_verstamp (FILE);					\
    output_file_directive ((FILE), main_input_filename);		\
    fprintf ((FILE), "\t.version\t\"01.01\"\n");			\
    fprintf ((FILE), "\t.set noat\n");					\
  }

extern void output_file_directive ();
extern void alpha_output_lineno ();

#undef  ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(STREAM, LINE)				\
  alpha_output_lineno ((STREAM), (LINE))

/* Switch into a generic section.
   This is currently only used to support section attributes.

   We make the section read-only and executable for a function decl,
   read-only for a const data decl, and writable for a non-const data decl.  */
#undef  ASM_OUTPUT_SECTION_NAME
#define ASM_OUTPUT_SECTION_NAME(FILE, DECL, NAME, RELOC)		\
  fprintf ((FILE), ".section\t%s,\"%s\",@progbits\n", (NAME),		\
	   (DECL) && TREE_CODE (DECL) == FUNCTION_DECL ? "ax" :		\
	   (DECL) && DECL_READONLY_SECTION ((DECL), (RELOC)) ? "a" : "aw")

/* A C statement or statements to switch to the appropriate
   section for output of DECL.  DECL is either a `VAR_DECL' node
   or a constant of some sort.  RELOC indicates whether forming
   the initial value of DECL requires link-time relocations.  */

#undef  SELECT_SECTION
#define SELECT_SECTION(DECL, RELOC)					\
  {									\
    if (TREE_CODE (DECL) == STRING_CST)					\
      {									\
	if (! flag_writable_strings)					\
	  const_section ();						\
	else								\
	  data_section ();						\
      }									\
    else if (TREE_CODE (DECL) == VAR_DECL)				\
      {									\
	if ((flag_pic && (RELOC))					\
	    || !TREE_READONLY (DECL) || TREE_SIDE_EFFECTS (DECL)	\
	    || !DECL_INITIAL (DECL)					\
	    || (DECL_INITIAL (DECL) != error_mark_node			\
	    && !TREE_CONSTANT (DECL_INITIAL (DECL))))			\
	  data_section ();						\
        else								\
	  const_section ();						\
      }									\
    else								\
      const_section ();							\
  }

/* This is how we tell the assembler that two symbols have the same value.  */

#undef  ASM_OUTPUT_DEF
#define ASM_OUTPUT_DEF(FILE,NAME1,NAME2)				\
  do {									\
    assemble_name((FILE), (NAME1));					\
    fputs(" = ", (FILE));						\
    assemble_name((FILE), (NAME2));					\
    fputc('\n', (FILE));						\
  } while (0)


/************************[  Debugger stuff  ]*********************************/

/* This is the char to use for continuation (in case we need to turn
   continuation back on).  */

#undef  DBX_CONTIN_CHAR
#define DBX_CONTIN_CHAR	'?'
