/* Definitions for Intel 386 running FreeBSD with either a.out or ELF format
   Copyright (C) 1996 Free Software Foundation, Inc.
   Contributed by Eric Youngdale.
   Modified for stabs-in-ELF by H.J. Lu.
   Adapted from GNU/Linux version by John Polstra.
   Added support for generating "old a.out gas" on the fly by Peter Wemm.
   Continued development by David O'Brien <obrien@freebsd.org>

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

/* $FreeBSD$ */


#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (i386 FreeBSD/ELF)");

#define MASK_PROFILER_EPILOGUE	010000000000
#define MASK_AOUT		004000000000	/* a.out not elf */
#define MASK_UNDERSCORES	002000000000	/* use leading _ */

#define TARGET_PROFILER_EPILOGUE	(target_flags & MASK_PROFILER_EPILOGUE)
#define TARGET_AOUT			(target_flags & MASK_AOUT)
#define TARGET_ELF			((target_flags & MASK_AOUT) == 0)
#define TARGET_UNDERSCORES		((target_flags & MASK_UNDERSCORES) != 0)

#undef	SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
     { "profiler-epilogue",	 MASK_PROFILER_EPILOGUE},		\
     { "no-profiler-epilogue",	-MASK_PROFILER_EPILOGUE},		\
     { "aout",			 MASK_AOUT},				\
     { "no-aout",		-MASK_AOUT},				\
     { "underscores",		 MASK_UNDERSCORES},			\
     { "no-underscores",	-MASK_UNDERSCORES},

/* Prefix for internally generated assembler labels.  If we aren't using 
   underscores, we are using prefix `.'s to identify labels that should  
   be ignored, as in `i386/gas.h' --karl@cs.umb.edu  */                 
#undef  LPREFIX
#define LPREFIX ((TARGET_UNDERSCORES) ? "L" : ".L")

/* Override the default comment-starter of "/".  */
#undef ASM_COMMENT_START
#define ASM_COMMENT_START "#"

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n" 

#undef SET_ASM_OP
#define SET_ASM_OP	".set"

/* Output at beginning of assembler file.  */
/* The .file command should always begin the output.  */

#undef ASM_FILE_START
#define ASM_FILE_START(FILE)						\
  do {									\
        output_file_directive ((FILE), main_input_filename);		\
	if (TARGET_ELF)							\
          fprintf ((FILE), "\t.version\t\"01.01\"\n");			\
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
  fprintf ((FILE), "%s%s%d:\n", (TARGET_UNDERSCORES) ? "" : ".",	\
	   (PREFIX), (NUM))

/* This is how to output a reference to a user-level label named NAME.  */
#undef  ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE, NAME)					\
  fprintf ((FILE), "%s%s", (TARGET_UNDERSCORES) ? "_" : "", (NAME))


/* This is how to output an element of a case-vector that is relative.
   This is only used for PIC code.  See comments by the `casesi' insn in
   i386.md for an explanation of the expression this outputs. */
#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, BODY, VALUE, REL) \
  fprintf ((FILE), "\t.long _GLOBAL_OFFSET_TABLE_+[.-%s%d]\n", LPREFIX, (VALUE))

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE, LOG)      				\
  if ((LOG)!=0) {							\
    if (in_text_section())						\
      fprintf ((FILE), "\t.p2align %d,0x90\n", (LOG));			\
    else								\
      fprintf ((FILE), "\t.p2align %d\n", (LOG));			\
  }

/* conditionalize the use of ".section rodata" on elf mode - otherwise .text */
#undef USE_CONST_SECTION
#define USE_CONST_SECTION	TARGET_ELF

/* The a.out tools do not support "linkonce" sections. */
#undef SUPPORTS_ONE_ONLY
#define SUPPORTS_ONE_ONLY	TARGET_ELF

/* The a.out tools do not support "Lscope" .stabs symbols. */
#undef NO_DBX_FUNCTION_END
#define NO_DBX_FUNCTION_END	TARGET_AOUT

/* A C statement (sans semicolon) to output an element in the table of
   global constructors.  */
#undef ASM_OUTPUT_CONSTRUCTOR
#define ASM_OUTPUT_CONSTRUCTOR(FILE, NAME)				\
  do {									\
    if (TARGET_ELF) {							\
      ctors_section ();							\
      fprintf ((FILE), "\t%s\t ", INT_ASM_OP);				\
      assemble_name ((FILE), (NAME));					\
      fprintf ((FILE), "\n");						\
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
#define ASM_OUTPUT_DESTRUCTOR(FILE, NAME)				\
  do {									\
    if (TARGET_ELF) {							\
      dtors_section ();							\
      fprintf ((FILE), "\t%s\t ", INT_ASM_OP);				\
      assemble_name ((FILE), (NAME));					\
      fprintf ((FILE), "\n");						\
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
    ASM_OUTPUT_ALIGNED_COMMON ((FILE), (NAME), (SIZE), (ALIGN));	\
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
#define ASM_OUTPUT_SKIP(FILE, SIZE) 					\
do {									\
  if (TARGET_ELF) {							\
    fprintf ((FILE), "\t%s\t%u\n", SKIP_ASM_OP, (SIZE));		\
  } else {								\
    fprintf ((FILE), "\t.space %u\n", (SIZE));				\
  }									\
} while (0)

#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(FILE, LINE)				\
do {									\
  static int sym_lineno = 1;						\
  if (TARGET_ELF) {							\
    fprintf ((FILE), ".stabn 68,0,%d,.LM%d-", (LINE), sym_lineno);	\
    assemble_name ((FILE), XSTR (XEXP (DECL_RTL (current_function_decl),\
                   0), 0));						\
    fprintf ((FILE), "\n.LM%d:\n", sym_lineno);				\
    sym_lineno += 1;							\
  } else {								\
    fprintf ((FILE), "\t%s %d,0,%d\n", ASM_STABD_OP, N_SLINE, lineno);	\
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
    fprintf ((FILE), "\t.text\n\t.stabs \"\",%d,0,0,.Letext\n.Letext:\n", \
             N_SO);							\
  }									\
} while (0)

/* stabs-in-elf has offsets relative to function beginning */
#undef DBX_OUTPUT_LBRAC
#define DBX_OUTPUT_LBRAC(FILE, NAME)					\
do {									\
  fprintf (asmfile, "%s %d,0,0,", ASM_STABN_OP, N_LBRAC);		\
  assemble_name (asmfile, buf);						\
  if (TARGET_ELF) {							\
    fputc ('-', asmfile);						\
    assemble_name (asmfile, XSTR (XEXP (DECL_RTL (current_function_decl),\
                   0), 0));						\
  }									\
  fprintf (asmfile, "\n");						\
} while (0)

#undef DBX_OUTPUT_RBRAC
#define DBX_OUTPUT_RBRAC(FILE, NAME)					\
do {									\
  fprintf (asmfile, "%s %d,0,0,", ASM_STABN_OP, N_RBRAC);		\
  assemble_name (asmfile, buf);						\
  if (TARGET_ELF) {							\
    fputc ('-', asmfile);						\
    assemble_name (asmfile, XSTR (XEXP (DECL_RTL (current_function_decl),\
                   0), 0));						\
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
#undef JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION (flag_pic)

/* override the exception table positioning */
#undef EXCEPTION_SECTION
#define EXCEPTION_SECTION() \
do {									\
  if (TARGET_ELF) {							\
    named_section (NULL_TREE, ".gcc_except_table", 0);			\
  } else {								\
    if (flag_pic)							\
      data_section ();							\
    else								\
      readonly_data_section ();						\
  }									\
} while (0);

/* supply our own hook for calling __main() from main() */
#undef GEN_CALL__MAIN
#define GEN_CALL__MAIN \
  do {									\
    if (!(TARGET_ELF))							\
      emit_library_call (gen_rtx (SYMBOL_REF, Pmode, NAME__MAIN), 0,	\
			 VOIDmode, 0);					\
  } while (0)

/* Copy this from the svr4 specifications... */
/* Define the register numbers to be used in Dwarf debugging information.
   The SVR4 reference port C compiler uses the following register numbers
   in its Dwarf output code:
	0 for %eax (gnu regno = 0)
	1 for %ecx (gnu regno = 2)
	2 for %edx (gnu regno = 1)
	3 for %ebx (gnu regno = 3)
	4 for %esp (gnu regno = 7)
	5 for %ebp (gnu regno = 6)
	6 for %esi (gnu regno = 4)
	7 for %edi (gnu regno = 5)
   The following three DWARF register numbers are never generated by
   the SVR4 C compiler or by the GNU compilers, but SDB on x86/svr4
   believes these numbers have these meanings.
	8  for %eip    (no gnu equivalent)
	9  for %eflags (no gnu equivalent)
	10 for %trapno (no gnu equivalent)
   It is not at all clear how we should number the FP stack registers
   for the x86 architecture.  If the version of SDB on x86/svr4 were
   a bit less brain dead with respect to floating-point then we would
   have a precedent to follow with respect to DWARF register numbers
   for x86 FP registers, but the SDB on x86/svr4 is so completely
   broken with respect to FP registers that it is hardly worth thinking
   of it as something to strive for compatibility with.
   The version of x86/svr4 SDB I have at the moment does (partially)
   seem to believe that DWARF register number 11 is associated with
   the x86 register %st(0), but that's about all.  Higher DWARF
   register numbers don't seem to be associated with anything in
   particular, and even for DWARF regno 11, SDB only seems to under-
   stand that it should say that a variable lives in %st(0) (when
   asked via an `=' command) if we said it was in DWARF regno 11,
   but SDB still prints garbage when asked for the value of the
   variable in question (via a `/' command).
   (Also note that the labels SDB prints for various FP stack regs
   when doing an `x' command are all wrong.)
   Note that these problems generally don't affect the native SVR4
   C compiler because it doesn't allow the use of -O with -g and
   because when it is *not* optimizing, it allocates a memory
   location for each floating-point variable, and the memory
   location is what gets described in the DWARF AT_location
   attribute for the variable in question.
   Regardless of the severe mental illness of the x86/svr4 SDB, we
   do something sensible here and we use the following DWARF
   register numbers.  Note that these are all stack-top-relative
   numbers.
	11 for %st(0) (gnu regno = 8)
	12 for %st(1) (gnu regno = 9)
	13 for %st(2) (gnu regno = 10)
	14 for %st(3) (gnu regno = 11)
	15 for %st(4) (gnu regno = 12)
	16 for %st(5) (gnu regno = 13)
	17 for %st(6) (gnu regno = 14)
	18 for %st(7) (gnu regno = 15)
*/
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
#undef STABS_DBX_REGISTER_NUMBER
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
    {									\
      fprintf ((FILE), "\tcall *%s@GOT(%%ebx)\n",			\
      TARGET_AOUT ? "mcount" : ".mcount");				\
    }									\
  else									\
    {									\
      fprintf ((FILE), "\tcall %s\n", TARGET_AOUT ? "mcount" : ".mcount"); \
    }									\
}

#undef FUNCTION_PROFILER_EPILOGUE
#define FUNCTION_PROFILER_EPILOGUE(FILE)  \
{									\
  if (TARGET_PROFILER_EPILOGUE)						\
    {									\
      if (flag_pic)							\
	fprintf ((FILE), "\tcall *%s@GOT(%%ebx)\n",			\
	  TARGET_AOUT ? "mexitcount" : ".mexitcount");			\
      else								\
	fprintf ((FILE), "\tcall %s\n",					\
	  TARGET_AOUT ? "mexitcount" : ".mexitcount");			\
    }									\
}

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"
 
#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "int"
 
#undef WCHAR_UNSIGNED
#define WCHAR_UNSIGNED 0

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Di386 -Acpu(i386) -Amachine(i386)" CPP_FBSD_PREDEFINES

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

/* Provide a LINK_SPEC appropriate for FreeBSD.  Here we provide support
   for the special GCC options -static and -shared, which allow us to
   link things in one of these three modes by applying the appropriate
   combinations of options at link-time. We like to support here for
   as many of the other GNU linker options as possible. But I don't
   have the time to search for those flags. I am sure how to add
   support for -soname shared_object_name. H.J.

   I took out %{v:%{!V:-V}}. It is too much :-(. They can use
   -Wl,-V.

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

/* A C statement to output to the stdio stream FILE an assembler
   command to advance the location counter to a multiple of 1<<LOG
   bytes if it is within MAX_SKIP bytes.

   This is used to align code labels according to Intel recommendations.  */

#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
#error "we don't have this for the aout gas"
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE, LOG, MAX_SKIP) 			\
  if ((LOG)!=0)								\
    if ((MAX_SKIP)==0) fprintf ((FILE), "\t.p2align %d\n", (LOG));	\
    else fprintf ((FILE), "\t.p2align %d,,%d\n", (LOG), (MAX_SKIP))
#endif

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

/* FreeBSD ELF using our home-grown crtbegin.o/crtend.o does not support the
   DWARF2 unwinding mechanisms.  Once `make world' bootstraping problems with
   the EGCS crtstuff.c is overcome, we will switch to the non-sjlj-exceptions 
   type exception machanism.  */
#define DWARF2_UNWIND_INFO 0
