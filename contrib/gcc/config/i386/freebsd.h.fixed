/* Definitions for Intel 386 running FreeBSD with either a.out or ELF format
   Copyright (C) 1994, 1995 Free Software Foundation, Inc.
   Contributed by Eric Youngdale.
   Modified for stabs-in-ELF by H.J. Lu.
   Adapted from Linux version by John Polstra.
   Added support for generating "old a.out gas" on the fly by Peter Wemm.

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
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* A lie, I guess, but the general idea behind FreeBSD/ELF is that we are
   supposed to be outputting something that will assemble under SVr4.
   This gets us pretty close.  */
#include <i386/i386.h>	/* Base i386 target machine definitions */
#include <i386/att.h>	/* Use the i386 AT&T assembler syntax */
#include <linux.h>	/* some common stuff */

/* Don't assume anything about the header files. */
#define NO_IMPLICIT_EXTERN_C

/* This defines which switch letters take arguments.  On svr4, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker).  We have a slightly different mix.  We
   have -R (alias --rpath), no -z, --soname (-h), --assert etc. */

#undef SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) \
  (   (CHAR) == 'D' \
   || (CHAR) == 'U' \
   || (CHAR) == 'o' \
   || (CHAR) == 'e' \
   || (CHAR) == 'T' \
   || (CHAR) == 'u' \
   || (CHAR) == 'I' \
   || (CHAR) == 'm' \
   || (CHAR) == 'L' \
   || (CHAR) == 'A' \
   || (CHAR) == 'h' \
   || (CHAR) == 'z' /* ignored by ld */ \
   || (CHAR) == 'R')

#undef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR)					\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)					\
   || !strcmp (STR, "rpath") || !strcmp (STR, "rpath-link")		\
   || !strcmp (STR, "soname") || !strcmp (STR, "defsym") 		\
   || !strcmp (STR, "assert") || !strcmp (STR, "dynamic-linker"))

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (i386 FreeBSD)");

#define MASK_PROFILER_EPILOGUE	010000000000
#define MASK_AOUT		004000000000	/* a.out not elf */
#define MASK_UNDERSCORES	002000000000	/* use leading _ */

#define TARGET_PROFILER_EPILOGUE	(target_flags & MASK_PROFILER_EPILOGUE)
#define TARGET_AOUT			(target_flags & MASK_AOUT)
#define TARGET_ELF			((target_flags & MASK_AOUT) == 0)
#define TARGET_UNDERSCORES		((target_flags & MASK_UNDERSCORES) != 0)

#undef	SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES					\
     { "profiler-epilogue",	 MASK_PROFILER_EPILOGUE},	\
     { "no-profiler-epilogue",	-MASK_PROFILER_EPILOGUE},	\
     { "aout",			 MASK_AOUT},			\
     { "no-aout",		-MASK_AOUT},			\
     { "underscores",		 MASK_UNDERSCORES},		\
     { "no-underscores",	-MASK_UNDERSCORES},

/* The svr4 ABI for the i386 says that records and unions are returned
   in memory.  */
/* On FreeBSD, we do not. */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Prefix for internally generated assembler labels.  If we aren't using 
   underscores, we are using prefix `.'s to identify labels that should  
   be ignored, as in `i386/gas.h' --karl@cs.umb.edu  */                 
#undef  LPREFIX
#define LPREFIX ((TARGET_UNDERSCORES) ? "L" : ".L")

/* Override the default comment-starter of "/".  */
#undef ASM_COMMENT_START
#define ASM_COMMENT_START "#"

#undef COMMENT_BEGIN
#define COMMENT_BEGIN "#"

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n" 

/* Output at beginning of assembler file.  */
/* The .file command should always begin the output.  */

#undef ASM_FILE_START
#define ASM_FILE_START(FILE)						\
  do {									\
        output_file_directive (FILE, main_input_filename);		\
	if (TARGET_ELF)							\
          fprintf (FILE, "\t.version\t\"01.01\"\n");			\
  } while (0)

/* Identify the front-end which produced this file.  To keep symbol
   space down, and not confuse kdb, only do this if the language is
   not C. (svr4.h defines ASM_IDENTIFY_GCC but neglects this) */
#undef ASM_IDENTIFY_LANGUAGE
#define ASM_IDENTIFY_LANGUAGE(STREAM)					\
{									\
  if (strcmp (lang_identify (), "c") != 0)				\
    output_lang_identify (STREAM);					\
}

/* This is how to store into the string BUF
   the symbol_ref name of an internal numbered label where      
   PREFIX is the class of label and NUM is the number within the class.  
   This is suitable for output with `assemble_name'.  */
#undef	ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(BUF,PREFIX,NUMBER)			\
    sprintf ((BUF), "*%s%s%d", (TARGET_UNDERSCORES) ? "" : ".",		\
	     (PREFIX), (NUMBER))

/* This is how to output an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.  */
#undef	ASM_OUTPUT_INTERNAL_LABEL
#define	ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)			\
  fprintf (FILE, "%s%s%d:\n", (TARGET_UNDERSCORES) ? "" : ".",		\
	   PREFIX, NUM)

/* This is how to output a reference to a user-level label named NAME.  */
#undef  ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)					\
  fprintf (FILE, "%s%s", (TARGET_UNDERSCORES) ? "_" : "", NAME)


/* This is how to output an element of a case-vector that is relative.
   This is only used for PIC code.  See comments by the `casesi' insn in
   i386.md for an explanation of the expression this outputs. */
#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL) \
  fprintf (FILE, "\t.long _GLOBAL_OFFSET_TABLE_+[.-%s%d]\n", LPREFIX, VALUE)

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)      \
  if ((LOG)!=0) fprintf ((FILE), "\t.p2align %d\n", (LOG))

/* Align labels, etc. at 4-byte boundaries.
   For the 486, align to 16-byte boundary for sake of cache.  */
#undef ASM_OUTPUT_ALIGN_CODE
#define ASM_OUTPUT_ALIGN_CODE(FILE) \
  fprintf ((FILE), "\t.p2align %d,0x90\n", i386_align_jumps)

/* Align start of loop at 4-byte boundary.  */
#undef ASM_OUTPUT_LOOP_ALIGN
#define ASM_OUTPUT_LOOP_ALIGN(FILE) \
  fprintf ((FILE), "\t.p2align %d,0x90\n", i386_align_loops)


/* conditionalize the use of ".section rodata" on elf mode - otherwise .text */
#undef USE_CONST_SECTION
#define USE_CONST_SECTION	TARGET_ELF

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#undef ASM_OUTPUT_CONSTRUCTOR
#define ASM_OUTPUT_CONSTRUCTOR(FILE,NAME)				\
  do {									\
    if (TARGET_ELF) {							\
      ctors_section ();							\
      fprintf (FILE, "\t%s\t ", INT_ASM_OP);				\
      assemble_name (FILE, NAME);					\
      fprintf (FILE, "\n");						\
    } else {								\
      fprintf (asm_out_file, "%s \"%s__CTOR_LIST__\",22,0,0,", ASM_STABS_OP, \
	       (TARGET_UNDERSCORES) ? "_" : "");			\
      assemble_name (asm_out_file, name);				\
      fputc ('\n', asm_out_file);					\
    }									\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#undef ASM_OUTPUT_DESTRUCTOR
#define ASM_OUTPUT_DESTRUCTOR(FILE,NAME)				\
  do {									\
    if (TARGET_ELF) {							\
      dtors_section ();							\
      fprintf (FILE, "\t%s\t ", INT_ASM_OP);				\
      assemble_name (FILE, NAME);					\
      fprintf (FILE, "\n");						\
    } else {								\
      fprintf (asm_out_file, "%s \"%s__DTOR_LIST__\",22,0,0,", ASM_STABS_OP, \
	       (TARGET_UNDERSCORES) ? "_" : "");			\
      assemble_name (asm_out_file, name);				\
      fputc ('\n', asm_out_file);					\
    }									\
  } while (0)

/* This says how to output assembler code to declare an
   uninitialized internal linkage data object.  Under SVR4,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN)		\
do {									\
  if (TARGET_ELF) {							\
    fprintf ((FILE), "\t%s\t", LOCAL_ASM_OP);				\
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), "\n");						\
    ASM_OUTPUT_ALIGNED_COMMON (FILE, NAME, SIZE, ALIGN);		\
  } else {								\
    int rounded = (SIZE);						\
    if (rounded == 0) rounded = 1;					\
    rounded += (BIGGEST_ALIGNMENT / BITS_PER_UNIT) - 1;			\
    rounded = (rounded / (BIGGEST_ALIGNMENT / BITS_PER_UNIT)		\
			   * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));	\
    fputs (".lcomm ", (FILE));						\
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), ",%u\n", (rounded));				\
  }									\
} while (0)

#undef ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGN)		\
do {									\
  if (TARGET_ELF) {							\
    fprintf ((FILE), "\t%s\t", COMMON_ASM_OP);				\
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), ",%u,%u\n", (SIZE), (ALIGN) / BITS_PER_UNIT);	\
  } else {								\
    int rounded = (SIZE);						\
    if (rounded == 0) rounded = 1;					\
    rounded += (BIGGEST_ALIGNMENT / BITS_PER_UNIT) - 1;			\
    rounded = (rounded / (BIGGEST_ALIGNMENT / BITS_PER_UNIT)		\
			   * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));	\
    fputs (".comm ", (FILE));						\
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), ",%u\n", (rounded));				\
  }									\
} while (0)

/* Turn off svr4.h version, it chokes the old gas.  The old layout
   works fine under new gas anyway. */
#undef ASM_OUTPUT_ASCII

/* How to output some space */
#undef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE) 					\
do {									\
  if (TARGET_ELF) {							\
    fprintf (FILE, "\t%s\t%u\n", SKIP_ASM_OP, (SIZE));			\
  } else {								\
    fprintf (FILE, "\t.space %u\n", (SIZE));				\
  }									\
} while (0)

#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(file, line)				\
do {									\
  static int sym_lineno = 1;						\
  if (TARGET_ELF) {							\
    fprintf (file, ".stabn 68,0,%d,.LM%d-", line, sym_lineno);		\
    assemble_name (file, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));\
    fprintf (file, "\n.LM%d:\n", sym_lineno);				\
    sym_lineno += 1;							\
  } else {								\
    fprintf (file, "\t%s %d,0,%d\n", ASM_STABD_OP, N_SLINE, lineno);	\
  }									\
} while (0)

/* in elf, the function stabs come first, before the relative offsets */
#undef DBX_FUNCTION_FIRST
#define DBX_CHECK_FUNCTION_FIRST TARGET_ELF

/* tag end of file in elf mode */
#undef DBX_OUTPUT_MAIN_SOURCE_FILE_END
#define DBX_OUTPUT_MAIN_SOURCE_FILE_END(FILE, FILENAME)			\
do {									\
  if (TARGET_ELF) {							\
    fprintf (FILE, "\t.text\n\t.stabs \"\",%d,0,0,.Letext\n.Letext:\n", N_SO); \
  }									\
} while (0)

/* stabs-in-elf has offsets relative to function beginning */
#undef DBX_OUTPUT_LBRAC
#define DBX_OUTPUT_LBRAC(file,name)					\
do {									\
  fprintf (asmfile, "%s %d,0,0,", ASM_STABN_OP, N_LBRAC);		\
  assemble_name (asmfile, buf);						\
  if (TARGET_ELF) {							\
    fputc ('-', asmfile);						\
    assemble_name (asmfile, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0)); \
  }									\
  fprintf (asmfile, "\n");						\
} while (0)

#undef DBX_OUTPUT_RBRAC
#define DBX_OUTPUT_RBRAC(file,name)					\
do {									\
  fprintf (asmfile, "%s %d,0,0,", ASM_STABN_OP, N_RBRAC);		\
  assemble_name (asmfile, buf);						\
  if (TARGET_ELF) {							\
    fputc ('-', asmfile);						\
    assemble_name (asmfile, XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0)); \
  }									\
  fprintf (asmfile, "\n");						\
} while (0)


/* Define macro used to output shift-double opcodes when the shift
   count is in %cl.  Some assemblers require %cl as an argument;
   some don't.

   *OLD* GAS requires the %cl argument, so override i386/unix.h. */
      
#undef AS3_SHIFT_DOUBLE
#define AS3_SHIFT_DOUBLE(a,b,c,d) AS3 (a,b,c,d)

/* Indicate that jump tables go in the text section.  This is
   necessary when compiling PIC code.  */
#define JUMP_TABLES_IN_TEXT_SECTION

/* override the exception table positioning */
#define EXCEPTION_SECTION_FUNCTION \
do {									\
  if (TARGET_ELF) {							\
    named_section (NULL_TREE, ".gcc_except_table");			\
  } else {								\
    if (flag_pic)							\
      data_section ();							\
    else								\
      readonly_data_section ();						\
  }									\
} while (0);

/* supply our own hook for calling __main() from main() */
#define GEN_CALL__MAIN \
  do {									\
    if (!(TARGET_ELF))							\
      emit_library_call (gen_rtx (SYMBOL_REF, Pmode, NAME__MAIN), 0,	\
			 VOIDmode, 0);					\
  } while (0)

/* Use dollar signs in special g++ assembler names.  */
#undef NO_DOLLAR_IN_LABEL

/* Map i386 registers to the numbers dwarf expects.  Of course this is different
   from what stabs expects.  */

#undef DWARF_DBX_REGISTER_NUMBER
#define DWARF_DBX_REGISTER_NUMBER(n) \
((n) == 0 ? 0 \
 : (n) == 1 ? 2 \
 : (n) == 2 ? 1 \
 : (n) == 3 ? 3 \
 : (n) == 4 ? 6 \
 : (n) == 5 ? 7 \
 : (n) == 6 ? 5 \
 : (n) == 7 ? 4 \
 : ((n) >= FIRST_STACK_REG && (n) <= LAST_STACK_REG) ? (n)+3 \
 : (-1))

/* Now what stabs expects in the register.  */
#define STABS_DBX_REGISTER_NUMBER(n) \
((n) == 0 ? 0 : \
 (n) == 1 ? 2 : \
 (n) == 2 ? 1 : \
 (n) == 3 ? 3 : \
 (n) == 4 ? 6 : \
 (n) == 5 ? 7 : \
 (n) == 6 ? 4 : \
 (n) == 7 ? 5 : \
 (n) + 4)

#undef  DBX_REGISTER_NUMBER
#define DBX_REGISTER_NUMBER(n)	((write_symbols == DWARF_DEBUG)	\
				? DWARF_DBX_REGISTER_NUMBER(n)	\
				: STABS_DBX_REGISTER_NUMBER(n))

/* Tell final.c that we don't need a label passed to mcount.  */
#define NO_PROFILE_DATA

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  */
/* Redefine this to not pass an unused label in %edx.  */

#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)  \
{									\
  if (flag_pic)								\
    fprintf (FILE, "\tcall *mcount@GOT(%%ebx)\n");			\
  else									\
    fprintf (FILE, "\tcall mcount\n");					\
}

#define FUNCTION_PROFILER_EPILOGUE(FILE)  \
{									\
  if (TARGET_PROFILER_EPILOGUE)						\
    {									\
      if (flag_pic)							\
	fprintf (FILE, "\tcall *mexitcount@GOT(%%ebx)\n");		\
      else								\
	fprintf (FILE, "\tcall mexitcount\n");				\
    }									\
}

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"
 
#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"
  
#undef WCHAR_TYPE
#define WCHAR_TYPE "int"
 
#define WCHAR_UNSIGNED 0

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD

/* FREEBSD_NATIVE is defined when gcc is integrated into the FreeBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

#ifdef FREEBSD_NATIVE

/* Look for the include files in the system-defined places.  */

#define GPLUSPLUS_INCLUDE_DIR		"/usr/include/g++"

#define GCC_INCLUDE_DIR			"/usr/include"

/* FreeBSD has GCC_INCLUDE_DIR first.  */
#define INCLUDE_DEFAULTS		\
  {					\
    { GCC_INCLUDE_DIR, 0, 0 },		\
    { GPLUSPLUS_INCLUDE_DIR, 1, 1 },	\
    { 0, 0, 0 }				\
  }

/* Under FreeBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.  */

#define STANDARD_EXEC_PREFIX		"/usr/libexec/"

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#define STANDARD_STARTFILE_PREFIX	"/usr/lib/"

/* On FreeBSD, gcc is called 'cc' */
#define GCC_NAME			"cc"

/* FreeBSD is 4.4BSD derived */
#define bsd4_4

#endif /* FREEBSD_NATIVE */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dunix -Di386 -D__FreeBSD__=3 -Asystem(unix) -Asystem(FreeBSD) -Acpu(i386) -Amachine(i386)"

#undef CPP_SPEC
#if TARGET_CPU_DEFAULT == 2
#define CPP_SPEC "\
%{!maout: -D__ELF__} \
%{munderscores: -D__UNDERSCORES__} \
%{maout: %{!mno-underscores: -D__UNDERSCORES__}} \
%{fPIC:-D__PIC__ -D__pic__} %{fpic:-D__PIC__ -D__pic__} %{!m386:-D__i486__}"
#else
#define CPP_SPEC "\
%{!maout: -D__ELF__} \
%{munderscores: -D__UNDERSCORES__} \
%{maout: %{!mno-underscores: -D__UNDERSCORES__}} \
%{fPIC:-D__PIC__ -D__pic__} %{fpic:-D__PIC__ -D__pic__} %{m486:-D__i486__}"
#endif

#undef CC1_SPEC
#define CC1_SPEC "\
%{gline:%{!g:%{!g0:%{!g1:%{!g2: -g1}}}}} \
%{maout: %{!mno-underscores: %{!munderscores: -munderscores }}}"

#undef  ASM_SPEC
#define ASM_SPEC	"%{v*: -v} %{maout: %{fpic:-k} %{fPIC:-k}}"

/* Like the default, except no -lg, and no -p.  */
#undef LIB_SPEC
#define LIB_SPEC "%{!shared:%{!pg:%{!pthread:%{!kthread:-lc}%{kthread:-lpthread -lc}}%{pthread:-lc_r}}%{pg:%{!pthread:%{!kthread:-lc_r}%{kthread:-lpthread_p -lc_p}}%{pthread:-lc_r_p}}}"

/* Let gcc locate this for us according to the -m rules */
#undef  LIBGCC_SPEC
#define LIBGCC_SPEC "%{!shared:libgcc.a%s}"

/* Provide a LINK_SPEC appropriate for FreeBSD.  Here we provide support
   for the special GCC options -static and -shared, which allow us to
   link things in one of these three modes by applying the appropriate
   combinations of options at link-time. We like to support here for
   as many of the other GNU linker options as possible. But I don't
   have the time to search for those flags. I am sure how to add
   support for -soname shared_object_name. H.J.

   When the -shared link option is used a final link is not being
   done.  */

#undef	LINK_SPEC
#define LINK_SPEC "\
 %{p:%e`-p' not supported; use `-pg' and gprof(1)} \
  %{maout: %{shared:-Bshareable} \
    %{!shared:%{!nostdlib:%{!r:%{!e*:-e start}}} -dc -dp %{static:-Bstatic} \
      %{pg:-Bstatic} %{Z}} \
    %{assert*} %{R*}} \
  %{!maout: \
    -m elf_i386 \
    %{Wl,*:%*} \
    %{assert*} %{R*} %{rpath*} %{defsym*} \
    %{shared:-Bshareable %{h*} %{soname*}} \
    %{symbolic:-Bsymbolic} \
    %{!shared: \
      %{!static: \
	%{rdynamic: -export-dynamic} \
	%{!dynamic-linker: -dynamic-linker /usr/libexec/ld-elf.so.1}} \
      %{static:-Bstatic}}}"

/* Get perform_* macros to build libgcc.a.  */
#include "i386/perform.h"

#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
  %{maout: %{shared:c++rt0.o%s} \
    %{!shared:%{pg:gcrt0.o%s}%{!pg:%{static:scrt0.o%s}%{!static:crt0.o%s}}}} \
  %{!maout:  %{!shared: \
    %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}} \
    crti.o%s %{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}}"

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{!maout: %{!shared:crtend.o%s} %{shared:crtendS.o%s} crtn.o%s}"

/* This goes away when the math emulator is fixed.  */
#undef TARGET_DEFAULT
#define TARGET_DEFAULT	(MASK_NO_FANCY_MATH_387 | 0301)

#define HAVE_ATEXIT
#define HAVE_PUTENV

/* to assist building libgcc2.c */
#ifndef __ELF__
#undef OBJECT_FORMAT_ELF
#endif
