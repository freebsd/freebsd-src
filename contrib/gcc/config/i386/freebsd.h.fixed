/* Definitions of target machine for GNU compiler for Intel 80386
   running FreeBSD.
   Copyright (C) 1988, 1992, 1994 Free Software Foundation, Inc.
   Contributed by Poul-Henning Kamp <phk@login.dkuug.dk>

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

/* This is tested by i386gas.h.  */
#define YES_UNDERSCORES

/* Don't assume anything about the header files. */
#define NO_IMPLICIT_EXTERN_C

#include "i386/gstabs.h"

/* Get perform_* macros to build libgcc.a.  */
#include "i386/perform.h"

/* This was cloned from ../netbsd.h.  It and several other things in
   this file should be in ../freebsd.h.  */
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

#define MASK_PROFILER_EPILOGUE	010000000000

#define TARGET_PROFILER_EPILOGUE (target_flags & MASK_PROFILER_EPILOGUE)

#undef	SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
     { "profiler-epilogue",	 MASK_PROFILER_EPILOGUE},		\
     { "no-profiler-epilogue",	-MASK_PROFILER_EPILOGUE},


#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dunix -Di386 -D__FreeBSD__=3 -Asystem(unix) -Asystem(FreeBSD) -Acpu(i386) -Amachine(i386)"

#define ASM_SPEC   " %| %{fpic:-k} %{fPIC:-k}"

/* Like the default, except no -lg, and no -p.  */
#define LIB_SPEC "%{!shared:%{!pg:-lc}%{pg:-lc_p}}"

#define LINK_SPEC \
  "%{p:%e`-p' not supported; use `-pg' and gprof(1)} \
   %{shared:-Bshareable} \
   %{!shared:%{!nostdlib:%{!r:%{!e*:-e start}}} -dc -dp %{static:-Bstatic} \
   %{pg:-Bstatic} %{Z}} \
   %{assert*} %{R*}"

#define LINK_LIBGCC_SPECIAL_1	1

#define STARTFILE_SPEC  \
  "%{shared:c++rt0.o%s} \
   %{!shared:%{pg:gcrt0.o%s}%{!pg:%{static:scrt0.o%s}%{!static:crt0.o%s}}}"

/* This goes away when the math emulator is fixed.  */
#undef TARGET_DEFAULT
#define TARGET_DEFAULT	(MASK_NO_FANCY_MATH_387 | 0301)

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#define WCHAR_UNSIGNED 0

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD

#define HAVE_ATEXIT

#define HAVE_PUTENV

/* Override the default comment-starter of "/".  */

#undef ASM_COMMENT_START
#define ASM_COMMENT_START "#"

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* The following macros are stolen from i386v4.h */
/* These have to be defined to get PIC code correct */

/* This is how to output an element of a case-vector that is relative.
   This is only used for PIC code.  See comments by the `casesi' insn in
   i386.md for an explanation of the expression this outputs. */

#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL) \
  fprintf (FILE, "\t.long _GLOBAL_OFFSET_TABLE_+[.-%s%d]\n", LPREFIX, VALUE)

/* Indicate that jump tables go in the text section.  This is
   necessary when compiling PIC code.  */

#define JUMP_TABLES_IN_TEXT_SECTION

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Tell final.c that we don't need a label passed to mcount.  */

#define NO_PROFILE_DATA

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

/*
 * Some imports from svr4.h in support of shared libraries.
 * Currently, we need the DECLARE_OBJECT_SIZE stuff.
 */

#define HANDLE_SYSV_PRAGMA

/* Define the strings used for the special svr4 .type and .size directives.
   These strings generally do not vary from one system running svr4 to
   another, but if a given system (e.g. m88k running svr) needs to use
   different pseudo-op names for these, they may be overridden in the
   file which includes this one.  */

#define TYPE_ASM_OP	".type"
#define SIZE_ASM_OP	".size"
#define SET_ASM_OP	".set"

/* This is how we tell the assembler that a symbol is weak.  */
#define ASM_WEAKEN_LABEL(FILE,NAME) \
  do { fputs ("\t.weak\t", FILE); assemble_name (FILE, NAME); \
       fputc ('\n', FILE); } while (0)

/* The following macro defines the format used to output the second
   operand of the .type assembler directive.  Different svr4 assemblers
   expect various different forms for this operand.  The one given here
   is just a default.  You may need to override it in your machine-
   specific tm.h file (depending upon the particulars of your assembler).  */

#define TYPE_OPERAND_FMT	"@%s"

/* Write the extra assembler code needed to declare a function's result.
   Most svr4 assemblers don't require any special declaration of the
   result value, but there are exceptions.  */

#ifndef ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)
#endif

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare a function properly.
   Some svr4 assemblers need to also have something extra said about the
   function's return value.  We allow for that here.  */

#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "function");			\
    putc ('\n', FILE);							\
    ASM_DECLARE_RESULT (FILE, DECL_RESULT (DECL));			\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Write the extra assembler code needed to declare an object properly.  */

#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    fprintf (FILE, "\t%s\t ", TYPE_ASM_OP);				\
    assemble_name (FILE, NAME);						\
    putc (',', FILE);							\
    fprintf (FILE, TYPE_OPERAND_FMT, "object");				\
    putc ('\n', FILE);							\
    size_directive_output = 0;						\
    if (!flag_inhibit_size_directive && DECL_SIZE (DECL))		\
      {									\
        size_directive_output = 1;					\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, NAME);					\
	fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (DECL)));	\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */

#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)        \
do {                                                                    \
     char *name = XSTR (XEXP (DECL_RTL (DECL), 0), 0);                  \
     if (!flag_inhibit_size_directive && DECL_SIZE (DECL)	        \
         && ! AT_END && TOP_LEVEL                                       \
         && DECL_INITIAL (DECL) == error_mark_node                      \
         && !size_directive_output)                                     \
       {                                                                \
         fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);                        \
	 assemble_name (FILE, name);                                    \
	 fprintf (FILE, ",%d\n",  int_size_in_bytes (TREE_TYPE (DECL)));\
	}								\
   } while (0)


/* This is how to declare the size of a function.  */

#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
    if (!flag_inhibit_size_directive)					\
      {									\
        char label[256];						\
	static int labelno;						\
	labelno++;							\
	ASM_GENERATE_INTERNAL_LABEL (label, "Lfe", labelno);		\
	ASM_OUTPUT_INTERNAL_LABEL (FILE, "Lfe", labelno);		\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, (FNAME));					\
        fprintf (FILE, ",");						\
	assemble_name (FILE, label);					\
        fprintf (FILE, "-");						\
	assemble_name (FILE, (FNAME));					\
	putc ('\n', FILE);						\
      }									\
  } while (0)
