/* Definitions for Intel 386 running Interactive Unix System V,
   producing stabs-in-coff output (using a slightly modified gas).
   Specifically, this is for recent versions that support POSIX;
   for version 2.0.2, use configuration option i386-sysv instead.  */

/* Underscores are not used on ISC systems (probably not on any COFF
   system), despite the comments in i386/gas.h.  If this is not defined,
   enquire (for example) will fail to link.  --karl@cs.umb.edu  */
#define NO_UNDERSCORES

/* Mostly like other gas-using systems.  */
#include "i386/gas.h"

/* But with ISC-specific additions.  */
#include "i386/isc.h"

/* We do not want to output SDB debugging information.  */

#undef SDB_DEBUGGING_INFO

/* We want to output DBX debugging information.  */

#define DBX_DEBUGGING_INFO


/* The function `dbxout_init' in dbxout.c omits the first character of
   `ltext_label_name' when outputting the main source directory and main
   source filename.  I don't understand why, but rather than making a
   system-independent change there, I override dbxout.c's defaults.
   Perhaps it would be better to use ".Ltext0" instead of
   `ltext_label_name', but we've already generated the label, so we just
   use it here.  --karl@cs.umb.edu  */
#define DBX_OUTPUT_MAIN_SOURCE_DIRECTORY(asmfile, cwd)			\
  do {	fprintf (asmfile, "%s ", ASM_STABS_OP);				\
	output_quoted_string (asmfile, cwd);				\
	fprintf (asmfile, ",%d,0,0,%s\n", N_SO, ltext_label_name);	\
  } while (0)
#define DBX_OUTPUT_MAIN_SOURCE_FILENAME(asmfile, input_file_name)	\
  fprintf (asmfile, "%s ", ASM_STABS_OP);				\
  output_quoted_string (input_file_name);				\
  fprintf (asmfile, ",%d,0,0,%s\n", N_SO, ltext_label_name);		\
  text_section ();							\
  ASM_OUTPUT_INTERNAL_LABEL (asmfile, "Ltext", 0)


/* Because we don't include `svr3.h', we haven't yet defined SIZE_TYPE
   and PTRDIFF_TYPE.  ISC's definitions don't match GCC's defaults, so: */

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"


/* But we can't use crtbegin.o and crtend.o, because gas 1.38.1 doesn't
   grok .section.  The definitions here are otherwise identical to those
   in i386/isc.h.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC \
  "%{!shlib:%{posix:%{pg:mcrtp1.o%s}%{!pg:%{p:mcrtp1.o%s}%{!p:crtp1.o%s}}}\
   %{!posix:%{pg:mcrt1.o%s}%{!pg:%{p:mcrt1.o%s}%{!p:crt1.o%s}}\
   %{p:-L/lib/libp} %{pg:-L/lib/libp}}}\
   %{shlib:%{posix:crtp1.o%s}%{!posix:crt1.o%s}}"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC "crtn.o%s"
