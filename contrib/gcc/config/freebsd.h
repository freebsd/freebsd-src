/* Base configuration file for all FreeBSD targets.
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.

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

/* Common FreeBSD configuration. 
   All FreeBSD architectures should include this file, which will specify
   their commonalities.
   Adapted from /usr/src/contrib/gcc/config/i386/freebsd.h,
   /usr/src/contrib/gcc/config/svr4.h & 
   egcs/gcc/config/i386/freebsd-elf.h version by David O'Brien  */

/* $FreeBSD$ */


/* Cpp, assembler, linker, library, and startfile spec's.  */

/* This defines which switch letters take arguments.  On FreeBSD, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker) (comming from svr4).
   We also have -R (alias --rpath), no -z, --soname (-h), --assert etc.  */

#define FBSD_SWITCH_TAKES_ARG(CHAR) \
  (DEFAULT_SWITCH_TAKES_ARG (CHAR) \
   || (CHAR) == 'h' \
   || (CHAR) == 'z' /* ignored by ld */ \
   || (CHAR) == 'R')

#undef  SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR)	(FBSD_SWITCH_TAKES_ARG(CHAR))

/* This defines which multi-letter switches take arguments.  */

#define FBSD_WORD_SWITCH_TAKES_ARG(STR)					\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)					\
   || !strcmp ((STR), "rpath") || !strcmp ((STR), "rpath-link")		\
   || !strcmp ((STR), "soname") || !strcmp ((STR), "defsym") 		\
   || !strcmp ((STR), "assert") || !strcmp ((STR), "dynamic-linker"))

#undef  WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR)	(FBSD_WORD_SWITCH_TAKES_ARG(STR))

/* Place spaces around this string.  We depend on string splicing to produce
   the final CPP_PREDEFINES value.  */
#define FBSD_CPP_PREDEFINES " -Dunix -D__FreeBSD__=4 -D__FreeBSD_cc_version=420000 -Asystem(unix) -Asystem(FreeBSD) "

#define FBSD_CPP_SPEC "\
  %(cpp_cpu) \
  %{!maout: -D__ELF__} \
  %{munderscores: -D__UNDERSCORES__} \
  %{maout: %{!mno-underscores: -D__UNDERSCORES__}} \
  %{fPIC:-D__PIC__ -D__pic__} %{fpic:-D__PIC__ -D__pic__} \
  %{posix:-D_POSIX_SOURCE}"

#undef  CPP_SPEC
#define CPP_SPEC FBSD_CPP_SPEC

/* Provide a LIB_SPEC appropriate for FreeBSD.  Just select the appropriate
   libc, depending on whether we're doing profiling. 
   (like the default, except no -lg, and no -p).  */
#undef  LIB_SPEC
#define LIB_SPEC "\
  %{!shared: \
    %{!pg: \
      %{!pthread:-lc} \
      %{pthread:-lc_r}} \
    %{pg: \
      %{!pthread:-lc_p} \
      %{pthread:-lc_r_p}} \
  }"


/************************[  Target stuff  ]***********************************/

/* All FreeBSD Architectures support the ELF object file format.  */
#undef  OBJECT_FORMAT_ELF
#define OBJECT_FORMAT_ELF

/* Don't assume anything about the header files.  */
#undef  NO_IMPLICIT_EXTERN_C
#define NO_IMPLICIT_EXTERN_C

/* Implicit library calls should use memcpy, not bcopy, etc.  */
#undef  TARGET_MEM_FUNCTIONS
#define TARGET_MEM_FUNCTIONS

/* Allow #sccs in preprocessor.  */
#undef  SCCS_DIRECTIVE
#define SCCS_DIRECTIVE

/* Tell libgcc2.c that FreeBSD targets support atexit(3).  */
#undef  HAVE_ATEXIT
#define HAVE_ATEXIT

/* Code generation parameters.  */

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions
   (even though the svr4 ABI for the i386 says that records and unions are
   returned in memory).  */
#undef  DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Writing `int' for a bitfield forces int alignment for the structure.  */
/* XXX: ok for Alpha??  */
#undef  PCC_BITFIELD_TYPE_MATTERS
#define PCC_BITFIELD_TYPE_MATTERS 1

/* Use periods rather than dollar signs in special g++ assembler names.
   This ensures the configuration knows our system correctly so we can link
   with libraries compiled with the native cc.  */
#undef NO_DOLLAR_IN_LABEL

/* The prefix to add to user-visible assembler symbols.
   For System V Release 4 & ELF the convention is *not* to prepend a leading
   underscore onto user-level symbol names.  */

#undef  USER_LABEL_PREFIX
#define USER_LABEL_PREFIX ""

/* Handle #pragma weak and #pragma pack.  */
#undef  HANDLE_SYSV_PRAGMA
#define HANDLE_SYSV_PRAGMA

/* FreeBSD ELF using our home-grown crtbegin.o/crtend.o does not support the
   DWARF2 unwinding mechanisms.  Once `make world' bootstraping problems with
   the EGCS crtstuff.c is overcome, we will switch to the non-sjlj-exceptions 
   type exception machanism.  */
#define DWARF2_UNWIND_INFO 0

/* Do not use ``thunks'' to implement C++ vtables.  This method still has
   fatal bugs.  Also, GCC 3.0 will have a new C++ ABI that may not even
   support `thunks'.  */
#undef DEFAULT_VTABLE_THUNKS


/************************[  Assembler stuff  ]********************************/

/* Override the default comment-starter of "/".  */
#undef  ASM_COMMENT_START
#define ASM_COMMENT_START	"#"

/* Attach a special .ident directive to the end of the file to identify
   the version of GCC which compiled this code.  The format of the
   .ident string is patterned after the ones produced by native svr4
   C compilers.  */

#undef  IDENT_ASM_OP
#define IDENT_ASM_OP	".ident"

/* Output #ident as a .ident.  */

#undef  ASM_OUTPUT_IDENT
#define ASM_OUTPUT_IDENT(FILE, NAME) \
  fprintf ((FILE), "\t%s\t\"%s\"\n", IDENT_ASM_OP, (NAME));

/* Identify the front-end which produced this file.  To keep symbol
   space down, and not confuse kdb, only do this if the language is
   not C. (svr4.h defines ASM_IDENTIFY_GCC but neglects this) */

#undef  ASM_IDENTIFY_LANGUAGE
#define ASM_IDENTIFY_LANGUAGE(FILE)					\
  {									\
    if (strcmp (lang_identify (), "c") != 0)				\
        output_lang_identify (FILE);					\
  }

#undef  ASM_FILE_END
#define ASM_FILE_END(FILE)						\
  do {				 					\
    if (!flag_no_ident)							\
      fprintf ((FILE), "\t%s\t\"[ASM_FILE_END]GCC: (%s) %s\"\n",	\
		IDENT_ASM_OP, lang_identify(), version_string);		\
  } while (0)

/* This is the pseudo-op used to generate a contiguous sequence of byte
   values from a double-quoted string WITHOUT HAVING A TERMINATING NUL
   AUTOMATICALLY APPENDED.  This is the same for most svr4 assemblers.  */

#undef  ASCII_DATA_ASM_OP
#define ASCII_DATA_ASM_OP	".ascii"

#undef  ASM_BYTE_OP
#define ASM_BYTE_OP		".byte"

/* This is how to allocate empty space in some section.  The .zero
   pseudo-op is used for this on most ELF assemblers.  */

#undef  SKIP_ASM_OP
#define SKIP_ASM_OP		".zero"

/* How to output some space.  The rules are different depending on the
   object format.  */
#undef  ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE, SIZE) 					\
  do {									\
    if (TARGET_ELF)							\
      {									\
        fprintf ((FILE), "\t%s\t%u\n", SKIP_ASM_OP, (SIZE));		\
      }									\
    else								\
      {									\
        fprintf ((FILE), "\t.space %u\n", (SIZE));			\
      }									\
  } while (0)

/* A table of bytes codes used by the ASM_OUTPUT_ASCII and
   ASM_OUTPUT_LIMITED_STRING macros.  Each byte in the table
   corresponds to a particular byte value [0..255].  For any
   given byte value, if the value in the corresponding table
   position is zero, the given character can be output directly.
   If the table value is 1, the byte must be output as a \ooo
   octal escape.  If the tables value is anything else, then the
   byte value should be output as a \ followed by the value
   in the table.  Note that we can use standard UN*X escape
   sequences for many control characters, but we don't use
   \a to represent BEL because some svr4 assemblers (e.g. on
   the i386) don't know about that.  Also, we don't use \v
   since some versions of gas, such as 2.2 did not accept it.  */

#define ESCAPES \
"\1\1\1\1\1\1\1\1btn\1fr\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\
\0\0\"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\
\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\\\0\0\0\
\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\
\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\
\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\
\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\
\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1"

/* Some svr4 assemblers have a limit on the number of characters which
   can appear in the operand of a .string directive.  If your assembler
   has such a limitation, you should define STRING_LIMIT to reflect that
   limit.  Note that at least some svr4 assemblers have a limit on the
   actual number of bytes in the double-quoted string, and that they
   count each character in an escape sequence as one byte.  Thus, an
   escape sequence like \377 would count as four bytes.

   If your target assembler doesn't support the .string directive, you
   should define this to zero.
*/

#undef  STRING_LIMIT
#define STRING_LIMIT	((unsigned) 256)

#undef  STRING_ASM_OP
#define STRING_ASM_OP	".string"

/* Output the label which precedes a jumptable.  Note that for all svr4/ELF
   systems where we actually generate jumptables (which is to say every
   svr4 target except i386, where we use casesi instead) we put the jump-
   tables into the .rodata section and since other stuff could have been
   put into the .rodata section prior to any given jumptable, we have to
   make sure that the location counter for the .rodata section gets pro-
   perly re-aligned prior to the actual beginning of the jump table.  */

#undef  ALIGN_ASM_OP
#define ALIGN_ASM_OP	".align"

/* This says how to output assembler code to declare an
   uninitialized external linkage data object.  Under SVR4/ELF,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef  COMMON_ASM_OP
#define COMMON_ASM_OP	".comm"

#undef  ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGN)		\
  do {									\
    if (TARGET_ELF)							\
      {							\
	fprintf ((FILE), "\t%s\t", COMMON_ASM_OP);			\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), ",%u,%u\n", (SIZE), (ALIGN) / BITS_PER_UNIT);	\
      }									\
    else								\
      {									\
	int rounded = (SIZE);						\
	if (rounded == 0) rounded = 1;					\
	rounded += (BIGGEST_ALIGNMENT / BITS_PER_UNIT) - 1;		\
	rounded = (rounded / (BIGGEST_ALIGNMENT / BITS_PER_UNIT)	\
		   * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));		\
	fputs (".comm ", (FILE));					\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), ",%u\n", (rounded));				\
      }									\
  } while (0)

/* This says how to output assembler code to declare an
   uninitialized internal linkage data object.  Under SVR4/ELF,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef  LOCAL_ASM_OP
#define LOCAL_ASM_OP	".local"

/* This says how to output assembler code to declare an
   uninitialized internal linkage data object.  Under SVR4,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef  ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN)		\
  do {									\
    if (TARGET_ELF)							\
      {									\
	fprintf ((FILE), "\t%s\t", LOCAL_ASM_OP);			\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), "\n");						\
	ASM_OUTPUT_ALIGNED_COMMON ((FILE), (NAME), (SIZE), (ALIGN));	\
      }									\
    else								\
      {									\
	int rounded = (SIZE);						\
	if (rounded == 0) rounded = 1;					\
	rounded += (BIGGEST_ALIGNMENT / BITS_PER_UNIT) - 1;		\
	rounded = (rounded / (BIGGEST_ALIGNMENT / BITS_PER_UNIT)	\
		   * (BIGGEST_ALIGNMENT / BITS_PER_UNIT));		\
	fputs (".lcomm ", (FILE));					\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), ",%u\n", (rounded));				\
      }									\
  } while (0)

#undef  ASM_OUTPUT_BEFORE_CASE_LABEL
#define ASM_OUTPUT_BEFORE_CASE_LABEL(FILE, PREFIX, NUM, TABLE)		\
  ASM_OUTPUT_ALIGN ((FILE), 2);

#undef  ASM_OUTPUT_CASE_LABEL
#define ASM_OUTPUT_CASE_LABEL(FILE, PREFIX, NUM, JUMPTABLE)		\
  do {									\
    ASM_OUTPUT_BEFORE_CASE_LABEL ((FILE), (PREFIX), (NUM), (JUMPTABLE))	\
    ASM_OUTPUT_INTERNAL_LABEL ((FILE), (PREFIX), (NUM));		\
  } while (0)

/* The standard SVR4/ELF assembler seems to require that certain builtin
   library routines (e.g. .udiv) be explicitly declared as .globl
   in each assembly file where they are referenced.  */

#undef  ASM_OUTPUT_EXTERNAL_LIBCALL
#define ASM_OUTPUT_EXTERNAL_LIBCALL(FILE, FUN)				\
  ASM_GLOBALIZE_LABEL ((FILE), XSTR ((FUN), 0))

/* Support const sections and the ctors and dtors sections for g++.
   Note that there appears to be two different ways to support const
   sections at the moment.  You can either #define the symbol
   READONLY_DATA_SECTION (giving it some code which switches to the
   readonly data section) or else you can #define the symbols
   EXTRA_SECTIONS, EXTRA_SECTION_FUNCTIONS, SELECT_SECTION, and
   SELECT_RTX_SECTION.  We do both here just to be on the safe side.  
   FreeBSD conditionalizes the use of ".section rodata" depending on
   ELF mode - otherwise .text.  */

#undef  USE_CONST_SECTION
#define USE_CONST_SECTION	TARGET_ELF

#undef  CONST_SECTION_ASM_OP
#define CONST_SECTION_ASM_OP	".section\t.rodata"

/* Define the pseudo-ops used to switch to the .ctors and .dtors sections.

   Note that we want to give these sections the SHF_WRITE attribute
   because these sections will actually contain data (i.e. tables of
   addresses of functions in the current root executable or shared library
   file) and, in the case of a shared library, the relocatable addresses
   will have to be properly resolved/relocated (and then written into) by
   the dynamic linker when it actually attaches the given shared library
   to the executing process.  (Note that on SVR4, you may wish to use the
   `-z text' option to the ELF linker, when building a shared library, as
   an additional check that you are doing everything right.  But if you do
   use the `-z text' option when building a shared library, you will get
   errors unless the .ctors and .dtors sections are marked as writable
   via the SHF_WRITE attribute.)  */

#undef  CTORS_SECTION_ASM_OP
#define CTORS_SECTION_ASM_OP	".section\t.ctors,\"aw\""
#undef  DTORS_SECTION_ASM_OP
#define DTORS_SECTION_ASM_OP	".section\t.dtors,\"aw\""

/* On svr4, we *do* have support for the .init and .fini sections, and we
   can put stuff in there to be executed before and after `main'.  We let
   crtstuff.c and other files know this by defining the following symbols.
   The definitions say how to change sections to the .init and .fini
   sections.  This is the same for all known svr4 assemblers.  */

#undef  INIT_SECTION_ASM_OP
#define INIT_SECTION_ASM_OP	".section\t.init"
#undef  FINI_SECTION_ASM_OP
#define FINI_SECTION_ASM_OP	".section\t.fini"

/* A default list of other sections which we might be "in" at any given
   time.  For targets that use additional sections (e.g. .tdesc) you
   should override this definition in the target-specific file which
   includes this file.  */

#undef  EXTRA_SECTIONS
#define EXTRA_SECTIONS	in_const, in_ctors, in_dtors

/* A default list of extra section function definitions.  For targets
   that use additional sections (e.g. .tdesc) you should override this
   definition in the target-specific file which includes this file.  */

#undef  EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS						\
  CONST_SECTION_FUNCTION						\
  CTORS_SECTION_FUNCTION						\
  DTORS_SECTION_FUNCTION

#undef  READONLY_DATA_SECTION
#define READONLY_DATA_SECTION()	const_section ()

extern void text_section ();

#undef  CONST_SECTION_FUNCTION
#define CONST_SECTION_FUNCTION						\
  void									\
  const_section ()							\
  {									\
    if (!USE_CONST_SECTION)						\
      text_section();							\
    else if (in_section != in_const)					\
      {									\
	fprintf (asm_out_file, "%s\n", CONST_SECTION_ASM_OP);		\
	in_section = in_const;						\
      }									\
  }

#undef  CTORS_SECTION_FUNCTION
#define CTORS_SECTION_FUNCTION						\
  void									\
  ctors_section ()							\
  {									\
    if (in_section != in_ctors)						\
      {									\
	fprintf (asm_out_file, "%s\n", CTORS_SECTION_ASM_OP);		\
	in_section = in_ctors;						\
      }									\
  }

#undef  DTORS_SECTION_FUNCTION
#define DTORS_SECTION_FUNCTION						\
  void									\
  dtors_section ()							\
  {									\
    if (in_section != in_dtors)						\
      {									\
 	fprintf (asm_out_file, "%s\n", DTORS_SECTION_ASM_OP);		\
	in_section = in_dtors;						\
      }									\
  }

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#undef  ASM_OUTPUT_CONSTRUCTOR
#define ASM_OUTPUT_CONSTRUCTOR(FILE, NAME)				\
  do {									\
    if (TARGET_ELF)							\
      {									\
	ctors_section ();						\
	fprintf ((FILE), "\t%s\t ", INT_ASM_OP);			\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), "\n");						\
      }									\
    else								\
      {									\
	fprintf (asm_out_file, "%s \"%s__CTOR_LIST__\",22,0,0,",	\
		 ASM_STABS_OP, (TARGET_UNDERSCORES) ? "_" : "");	\
	assemble_name (asm_out_file, name);				\
	fputc ('\n', asm_out_file);					\
      }									\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#undef  ASM_OUTPUT_DESTRUCTOR
#define ASM_OUTPUT_DESTRUCTOR(FILE, NAME)				\
  do {									\
    if (TARGET_ELF)							\
      {									\
	dtors_section ();						\
	fprintf ((FILE), "\t%s\t ", INT_ASM_OP);			\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), "\n");						\
      }									\
    else								\
      {									\
	fprintf (asm_out_file, "%s \"%s__DTOR_LIST__\",22,0,0,",	\
		 ASM_STABS_OP, (TARGET_UNDERSCORES) ? "_" : "");	\
	assemble_name (asm_out_file, name);				\
	fputc ('\n', asm_out_file);					\
      }									\
  } while (0)

/* A C statement or statements to switch to the appropriate
   section for output of RTX in mode MODE.  RTX is some kind
   of constant in RTL.  The argument MODE is redundant except
   in the case of a `const_int' rtx.  Currently, these always
   go into the const section.  */

#undef  SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(MODE, RTX)	const_section()

/* Define the strings used for the special svr4/ELF .type and .size
   directives.  These strings generally do not vary from one svr4/ELF
   system to another.  */

#undef  TYPE_ASM_OP
#define TYPE_ASM_OP	".type"
#undef  SIZE_ASM_OP
#define SIZE_ASM_OP	".size"

/* This is how we tell the assembler that a symbol is weak.  */

#undef  ASM_WEAKEN_LABEL
#define ASM_WEAKEN_LABEL(FILE, NAME)					\
  do {									\
    fputs ("\t.globl\t", (FILE)); assemble_name ((FILE), (NAME));	\
    fputc ('\n', (FILE));						\
    fputs ("\t.weak\t", (FILE)); assemble_name ((FILE), (NAME));	\
    fputc ('\n', (FILE));						\
  } while (0)

/* The following macro defines the [default] format used with ELF to output
   the second operand of the .type assembler directive.  */

#undef  TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT	"@%s"

/* Write the extra assembler code needed to declare a function's result.
   Most svr4/ELF assemblers don't require any special declaration of the
   result value.  */

#undef  ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)

/* These macros generate the special .type and .size directives which
   are used to set the corresponding fields of the linker symbol table
   entries in an ELF object file under SVR4/ELF.  These macros also output
   the starting labels for the relevant functions/objects.  */

/* Write the extra assembler code needed to declare an object properly.  */

#undef  ASM_DECLARE_OBJECT_NAME
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
	putc (',', FILE);						\
	fprintf (FILE, HOST_WIDE_INT_PRINT_DEC,				\
		 int_size_in_bytes (TREE_TYPE (DECL)));			\
	fputc ('\n', FILE);						\
      }									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */

#undef  ASM_FINISH_DECLARE_OBJECT
#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)	\
  do {									\
    char *name = XSTR (XEXP (DECL_RTL (DECL), 0), 0);			\
    if (!flag_inhibit_size_directive && DECL_SIZE (DECL)		\
	&& ! AT_END && TOP_LEVEL					\
	&& DECL_INITIAL (DECL) == error_mark_node			\
	&& !size_directive_output)					\
      {									\
	size_directive_output = 1;					\
	fprintf (FILE, "\t%s\t ", SIZE_ASM_OP);				\
	assemble_name (FILE, name);					\
	putc (',', FILE);						\
	fprintf (FILE, HOST_WIDE_INT_PRINT_DEC,				\
		int_size_in_bytes (TREE_TYPE (DECL))); 			\
	fputc ('\n', FILE);						\
      }									\
  } while (0)


/************************[  Debugger stuff  ]*********************************/

/* All ELF targets can support DWARF-2.  */
#undef  DWARF2_DEBUGGING_INFO
#define DWARF2_DEBUGGING_INFO

/* This is BSD, so we want the DBX format.  */
#undef  DBX_DEBUGGING_INFO
#define DBX_DEBUGGING_INFO

/* Use stabs instead of DWARF debug format.  */
#undef  PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

/* But allow STABS to be supported as well.
   	Note that we want to override some definition settings done for some
   	architecture's native OS's tools that don't apply to us.  */
#undef ASM_IDENTIFY_GCC
#undef ASM_IDENTIFY_LANGUAGE

#include "dbxelf.h"
