/* Definitions of target machine for GNU compiler,
   for Alpha FreeBSD systems.
   Copyright (C) 1998 Free Software Foundation, Inc.

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


/* Make gcc agree with <machine/ansi.h> */

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

#undef WCHAR_UNSIGNED
#define WCHAR_UNSIGNED 0


/* Provide a CPP_SPEC appropriate for FreeBSD.  Current we just deal with
   the GCC option `-posix'.  */

#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE}"

/* Provide an ASM_SPEC appropriate for a FreeBSD/Alpha target.  This differs
   from the generic FreeBSD ASM_SPEC in that no special handling of PIC is
   necessary on the Alpha.  */

#undef ASM_SPEC
#define ASM_SPEC " %| %{mcpu=*:-m%*}"

#undef ASM_FINAL_SPEC

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  Under FreeBSD/Alpha, the assembler does
   nothing special with -pg. */

#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)			\
	fputs ("\tjsr $28,_mcount\n", (FILE)); /* at */

/* Show that we need a GP when profiling.  */
#define TARGET_PROFILING_NEEDS_GP

#undef HAS_INIT_SECTION

#undef PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (FreeBSD/alpha ELF)");

/* Names to predefine in the preprocessor for this target machine.
   XXX FreeBSD, by convention, shouldn't do __alpha, but lots of applications
   expect it because that's what OSF/1 does.  */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES \
  "-D__alpha__ -D__alpha -D__ELF__ -Acpu(alpha) -Amachine(alpha)"	\
  CPP_FBSD_PREDEFINES

#undef LINK_SPEC
#define LINK_SPEC "-m elf64alpha					\
  %{p:%e`-p' not supported; use `-pg' and gprof(1)}		\
  %{Wl,*:%*}							\
  %{assert*} %{R*} %{rpath*} %{defsym*}				\
  %{shared:-Bshareable %{h*} %{soname*}}			\
  %{symbolic:-Bsymbolic}					\
  %{!shared:							\
    %{!static:							\
      %{rdynamic:-export-dynamic}				\
      %{!dynamic-linker:-dynamic-linker /usr/libexec/ld-elf.so.1} \
    %{static:-Bstatic}}"

/* Provide a STARTFILE_SPEC for FreeBSD that is compatible with the
   non-aout version used on i386. */
   
#undef	STARTFILE_SPEC
#define STARTFILE_SPEC \
 "%{!shared: %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}} \
    %{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"

/* Provide a ENDFILE_SPEC appropriate for FreeBSD.  Here we tack on
   the file which provides part of the support for getting C++
   file-scope static object deconstructed after exiting `main' */

#undef	ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{!shared:crtend.o%s} %{shared:crtendS.o%s}"

/* Handle #pragma weak and #pragma pack.  */

#define HANDLE_SYSV_PRAGMA

#undef SET_ASM_OP
#define SET_ASM_OP	".set"

#undef OBJECT_FORMAT_COFF
#undef EXTENDED_COFF
#define OBJECT_FORMAT_ELF

/* This is the char to use for continuation (in case we need to turn
   continuation back on).  */

#undef DBX_CONTIN_CHAR
#define DBX_CONTIN_CHAR '?'

#undef TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_FP | MASK_FPREGS | MASK_GAS)

/* Output at beginning of assembler file.  */

#undef ASM_FILE_START
#define ASM_FILE_START(FILE)						\
{									\
  alpha_write_verstamp (FILE);						\
  output_file_directive ((FILE), main_input_filename);			\
  fprintf ((FILE), "\t.version\t\"01.01\"\n");				\
  fprintf ((FILE), "\t.set noat\n");					\
}

#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(STREAM, LINE)				\
  alpha_output_lineno ((STREAM), (LINE))
extern void alpha_output_lineno ();

extern void output_file_directive ();

/* Attach a special .ident directive to the end of the file to identify
   the version of GCC which compiled this code.  The format of the
   .ident string is patterned after the ones produced by native svr4
   C compilers.  */

#undef IDENT_ASM_OP
#define IDENT_ASM_OP ".ident"

#ifdef IDENTIFY_WITH_IDENT
#undef ASM_IDENTIFY_GCC
#define ASM_IDENTIFY_GCC(FILE) /* nothing */
#undef ASM_IDENTIFY_LANGUAGE
#define ASM_IDENTIFY_LANGUAGE(FILE)					\
 fprintf((FILE), "\t%s \"GCC (%s) %s\"\n", IDENT_ASM_OP,		\
	 lang_identify(), version_string)
#else
#undef ASM_FILE_END
#define ASM_FILE_END(FILE)						\
do {									\
     fprintf ((FILE), "\t%s\t\"GCC: (GNU) %s\"\n",			\
	      IDENT_ASM_OP, version_string);				\
   } while (0)
#endif

/* Allow #sccs in preprocessor.  */

#define SCCS_DIRECTIVE

/* Output #ident as a .ident.  */

#undef ASM_OUTPUT_IDENT
#define ASM_OUTPUT_IDENT(FILE, NAME) \
  fprintf ((FILE), "\t%s\t\"%s\"\n", IDENT_ASM_OP, (NAME));

/* This is how to allocate empty space in some section.  The .zero
   pseudo-op is used for this on most svr4 assemblers.  */

#undef SKIP_ASM_OP
#define SKIP_ASM_OP	".zero"

#undef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE, SIZE) \
  fprintf ((FILE), "\t%s\t%u\n", SKIP_ASM_OP, (SIZE))

/* Output the label which precedes a jumptable.  Note that for all svr4
   systems where we actually generate jumptables (which is to say every
   svr4 target except i386, where we use casesi instead) we put the jump-
   tables into the .rodata section and since other stuff could have been
   put into the .rodata section prior to any given jumptable, we have to
   make sure that the location counter for the .rodata section gets pro-
   perly re-aligned prior to the actual beginning of the jump table.  */

#undef ALIGN_ASM_OP
#define ALIGN_ASM_OP ".align"

#ifndef ASM_OUTPUT_BEFORE_CASE_LABEL
#define ASM_OUTPUT_BEFORE_CASE_LABEL(FILE, PREFIX, NUM, TABLE)		\
  ASM_OUTPUT_ALIGN ((FILE), 2);
#endif

#undef ASM_OUTPUT_CASE_LABEL
#define ASM_OUTPUT_CASE_LABEL(FILE, PREFIX, NUM, JUMPTABLE)		\
  do {									\
    ASM_OUTPUT_BEFORE_CASE_LABEL ((FILE), (PREFIX), (NUM), (JUMPTABLE))	\
    ASM_OUTPUT_INTERNAL_LABEL ((FILE), (PREFIX), (NUM));		\
  } while (0)

/* The standard SVR4 assembler seems to require that certain builtin
   library routines (e.g. .udiv) be explicitly declared as .globl
   in each assembly file where they are referenced.  */

#undef ASM_OUTPUT_EXTERNAL_LIBCALL
#define ASM_OUTPUT_EXTERNAL_LIBCALL(FILE, FUN)				\
  ASM_GLOBALIZE_LABEL ((FILE), XSTR ((FUN), 0))

/* This says how to output assembler code to declare an
   uninitialized external linkage data object.  Under SVR4,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef COMMON_ASM_OP
#define COMMON_ASM_OP	".comm"

#undef ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGN)		\
do {									\
  fprintf ((FILE), "\t%s\t", COMMON_ASM_OP);				\
  assemble_name ((FILE), (NAME));					\
  fprintf ((FILE), ",%u,%u\n", (SIZE), (ALIGN) / BITS_PER_UNIT);	\
} while (0)

/* This says how to output assembler code to declare an
   uninitialized internal linkage data object.  Under SVR4,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef LOCAL_ASM_OP
#define LOCAL_ASM_OP	".local"

#undef ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN)		\
do {									\
  fprintf ((FILE), "\t%s\t", LOCAL_ASM_OP);				\
  assemble_name ((FILE), (NAME));					\
  fprintf ((FILE), "\n");						\
  ASM_OUTPUT_ALIGNED_COMMON ((FILE), (NAME), (SIZE), (ALIGN));		\
} while (0)

/* This is the pseudo-op used to generate a 64-bit word of data with a
   specific value in some section.  */

#undef INT_ASM_OP
#define INT_ASM_OP		".quad"

/* This is the pseudo-op used to generate a contiguous sequence of byte
   values from a double-quoted string WITHOUT HAVING A TERMINATING NUL
   AUTOMATICALLY APPENDED.  This is the same for most svr4 assemblers.  */

#undef ASCII_DATA_ASM_OP
#define ASCII_DATA_ASM_OP	".ascii"

/* Support const sections and the ctors and dtors sections for g++.
   Note that there appears to be two different ways to support const
   sections at the moment.  You can either #define the symbol
   READONLY_DATA_SECTION (giving it some code which switches to the
   readonly data section) or else you can #define the symbols
   EXTRA_SECTIONS, EXTRA_SECTION_FUNCTIONS, SELECT_SECTION, and
   SELECT_RTX_SECTION.  We do both here just to be on the safe side.  */

#undef USE_CONST_SECTION
#define USE_CONST_SECTION	1

#undef CONST_SECTION_ASM_OP
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

#undef CTORS_SECTION_ASM_OP
#define CTORS_SECTION_ASM_OP	".section\t.ctors,\"aw\""
#undef DTORS_SECTION_ASM_OP
#define DTORS_SECTION_ASM_OP	".section\t.dtors,\"aw\""

/* On svr4, we *do* have support for the .init and .fini sections, and we
   can put stuff in there to be executed before and after `main'.  We let
   crtstuff.c and other files know this by defining the following symbols.
   The definitions say how to change sections to the .init and .fini
   sections.  This is the same for all known svr4 assemblers.  */

#undef INIT_SECTION_ASM_OP
#define INIT_SECTION_ASM_OP	".section\t.init"
#undef FINI_SECTION_ASM_OP
#define FINI_SECTION_ASM_OP	".section\t.fini"

/* A default list of other sections which we might be "in" at any given
   time.  For targets that use additional sections (e.g. .tdesc) you
   should override this definition in the target-specific file which
   includes this file.  */

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_const, in_ctors, in_dtors

/* A default list of extra section function definitions.  For targets
   that use additional sections (e.g. .tdesc) you should override this
   definition in the target-specific file which includes this file.  */

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS						\
  CONST_SECTION_FUNCTION						\
  CTORS_SECTION_FUNCTION						\
  DTORS_SECTION_FUNCTION

#undef READONLY_DATA_SECTION
#define READONLY_DATA_SECTION() const_section ()

extern void text_section ();

#undef CONST_SECTION_FUNCTION
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

#undef CTORS_SECTION_FUNCTION
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

#undef DTORS_SECTION_FUNCTION
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

/* Switch into a generic section.
   This is currently only used to support section attributes.

   We make the section read-only and executable for a function decl,
   read-only for a const data decl, and writable for a non-const data decl.  */
#undef ASM_OUTPUT_SECTION_NAME
#define ASM_OUTPUT_SECTION_NAME(FILE, DECL, NAME, RELOC)		\
  fprintf ((FILE), ".section\t%s,\"%s\",@progbits\n", (NAME),		\
	   (DECL) && TREE_CODE (DECL) == FUNCTION_DECL ? "ax" :		\
	   (DECL) && DECL_READONLY_SECTION ((DECL), (RELOC)) ? "a" : "aw")


/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#undef ASM_OUTPUT_CONSTRUCTOR
#define ASM_OUTPUT_CONSTRUCTOR(FILE, NAME)				\
  do {									\
    ctors_section ();							\
    fprintf ((FILE), "\t%s\t ", INT_ASM_OP);				\
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), "\n");						\
  } while (0)

/* A C statement (sans semicolon) to output an element in the table of
   global destructors.  */
#undef ASM_OUTPUT_DESTRUCTOR
#define ASM_OUTPUT_DESTRUCTOR(FILE, NAME)				\
  do {									\
    dtors_section ();							\
    fprintf ((FILE), "\t%s\t ", INT_ASM_OP);				\
    assemble_name ((FILE), (NAME));					\
    fprintf ((FILE), "\n");						\
  } while (0)

/* A C statement or statements to switch to the appropriate
   section for output of DECL.  DECL is either a `VAR_DECL' node
   or a constant of some sort.  RELOC indicates whether forming
   the initial value of DECL requires link-time relocations.  */

#undef SELECT_SECTION
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
      if ((flag_pic && (RELOC))						\
	  || !TREE_READONLY (DECL) || TREE_SIDE_EFFECTS (DECL)		\
	  || !DECL_INITIAL (DECL)					\
	  || (DECL_INITIAL (DECL) != error_mark_node			\
	      && !TREE_CONSTANT (DECL_INITIAL (DECL))))			\
	data_section ();						\
      else								\
	const_section ();						\
    }									\
  else									\
    const_section ();							\
}

/* A C statement or statements to switch to the appropriate
   section for output of RTX in mode MODE.  RTX is some kind
   of constant in RTL.  The argument MODE is redundant except
   in the case of a `const_int' rtx.  Currently, these always
   go into the const section.  */

#undef SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(MODE,RTX) const_section()

/* Define the strings used for the .type, .size and .set directives.
   These strings generally do not vary from one system running svr4 to
   another, but if a given system (e.g. m88k running svr) needs to use
   different pseudo-op names for these, they may be overridden in the
   file which includes this one.  */

#undef TYPE_ASM_OP
#define TYPE_ASM_OP	".type"
#undef SIZE_ASM_OP
#define SIZE_ASM_OP	".size"

/* This is how we tell the assembler that two symbols have the same value.  */

#undef ASM_OUTPUT_DEF
#define ASM_OUTPUT_DEF(FILE,NAME1,NAME2)				\
  do { assemble_name((FILE), (NAME1));					\
       fputs(" = ", (FILE));						\
       assemble_name((FILE), (NAME2));					\
       fputc('\n', (FILE)); } while (0)

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

#undef ESCAPES
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
   should define this to zero.  */

#undef STRING_LIMIT
#define STRING_LIMIT	((unsigned) 256)

#undef STRING_ASM_OP
#define STRING_ASM_OP	".string"

/* We always use gas here, so we don't worry about ECOFF assembler problems.  */
#undef TARGET_GAS
#define TARGET_GAS	1

/* Implicit library calls should use memcpy, not bcopy, etc.  */

#define TARGET_MEM_FUNCTIONS

/* Some imports from svr4.h in support of shared libraries.  Currently, we
   need the DECLARE_OBJECT_SIZE stuff.  */

/* This is how we tell the assembler that a symbol is weak.  */

#undef ASM_WEAKEN_LABEL
#define ASM_WEAKEN_LABEL(FILE, NAME)					\
  do { fputs ("\t.globl\t", (FILE)); assemble_name ((FILE), (NAME));	\
       fputc ('\n', (FILE));						\
       fputs ("\t.weak\t", (FILE)); assemble_name ((FILE), (NAME));	\
       fputc ('\n', (FILE)); } while (0)

/* The following macro defines the format used to output the second
   operand of the .type assembler directive.  Different svr4 assemblers
   expect various different forms for this operand.  The one given here
   is just a default.  You may need to override it in your machine-
   specific tm.h file (depending upon the particulars of your assembler).  */

#undef TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT	"@%s"

/* Write the extra assembler code needed to declare a function's result.
   Most svr4 assemblers don't require any special declaration of the
   result value, but there are exceptions.  */

#ifndef ASM_DECLARE_RESULT
#define ASM_DECLARE_RESULT(FILE, RESULT)
#endif
