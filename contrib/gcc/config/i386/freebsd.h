/* Definitions for Intel 386 running FreeBSD with either a.out or ELF format
   Copyright (C) 1996, 2000, 2002 Free Software Foundation, Inc.
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

#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (i386 FreeBSD/ELF)");

#undef  CC1_SPEC
#define CC1_SPEC "%(cc1_cpu) %{profile:-p} \
  %{gline:%{!g:%{!g0:%{!g1:%{!g2: -g1}}}}} \
  %{maout: %{!mno-underscores: %{!munderscores: -munderscores }}}"

#undef  ASM_SPEC
#define ASM_SPEC	"%{v*: -v} %{maout: %{fpic:-k} %{fPIC:-k}}"

#undef  ASM_FINAL_SPEC
#define ASM_FINAL_SPEC	"%|"

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
    %{Wl,*:%*} \
    %{v:-V} \
    %{assert*} %{R*} %{rpath*} %{defsym*} \
    %{shared:-Bshareable %{h*} %{soname*}} \
    %{!shared: \
      %{!static: \
	%{rdynamic: -export-dynamic} \
	%{!dynamic-linker: -dynamic-linker /usr/libexec/ld-elf.so.1}} \
      %{static:-Bstatic}} \
    %{symbolic:-Bsymbolic}}"

/* Provide a STARTFILE_SPEC appropriate for FreeBSD.  Here we add the magical
   crtbegin.o file (see crtstuff.c) which provides part of the support for
   getting C++ file-scope static object constructed before entering `main'.  */

#undef STARTFILE_SPEC
#define STARTFILE_SPEC "\
  %{maout: %{shared:c++rt0.o%s} \
    %{!shared: \
      %{pg:gcrt0.o%s}%{!pg: \
	%{static:scrt0.o%s} \
	%{!static:crt0.o%s}}}} \
  %{!maout: \
    %{!shared: \
      %{pg:gcrt1.o%s} \
      %{!pg: \
	%{p:gcrt1.o%s} \
	%{!p:crt1.o%s}}} \
    crti.o%s \
    %{!shared:crtbegin.o%s} \
    %{shared:crtbeginS.o%s}}"

/* Provide an ENDFILE_SPEC appropriate for FreeBSD/i386.  Here we tack on our
   own magical crtend.o file (see crtstuff.c) which provides part of the
   support for getting C++ file-scope static object constructed before
   entering `main', followed by the normal "finalizer" file, `crtn.o'.  */

#undef  ENDFILE_SPEC
#define ENDFILE_SPEC "\
  %{!maout: \
    %{!shared:crtend.o%s} \
    %{shared:crtendS.o%s} crtn.o%s}"


/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

#undef  SIZE_TYPE
#define SIZE_TYPE	"unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE	"int"

#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE	BITS_PER_WORD

#undef  TARGET_VERSION
#define TARGET_VERSION	fprintf (stderr, " (i386 FreeBSD/ELF)");

#define MASK_PROFILER_EPILOGUE	010000000000
#define MASK_AOUT		004000000000	/* a.out not elf */
#define MASK_UNDERSCORES	002000000000	/* use leading _ */

#define TARGET_PROFILER_EPILOGUE	(target_flags & MASK_PROFILER_EPILOGUE)
#define TARGET_AOUT			(target_flags & MASK_AOUT)
#define TARGET_ELF			((target_flags & MASK_AOUT) == 0)
#define TARGET_UNDERSCORES		((target_flags & MASK_UNDERSCORES) != 0)

#undef	SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
  { "profiler-epilogue",	 MASK_PROFILER_EPILOGUE, "Function profiler epilogue"}, \
  { "no-profiler-epilogue",	-MASK_PROFILER_EPILOGUE, "No function profiler epilogue"}, \
  { "aout",			 MASK_AOUT, "Generate an a.out (vs. ELF) binary"}, \
  { "no-aout",			-MASK_AOUT, "Do not generate an a.out binary"}, \
  { "underscores",		 MASK_UNDERSCORES, "Add leading underscores to symbols"}, \
  { "no-underscores",		-MASK_UNDERSCORES, "Do not add leading underscores to symbols"},

/* This goes away when the math emulator is fixed.  */
#undef  TARGET_SUBTARGET_DEFAULT
#define TARGET_SUBTARGET_DEFAULT \
  (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS | MASK_NO_FANCY_MATH_387)

/* Don't default to pcc-struct-return, we want to retain compatibility with
   older gcc versions AND pcc-struct-return is nonreentrant.
   (even though the SVR4 ABI for the i386 says that records and unions are
   returned in memory).  */

#undef  DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* The a.out tools do not support "linkonce" sections. */
#undef  SUPPORTS_ONE_ONLY
#define SUPPORTS_ONE_ONLY	TARGET_ELF

/* Prefix for internally generated assembler labels.  If we aren't using
   underscores, we are using prefix `.'s to identify labels that should
   be ignored, as in `i386/gas.h' --karl@cs.umb.edu  */
#undef  LPREFIX
#define LPREFIX ((TARGET_UNDERSCORES) ? "L" : ".L")

/* supply our own hook for calling __main() from main() */
#undef  INVOKE__main
#define INVOKE__main
#undef  GEN_CALL__MAIN
#define GEN_CALL__MAIN							\
  do {									\
    if (!(TARGET_ELF))							\
      emit_library_call (gen_rtx (SYMBOL_REF, Pmode, NAME__MAIN), 0,	\
			 VOIDmode, 0);					\
  } while (0)

/* Tell final.c that we don't need a label passed to mcount.  */
#define NO_PROFILE_COUNTERS	1

/* Output assembler code to FILE to begin profiling of the current function.
   LABELNO is an optional label.  */

#undef  FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)  \
  do {									\
    char *_name = TARGET_AOUT ? "mcount" : ".mcount";			\
    if (flag_pic)							\
      fprintf ((FILE), "\tcall *%s@GOT(%%ebx)\n", _name);		\
    else								\
      fprintf ((FILE), "\tcall %s\n", _name);				\
  } while (0)

/* Output assembler code to FILE to end profiling of the current function.  */

#undef  FUNCTION_PROFILER_EPILOGUE	/* BDE will need to fix this. */


/************************[  Assembler stuff  ]********************************/

/* Override the default comment-starter of "/" from unix.h.  */
#undef  ASM_COMMENT_START
#define ASM_COMMENT_START "#"

/* Override the default comment-starter of "/APP" from unix.h.  */
#undef  ASM_APP_ON
#define ASM_APP_ON	"#APP\n"
#undef  ASM_APP_OFF
#define ASM_APP_OFF	"#NO_APP\n"

/* This is how to store into the string BUF
   the symbol_ref name of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   This is suitable for output with `assemble_name'.  */
#undef	ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL, PREFIX, NUM)			\
  sprintf ((LABEL), "*%s%s%u", (TARGET_UNDERSCORES) ? "" : ".",		\
	   (PREFIX), (unsigned) (NUM))

/* This is how to output an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   For most svr4/ELF systems, the convention is that any symbol which begins
   with a period is not put into the linker symbol table by the assembler.  */
#undef	ASM_OUTPUT_INTERNAL_LABEL
#define	ASM_OUTPUT_INTERNAL_LABEL(FILE, PREFIX, NUM)			\
  fprintf ((FILE), "%s%s%u:\n", (TARGET_UNDERSCORES) ? "" : ".",	\
	   (PREFIX), (unsigned) (NUM))

/* This is how to output a reference to a user-level label named NAME.  */
#undef  ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE, NAME)					\
  do {									\
    char *_name = (NAME);						\
    /* Hack to avoid writing lots of rtl in				\
       FUNCTION_PROFILER_EPILOGUE ().  */				\
    if (*_name == '.' && strcmp(_name + 1, "mexitcount") == 0)		\
      {									\
	if (TARGET_AOUT)						\
	  _name++;							\
	if (flag_pic)							\
	  fprintf ((FILE), "*%s@GOT(%%ebx)", _name);			\
	else								\
	  fprintf ((FILE), "%s", _name);				\
      }									\
    else								\
      fprintf (FILE, "%s%s", TARGET_UNDERSCORES ? "_" : "", _name);	\
} while (0)

/* This is how to hack on the symbol code of certain relcalcitrant
   symbols to modify their output in output_pic_addr_const ().  */

#undef  ASM_HACK_SYMBOLREF_CODE	/* BDE will need to fix this. */

#undef  ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE, LOG)      				\
  if ((LOG)!=0) {							\
    if (in_text_section())						\
      fprintf ((FILE), "\t.p2align %d,0x90\n", (LOG));			\
    else								\
      fprintf ((FILE), "\t.p2align %d\n", (LOG));			\
  }

#undef  ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGN)		\
  do {									\
    if (TARGET_ELF)							\
      {									\
	fprintf ((FILE), "%s", COMMON_ASM_OP);				\
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
	fprintf ((FILE), "%s ", COMMON_ASM_OP);				\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), ",%u\n", (rounded));				\
      }									\
  } while (0)

/* This says how to output assembler code to declare an
   uninitialized internal linkage data object.  Under SVR4,
   the linker seems to want the alignment of data objects
   to depend on their types.  We do exactly that here.  */

#undef  ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN)		\
  do {									\
    if (TARGET_ELF)							\
      {									\
	fprintf ((FILE), "%s", LOCAL_ASM_OP);				\
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
	fputs ("\t.lcomm\t", (FILE));					\
	assemble_name ((FILE), (NAME));					\
	fprintf ((FILE), ",%u\n", (rounded));				\
      }									\
  } while (0)

/* How to output some space.  The rules are different depending on the
   object format.  */
#undef  ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE, SIZE) 					\
  do {									\
    if (TARGET_ELF)							\
      {									\
        fprintf ((FILE), "%s%u\n", SKIP_ASM_OP, (SIZE));		\
      }									\
    else								\
      {									\
        fprintf ((FILE), "\t.space\t%u\n", (SIZE));			\
      }									\
  } while (0)

#undef  ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(FILE, LINE)				\
  do {									\
    static int sym_lineno = 1;						\
    if (TARGET_ELF)							\
      {									\
	fprintf ((FILE), ".stabn 68,0,%d,.LM%d-", (LINE), sym_lineno);	\
	assemble_name ((FILE), 						\
		XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));	\
	fprintf ((FILE), "\n.LM%d:\n", sym_lineno);			\
	sym_lineno += 1;						\
      }									\
    else								\
      {									\
	fprintf ((FILE), "\t%s %d,0,%d\n", ASM_STABD_OP, N_SLINE,	\
		lineno);						\
      }									\
  } while (0)

/* A C statement to output to the stdio stream FILE an assembler
   command to advance the location counter to a multiple of 1<<LOG
   bytes if it is within MAX_SKIP bytes.

   This is used to align code labels according to Intel recommendations.  */

#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE, LOG, MAX_SKIP)					\
  if ((LOG) != 0) {														\
    if ((MAX_SKIP) == 0) fprintf ((FILE), "\t.p2align %d\n", (LOG));	\
    else fprintf ((FILE), "\t.p2align %d,,%d\n", (LOG), (MAX_SKIP));	\
  }
#endif


/************************[  Debugger stuff  ]*********************************/

/* The a.out tools do not support "Lscope" .stabs symbols. */
#undef  NO_DBX_FUNCTION_END
#define NO_DBX_FUNCTION_END	TARGET_AOUT

/* In ELF, the function stabs come first, before the relative offsets.  */
#undef  DBX_FUNCTION_FIRST
#define DBX_CHECK_FUNCTION_FIRST TARGET_ELF

#undef  DBX_REGISTER_NUMBER
#define DBX_REGISTER_NUMBER(n)	(TARGET_64BIT ? dbx64_register_map[n]	\
				: (write_symbols == DWARF2_DEBUG	\
	    			  || write_symbols == DWARF_DEBUG)	\
				  ? svr4_dbx_register_map[(n)]		\
				  : dbx_register_map[(n)])

/* tag end of file in elf mode */
#undef  DBX_OUTPUT_MAIN_SOURCE_FILE_END
#define DBX_OUTPUT_MAIN_SOURCE_FILE_END(FILE, FILENAME)			\
  do {									\
    if (TARGET_ELF) {							\
      fprintf ((FILE), "\t.text\n\t.stabs \"\",%d,0,0,%LLetext\n%LLetext:\n", \
		N_SO);							\
    }									\
  } while (0)

/* stabs-in-elf has offsets relative to function beginning */
#undef  DBX_OUTPUT_LBRAC
#define DBX_OUTPUT_LBRAC(FILE, NAME)					\
  do {									\
    fprintf (asmfile, "%s %d,0,0,", ASM_STABN_OP, N_LBRAC);		\
    assemble_name (asmfile, buf);					\
    if (TARGET_ELF)							\
      {									\
        fputc ('-', asmfile);						\
        assemble_name (asmfile,						\
	      	 XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));	\
      }									\
    fprintf (asmfile, "\n");						\
  } while (0)

#undef  DBX_OUTPUT_RBRAC
#define DBX_OUTPUT_RBRAC(FILE, NAME)					\
  do {									\
    fprintf (asmfile, "%s %d,0,0,", ASM_STABN_OP, N_RBRAC);		\
    assemble_name (asmfile, buf);					\
    if (TARGET_ELF)							\
      {									\
        fputc ('-', asmfile);						\
        assemble_name (asmfile,						\
		 XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));	\
      }									\
    fprintf (asmfile, "\n");						\
  } while (0)
