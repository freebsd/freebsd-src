/* Parse options for the GNU linker.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002
   Free Software Foundation, Inc.

This file is part of GLD, the Gnu Linker.

GLD is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GLD is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GLD; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libiberty.h"
#include <stdio.h>
#include <string.h>
#include "safe-ctype.h"
#include "getopt.h"
#include "bfdlink.h"
#include "ld.h"
#include "ldmain.h"
#include "ldmisc.h"
#include "ldexp.h"
#include "ldlang.h"
#include "ldgram.h"
#include "ldlex.h"
#include "ldfile.h"
#include "ldver.h"
#include "ldemul.h"
#include "demangle.h"

#ifndef PATH_SEPARATOR
#if defined (__MSDOS__) || (defined (_WIN32) && ! defined (__CYGWIN32__))
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif
#endif

/* Somewhere above, sys/stat.h got included . . . .  */
#if !defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

static int is_num PARAMS ((const char *, int, int, int));
static void set_default_dirlist PARAMS ((char *dirlist_ptr));
static void set_section_start PARAMS ((char *sect, char *valstr));
static void help PARAMS ((void));

/* Non-zero if we are processing a --defsym from the command line.  */
int parsing_defsym = 0;

/* Codes used for the long options with no short synonyms.  150 isn't
   special; it's just an arbitrary non-ASCII char value.  */

#define OPTION_ASSERT			150
#define OPTION_CALL_SHARED		(OPTION_ASSERT + 1)
#define OPTION_CREF			(OPTION_CALL_SHARED + 1)
#define OPTION_DEFSYM			(OPTION_CREF + 1)
#define OPTION_DEMANGLE			(OPTION_DEFSYM + 1)
#define OPTION_DYNAMIC_LINKER		(OPTION_DEMANGLE + 1)
#define OPTION_EB			(OPTION_DYNAMIC_LINKER + 1)
#define OPTION_EL			(OPTION_EB + 1)
#define OPTION_EMBEDDED_RELOCS		(OPTION_EL + 1)
#define OPTION_EXPORT_DYNAMIC		(OPTION_EMBEDDED_RELOCS + 1)
#define OPTION_HELP			(OPTION_EXPORT_DYNAMIC + 1)
#define OPTION_IGNORE			(OPTION_HELP + 1)
#define OPTION_MAP			(OPTION_IGNORE + 1)
#define OPTION_NO_DEMANGLE		(OPTION_MAP + 1)
#define OPTION_NO_KEEP_MEMORY		(OPTION_NO_DEMANGLE + 1)
#define OPTION_NO_WARN_MISMATCH		(OPTION_NO_KEEP_MEMORY + 1)
#define OPTION_NOINHIBIT_EXEC		(OPTION_NO_WARN_MISMATCH + 1)
#define OPTION_NON_SHARED		(OPTION_NOINHIBIT_EXEC + 1)
#define OPTION_NO_WHOLE_ARCHIVE		(OPTION_NON_SHARED + 1)
#define OPTION_OFORMAT			(OPTION_NO_WHOLE_ARCHIVE + 1)
#define OPTION_RELAX			(OPTION_OFORMAT + 1)
#define OPTION_RETAIN_SYMBOLS_FILE	(OPTION_RELAX + 1)
#define OPTION_RPATH			(OPTION_RETAIN_SYMBOLS_FILE + 1)
#define OPTION_RPATH_LINK		(OPTION_RPATH + 1)
#define OPTION_SHARED			(OPTION_RPATH_LINK + 1)
#define OPTION_SONAME			(OPTION_SHARED + 1)
#define OPTION_SORT_COMMON		(OPTION_SONAME + 1)
#define OPTION_STATS			(OPTION_SORT_COMMON + 1)
#define OPTION_SYMBOLIC			(OPTION_STATS + 1)
#define OPTION_TASK_LINK		(OPTION_SYMBOLIC + 1)
#define OPTION_TBSS			(OPTION_TASK_LINK + 1)
#define OPTION_TDATA			(OPTION_TBSS + 1)
#define OPTION_TTEXT			(OPTION_TDATA + 1)
#define OPTION_TRADITIONAL_FORMAT	(OPTION_TTEXT + 1)
#define OPTION_UR			(OPTION_TRADITIONAL_FORMAT + 1)
#define OPTION_VERBOSE			(OPTION_UR + 1)
#define OPTION_VERSION			(OPTION_VERBOSE + 1)
#define OPTION_VERSION_SCRIPT		(OPTION_VERSION + 1)
#define OPTION_VERSION_EXPORTS_SECTION	(OPTION_VERSION_SCRIPT + 1)
#define OPTION_WARN_COMMON		(OPTION_VERSION_EXPORTS_SECTION + 1)
#define OPTION_WARN_CONSTRUCTORS	(OPTION_WARN_COMMON + 1)
#define OPTION_WARN_FATAL		(OPTION_WARN_CONSTRUCTORS + 1)
#define OPTION_WARN_MULTIPLE_GP		(OPTION_WARN_FATAL + 1)
#define OPTION_WARN_ONCE		(OPTION_WARN_MULTIPLE_GP + 1)
#define OPTION_WARN_SECTION_ALIGN	(OPTION_WARN_ONCE + 1)
#define OPTION_SPLIT_BY_RELOC		(OPTION_WARN_SECTION_ALIGN + 1)
#define OPTION_SPLIT_BY_FILE 	    	(OPTION_SPLIT_BY_RELOC + 1)
#define OPTION_WHOLE_ARCHIVE		(OPTION_SPLIT_BY_FILE + 1)
#define OPTION_WRAP			(OPTION_WHOLE_ARCHIVE + 1)
#define OPTION_FORCE_EXE_SUFFIX		(OPTION_WRAP + 1)
#define OPTION_GC_SECTIONS		(OPTION_FORCE_EXE_SUFFIX + 1)
#define OPTION_NO_GC_SECTIONS		(OPTION_GC_SECTIONS + 1)
#define OPTION_CHECK_SECTIONS		(OPTION_NO_GC_SECTIONS + 1)
#define OPTION_NO_CHECK_SECTIONS	(OPTION_CHECK_SECTIONS + 1)
#define OPTION_MPC860C0                 (OPTION_NO_CHECK_SECTIONS + 1)
#define OPTION_NO_UNDEFINED		(OPTION_MPC860C0 + 1)
#define OPTION_INIT                     (OPTION_NO_UNDEFINED + 1)
#define OPTION_FINI                     (OPTION_INIT + 1)
#define OPTION_SECTION_START		(OPTION_FINI + 1)
#define OPTION_UNIQUE			(OPTION_SECTION_START + 1)
#define OPTION_TARGET_HELP              (OPTION_UNIQUE + 1)
#define OPTION_ALLOW_SHLIB_UNDEFINED	(OPTION_TARGET_HELP + 1)
#define OPTION_ALLOW_MULTIPLE_DEFINITION (OPTION_ALLOW_SHLIB_UNDEFINED + 1)
#define OPTION_NO_UNDEFINED_VERSION	(OPTION_ALLOW_MULTIPLE_DEFINITION + 1)
#define OPTION_DISCARD_NONE		(OPTION_NO_UNDEFINED_VERSION + 1)
#define OPTION_SPARE_DYNAMIC_TAGS	(OPTION_DISCARD_NONE + 1)
#define OPTION_NO_DEFINE_COMMON		(OPTION_SPARE_DYNAMIC_TAGS + 1)
#define OPTION_NOSTDLIB			(OPTION_NO_DEFINE_COMMON + 1)

/* The long options.  This structure is used for both the option
   parsing and the help text.  */

struct ld_option
{
  /* The long option information.  */
  struct option opt;
  /* The short option with the same meaning ('\0' if none).  */
  char shortopt;
  /* The name of the argument (NULL if none).  */
  const char *arg;
  /* The documentation string.  If this is NULL, this is a synonym for
     the previous option.  */
  const char *doc;
  enum {
    /* Use one dash before long option name.  */
    ONE_DASH,
    /* Use two dashes before long option name.  */
    TWO_DASHES,
    /* Only accept two dashes before the long option name.
       This is an overloading of the use of this enum, since originally it
       was only intended to tell the --help display function how to display
       the long option name.  This feature was added in order to resolve
       the confusion about the -omagic command line switch.  Is it setting
       the output file name to "magic" or is it setting the NMAGIC flag on
       the output ?  It has been decided that it is setting the output file
       name, and that if you want to set the NMAGIC flag you should use -N
       or --omagic.  */
    EXACTLY_TWO_DASHES,
    /* Don't mention this option in --help output.  */
    NO_HELP
  } control;
};

static const struct ld_option ld_options[] =
{
  { {NULL, required_argument, NULL, '\0'},
      'a', N_("KEYWORD"), N_("Shared library control for HP/UX compatibility"),
      ONE_DASH },
  { {"architecture", required_argument, NULL, 'A'},
      'A', N_("ARCH"), N_("Set architecture") , TWO_DASHES },
  { {"format", required_argument, NULL, 'b'},
      'b', N_("TARGET"), N_("Specify target for following input files"), TWO_DASHES },
  { {"mri-script", required_argument, NULL, 'c'},
      'c', N_("FILE"), N_("Read MRI format linker script"), TWO_DASHES },
  { {"dc", no_argument, NULL, 'd'},
      'd', NULL, N_("Force common symbols to be defined"), ONE_DASH },
  { {"dp", no_argument, NULL, 'd'},
      '\0', NULL, NULL, ONE_DASH },
  { {"entry", required_argument, NULL, 'e'},
      'e', N_("ADDRESS"), N_("Set start address"), TWO_DASHES },
  { {"export-dynamic", no_argument, NULL, OPTION_EXPORT_DYNAMIC},
      'E', NULL, N_("Export all dynamic symbols"), TWO_DASHES },
  { {"EB", no_argument, NULL, OPTION_EB},
      '\0', NULL, N_("Link big-endian objects"), ONE_DASH },
  { {"EL", no_argument, NULL, OPTION_EL},
      '\0', NULL, N_("Link little-endian objects"), ONE_DASH },
  { {"auxiliary", required_argument, NULL, 'f'},
      'f', N_("SHLIB"), N_("Auxiliary filter for shared object symbol table"),
      TWO_DASHES },
  { {"filter", required_argument, NULL, 'F'},
      'F', N_("SHLIB"), N_("Filter for shared object symbol table"), TWO_DASHES },
  { {NULL, no_argument, NULL, '\0'},
      'g', NULL, N_("Ignored"), ONE_DASH },
  { {"gpsize", required_argument, NULL, 'G'},
      'G', N_("SIZE"), N_("Small data size (if no size, same as --shared)"),
      TWO_DASHES },
  { {"soname", required_argument, NULL, OPTION_SONAME},
      'h', N_("FILENAME"), N_("Set internal name of shared library"), ONE_DASH },
  { {"dynamic-linker", required_argument, NULL, OPTION_DYNAMIC_LINKER},
      'I', N_("PROGRAM"), N_("Set PROGRAM as the dynamic linker to use"), TWO_DASHES },
  { {"library", required_argument, NULL, 'l'},
      'l', N_("LIBNAME"), N_("Search for library LIBNAME"), TWO_DASHES },
  { {"library-path", required_argument, NULL, 'L'},
      'L', N_("DIRECTORY"), N_("Add DIRECTORY to library search path"), TWO_DASHES },
  { {NULL, required_argument, NULL, '\0'},
      'm', N_("EMULATION"), N_("Set emulation"), ONE_DASH },
  { {"print-map", no_argument, NULL, 'M'},
      'M', NULL, N_("Print map file on standard output"), TWO_DASHES },
  { {"nmagic", no_argument, NULL, 'n'},
      'n', NULL, N_("Do not page align data"), TWO_DASHES },
  { {"omagic", no_argument, NULL, 'N'},
      'N', NULL, N_("Do not page align data, do not make text readonly"),
      EXACTLY_TWO_DASHES },
  { {"output", required_argument, NULL, 'o'},
      'o', N_("FILE"), N_("Set output file name"), EXACTLY_TWO_DASHES },
  { {NULL, required_argument, NULL, '\0'},
      'O', NULL, N_("Optimize output file"), ONE_DASH },
  { {"Qy", no_argument, NULL, OPTION_IGNORE},
      '\0', NULL, N_("Ignored for SVR4 compatibility"), ONE_DASH },
  { {"emit-relocs", no_argument, NULL, 'q'},
      'q', NULL, "Generate relocations in final output", TWO_DASHES },
  { {"relocateable", no_argument, NULL, 'r'},
      'r', NULL, N_("Generate relocateable output"), TWO_DASHES },
  { {NULL, no_argument, NULL, '\0'},
      'i', NULL, NULL, ONE_DASH },
  { {"just-symbols", required_argument, NULL, 'R'},
      'R', N_("FILE"), N_("Just link symbols (if directory, same as --rpath)"),
      TWO_DASHES },
  { {"strip-all", no_argument, NULL, 's'},
      's', NULL, N_("Strip all symbols"), TWO_DASHES },
  { {"strip-debug", no_argument, NULL, 'S'},
      'S', NULL, N_("Strip debugging symbols"), TWO_DASHES },
  { {"trace", no_argument, NULL, 't'},
      't', NULL, N_("Trace file opens"), TWO_DASHES },
  { {"script", required_argument, NULL, 'T'},
      'T', N_("FILE"), N_("Read linker script"), TWO_DASHES },
  { {"undefined", required_argument, NULL, 'u'},
      'u', N_("SYMBOL"), N_("Start with undefined reference to SYMBOL"), TWO_DASHES },
  { {"unique", optional_argument, NULL, OPTION_UNIQUE},
      '\0', N_("[=SECTION]"), N_("Don't merge input [SECTION | orphan] sections"), TWO_DASHES },
  { {"Ur", no_argument, NULL, OPTION_UR},
      '\0', NULL, N_("Build global constructor/destructor tables"), ONE_DASH },
  { {"version", no_argument, NULL, OPTION_VERSION},
      'v', NULL, N_("Print version information"), TWO_DASHES },
  { {NULL, no_argument, NULL, '\0'},
      'V', NULL, N_("Print version and emulation information"), ONE_DASH },
  { {"discard-all", no_argument, NULL, 'x'},
      'x', NULL, N_("Discard all local symbols"), TWO_DASHES },
  { {"discard-locals", no_argument, NULL, 'X'},
      'X', NULL, N_("Discard temporary local symbols (default)"), TWO_DASHES },
  { {"discard-none", no_argument, NULL, OPTION_DISCARD_NONE},
      '\0', NULL, N_("Don't discard any local symbols"), TWO_DASHES },
  { {"trace-symbol", required_argument, NULL, 'y'},
      'y', N_("SYMBOL"), N_("Trace mentions of SYMBOL"), TWO_DASHES },
  { {NULL, required_argument, NULL, '\0'},
      'Y', N_("PATH"), N_("Default search path for Solaris compatibility"), ONE_DASH },
  { {"start-group", no_argument, NULL, '('},
      '(', NULL, N_("Start a group"), TWO_DASHES },
  { {"end-group", no_argument, NULL, ')'},
      ')', NULL, N_("End a group"), TWO_DASHES },
  { {"assert", required_argument, NULL, OPTION_ASSERT},
      '\0', N_("KEYWORD"), N_("Ignored for SunOS compatibility"), ONE_DASH },
  { {"Bdynamic", no_argument, NULL, OPTION_CALL_SHARED},
      '\0', NULL, N_("Link against shared libraries"), ONE_DASH },
  { {"dy", no_argument, NULL, OPTION_CALL_SHARED},
      '\0', NULL, NULL, ONE_DASH },
  { {"call_shared", no_argument, NULL, OPTION_CALL_SHARED},
      '\0', NULL, NULL, ONE_DASH },
  { {"Bstatic", no_argument, NULL, OPTION_NON_SHARED},
      '\0', NULL, N_("Do not link against shared libraries"), ONE_DASH },
  { {"dn", no_argument, NULL, OPTION_NON_SHARED},
      '\0', NULL, NULL, ONE_DASH },
  { {"non_shared", no_argument, NULL, OPTION_NON_SHARED},
      '\0', NULL, NULL, ONE_DASH },
  { {"static", no_argument, NULL, OPTION_NON_SHARED},
      '\0', NULL, NULL, ONE_DASH },
  { {"Bsymbolic", no_argument, NULL, OPTION_SYMBOLIC},
      '\0', NULL, N_("Bind global references locally"), ONE_DASH },
  { {"check-sections", no_argument, NULL, OPTION_CHECK_SECTIONS},
      '\0', NULL, N_("Check section addresses for overlaps (default)"), TWO_DASHES },
  { {"no-check-sections", no_argument, NULL, OPTION_NO_CHECK_SECTIONS},
      '\0', NULL, N_("Do not check section addresses for overlaps"),
      TWO_DASHES },
  { {"cref", no_argument, NULL, OPTION_CREF},
      '\0', NULL, N_("Output cross reference table"), TWO_DASHES },
  { {"defsym", required_argument, NULL, OPTION_DEFSYM},
      '\0', N_("SYMBOL=EXPRESSION"), N_("Define a symbol"), TWO_DASHES },
  { {"demangle", optional_argument, NULL, OPTION_DEMANGLE},
      '\0', N_("[=STYLE]"), N_("Demangle symbol names [using STYLE]"), TWO_DASHES },
  { {"embedded-relocs", no_argument, NULL, OPTION_EMBEDDED_RELOCS},
      '\0', NULL, N_("Generate embedded relocs"), TWO_DASHES},
  { {"fini", required_argument, NULL, OPTION_FINI},
     '\0', N_("SYMBOL"), N_("Call SYMBOL at unload-time"), ONE_DASH },
  { {"force-exe-suffix", no_argument, NULL, OPTION_FORCE_EXE_SUFFIX},
      '\0', NULL, N_("Force generation of file with .exe suffix"), TWO_DASHES},
  { {"gc-sections", no_argument, NULL, OPTION_GC_SECTIONS},
      '\0', NULL, N_("Remove unused sections (on some targets)"),
      TWO_DASHES },
  { {"no-gc-sections", no_argument, NULL, OPTION_NO_GC_SECTIONS},
      '\0', NULL, N_("Don't remove unused sections (default)"),
      TWO_DASHES },
  { {"help", no_argument, NULL, OPTION_HELP},
      '\0', NULL, N_("Print option help"), TWO_DASHES },
  { {"init", required_argument, NULL, OPTION_INIT},
     '\0', N_("SYMBOL"), N_("Call SYMBOL at load-time"), ONE_DASH },
  { {"Map", required_argument, NULL, OPTION_MAP},
      '\0', N_("FILE"), N_("Write a map file"), ONE_DASH },
  { {"no-define-common", no_argument, NULL, OPTION_NO_DEFINE_COMMON},
      '\0', NULL, N_("Do not define Common storage"), TWO_DASHES },
  { {"no-demangle", no_argument, NULL, OPTION_NO_DEMANGLE },
      '\0', NULL, N_("Do not demangle symbol names"), TWO_DASHES },
  { {"no-keep-memory", no_argument, NULL, OPTION_NO_KEEP_MEMORY},
      '\0', NULL, N_("Use less memory and more disk I/O"), TWO_DASHES },
  { {"no-undefined", no_argument, NULL, OPTION_NO_UNDEFINED},
     '\0', NULL, N_("Allow no undefined symbols"), TWO_DASHES },
  { {"allow-shlib-undefined", no_argument, NULL, OPTION_ALLOW_SHLIB_UNDEFINED},
     '\0', NULL, N_("Allow undefined symbols in shared objects"), TWO_DASHES },
  { {"allow-multiple-definition", no_argument, NULL, OPTION_ALLOW_MULTIPLE_DEFINITION},
     '\0', NULL, N_("Allow multiple definitions"), TWO_DASHES },
  { {"no-undefined-version", no_argument, NULL, OPTION_NO_UNDEFINED_VERSION},
     '\0', NULL, N_("Disallow undefined version"), TWO_DASHES },
  { {"no-warn-mismatch", no_argument, NULL, OPTION_NO_WARN_MISMATCH},
      '\0', NULL, N_("Don't warn about mismatched input files"), TWO_DASHES},
  { {"no-whole-archive", no_argument, NULL, OPTION_NO_WHOLE_ARCHIVE},
      '\0', NULL, N_("Turn off --whole-archive"), TWO_DASHES },
  { {"noinhibit-exec", no_argument, NULL, OPTION_NOINHIBIT_EXEC},
      '\0', NULL, N_("Create an output file even if errors occur"), TWO_DASHES },
  { {"noinhibit_exec", no_argument, NULL, OPTION_NOINHIBIT_EXEC},
      '\0', NULL, NULL, NO_HELP },
  { {"nostdlib", no_argument, NULL, OPTION_NOSTDLIB},
      '\0', NULL, N_("Only use library directories specified on\n\t\t\t\tthe command line"), ONE_DASH },
  { {"oformat", required_argument, NULL, OPTION_OFORMAT},
      '\0', N_("TARGET"), N_("Specify target of output file"), EXACTLY_TWO_DASHES },
  { {"qmagic", no_argument, NULL, OPTION_IGNORE},
      '\0', NULL, N_("Ignored for Linux compatibility"), ONE_DASH },
  { {"relax", no_argument, NULL, OPTION_RELAX},
      '\0', NULL, N_("Relax branches on certain targets"), TWO_DASHES },
  { {"retain-symbols-file", required_argument, NULL,
       OPTION_RETAIN_SYMBOLS_FILE},
      '\0', N_("FILE"), N_("Keep only symbols listed in FILE"), TWO_DASHES },
  { {"rpath", required_argument, NULL, OPTION_RPATH},
      '\0', N_("PATH"), N_("Set runtime shared library search path"), ONE_DASH },
  { {"rpath-link", required_argument, NULL, OPTION_RPATH_LINK},
      '\0', N_("PATH"), N_("Set link time shared library search path"), ONE_DASH },
  { {"shared", no_argument, NULL, OPTION_SHARED},
      '\0', NULL, N_("Create a shared library"), ONE_DASH },
  { {"Bshareable", no_argument, NULL, OPTION_SHARED }, /* FreeBSD.  */
      '\0', NULL, NULL, ONE_DASH },
  { {"sort-common", no_argument, NULL, OPTION_SORT_COMMON},
      '\0', NULL, N_("Sort common symbols by size"), TWO_DASHES },
  { {"sort_common", no_argument, NULL, OPTION_SORT_COMMON},
      '\0', NULL, NULL, NO_HELP },
  { {"spare-dynamic-tags", required_argument, NULL, OPTION_SPARE_DYNAMIC_TAGS},
      '\0', N_("COUNT"), N_("How many tags to reserve in .dynamic section"), TWO_DASHES },
  { {"split-by-file", optional_argument, NULL, OPTION_SPLIT_BY_FILE},
      '\0', N_("[=SIZE]"), N_("Split output sections every SIZE octets"), TWO_DASHES },
  { {"split-by-reloc", optional_argument, NULL, OPTION_SPLIT_BY_RELOC},
      '\0', N_("[=COUNT]"), N_("Split output sections every COUNT relocs"), TWO_DASHES },
  { {"stats", no_argument, NULL, OPTION_STATS},
      '\0', NULL, N_("Print memory usage statistics"), TWO_DASHES },
  { {"target-help", no_argument, NULL, OPTION_TARGET_HELP},
      '\0', NULL, N_("Display target specific options"), TWO_DASHES },
  { {"task-link", required_argument, NULL, OPTION_TASK_LINK},
      '\0', N_("SYMBOL"), N_("Do task level linking"), TWO_DASHES },
  { {"traditional-format", no_argument, NULL, OPTION_TRADITIONAL_FORMAT},
      '\0', NULL, N_("Use same format as native linker"), TWO_DASHES },
  { {"section-start", required_argument, NULL, OPTION_SECTION_START},
      '\0', N_("SECTION=ADDRESS"), N_("Set address of named section"), TWO_DASHES },
  { {"Tbss", required_argument, NULL, OPTION_TBSS},
      '\0', N_("ADDRESS"), N_("Set address of .bss section"), ONE_DASH },
  { {"Tdata", required_argument, NULL, OPTION_TDATA},
      '\0', N_("ADDRESS"), N_("Set address of .data section"), ONE_DASH },
  { {"Ttext", required_argument, NULL, OPTION_TTEXT},
      '\0', N_("ADDRESS"), N_("Set address of .text section"), ONE_DASH },
  { {"verbose", no_argument, NULL, OPTION_VERBOSE},
      '\0', NULL, N_("Output lots of information during link"), TWO_DASHES },
  { {"dll-verbose", no_argument, NULL, OPTION_VERBOSE}, /* Linux.  */
      '\0', NULL, NULL, NO_HELP },
  { {"version-script", required_argument, NULL, OPTION_VERSION_SCRIPT },
      '\0', N_("FILE"), N_("Read version information script"), TWO_DASHES },
  { {"version-exports-section", required_argument, NULL,
     OPTION_VERSION_EXPORTS_SECTION },
    '\0', N_("SYMBOL"), N_("Take export symbols list from .exports, using\n\t\t\t\tSYMBOL as the version."),
    TWO_DASHES },
  { {"warn-common", no_argument, NULL, OPTION_WARN_COMMON},
      '\0', NULL, N_("Warn about duplicate common symbols"), TWO_DASHES },
  { {"warn-constructors", no_argument, NULL, OPTION_WARN_CONSTRUCTORS},
      '\0', NULL, N_("Warn if global constructors/destructors are seen"),
      TWO_DASHES },
  { {"warn-multiple-gp", no_argument, NULL, OPTION_WARN_MULTIPLE_GP},
      '\0', NULL, N_("Warn if the multiple GP values are used"), TWO_DASHES },
  { {"warn-once", no_argument, NULL, OPTION_WARN_ONCE},
      '\0', NULL, N_("Warn only once per undefined symbol"), TWO_DASHES },
  { {"warn-section-align", no_argument, NULL, OPTION_WARN_SECTION_ALIGN},
      '\0', NULL, N_("Warn if start of section changes due to alignment"),
      TWO_DASHES },
  { {"fatal-warnings", no_argument, NULL, OPTION_WARN_FATAL},
     '\0', NULL, N_("Treat warnings as errors"),
     TWO_DASHES },
  { {"whole-archive", no_argument, NULL, OPTION_WHOLE_ARCHIVE},
      '\0', NULL, N_("Include all objects from following archives"), TWO_DASHES },
  { {"wrap", required_argument, NULL, OPTION_WRAP},
      '\0', N_("SYMBOL"), N_("Use wrapper functions for SYMBOL"), TWO_DASHES },
  { {"mpc860c0", optional_argument, NULL, OPTION_MPC860C0},
      '\0', N_("[=WORDS]"), N_("Modify problematic branches in last WORDS (1-10,\n\t\t\t\tdefault 5) words of a page"), TWO_DASHES }
};

#define OPTION_COUNT ARRAY_SIZE (ld_options)

/* Test STRING for containing a string of digits that form a number
   between MIN and MAX.  The return value is the number or ERR.  */

static int
is_num (string, min, max, err)
     const char *string;
     int min;
     int max;
     int err;
{
  int result = 0;

  for (; *string; ++string)
    {
      if (! ISDIGIT (*string))
	{
	  result = err;
	  break;
	}
      result = result * 10 + (*string - '0');
    }
  if (result < min || result > max)
    result = err;

  return result;
}

void
parse_args (argc, argv)
     unsigned argc;
     char **argv;
{
  unsigned i;
  int is, il, irl;
  int ingroup = 0;
  char *default_dirlist = NULL;
  char shortopts[OPTION_COUNT * 3 + 2];
  struct option longopts[OPTION_COUNT + 1];
  struct option really_longopts[OPTION_COUNT + 1];
  int last_optind;

  /* Starting the short option string with '-' is for programs that
     expect options and other ARGV-elements in any order and that care about
     the ordering of the two.  We describe each non-option ARGV-element
     as if it were the argument of an option with character code 1.  */
  shortopts[0] = '-';
  is = 1;
  il = 0;
  irl = 0;
  for (i = 0; i < OPTION_COUNT; i++)
    {
      if (ld_options[i].shortopt != '\0')
	{
	  shortopts[is] = ld_options[i].shortopt;
	  ++is;
	  if (ld_options[i].opt.has_arg == required_argument
	      || ld_options[i].opt.has_arg == optional_argument)
	    {
	      shortopts[is] = ':';
	      ++is;
	      if (ld_options[i].opt.has_arg == optional_argument)
		{
		  shortopts[is] = ':';
		  ++is;
		}
	    }
	}
      if (ld_options[i].opt.name != NULL)
	{
	  if (ld_options[i].control == EXACTLY_TWO_DASHES)
	    {
	      really_longopts[irl] = ld_options[i].opt;
	      ++irl;
	    }
	  else
	    {
	      longopts[il] = ld_options[i].opt;
	      ++il;
	    }
	}
    }
  shortopts[is] = '\0';
  longopts[il].name = NULL;
  really_longopts[irl].name = NULL;

  /* The -G option is ambiguous on different platforms.  Sometimes it
     specifies the largest data size to put into the small data
     section.  Sometimes it is equivalent to --shared.  Unfortunately,
     the first form takes an argument, while the second does not.

     We need to permit the --shared form because on some platforms,
     such as Solaris, gcc -shared will pass -G to the linker.

     To permit either usage, we look through the argument list.  If we
     find -G not followed by a number, we change it into --shared.
     This will work for most normal cases.  */
  for (i = 1; i < argc; i++)
    if (strcmp (argv[i], "-G") == 0
	&& (i + 1 >= argc
	    || ! ISDIGIT (argv[i + 1][0])))
      argv[i] = (char *) "--shared";

  /* Because we permit long options to start with a single dash, and
     we have a --library option, and the -l option is conventionally
     used with an immediately following argument, we can have bad
     results if somebody tries to use -l with a library whose name
     happens to start with "ibrary", as in -li.  We avoid problems by
     simply turning -l into --library.  This means that users will
     have to use two dashes in order to use --library, which is OK
     since that's how it is documented.

     FIXME: It's possible that this problem can arise for other short
     options as well, although the user does always have the recourse
     of adding a space between the option and the argument.  */
  for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-'
	  && argv[i][1] == 'l'
	  && argv[i][2] != '\0')
	{
	  char *n;

	  n = (char *) xmalloc (strlen (argv[i]) + 20);
	  sprintf (n, "--library=%s", argv[i] + 2);
	  argv[i] = n;
	}
    }

  last_optind = -1;
  while (1)
    {
      int longind;
      int optc;

      /* Using last_optind lets us avoid calling ldemul_parse_args
	 multiple times on a single option, which would lead to
	 confusion in the internal static variables maintained by
	 getopt.  This could otherwise happen for an argument like
	 -nx, in which the -n is parsed as a single option, and we
	 loop around to pick up the -x.  */
      if (optind != last_optind)
	if (ldemul_parse_args (argc, argv))
	  continue;

      /* getopt_long_only is like getopt_long, but '-' as well as '--'
	 can indicate a long option.  */
      opterr = 0;
      last_optind = optind;
      optc = getopt_long_only (argc, argv, shortopts, longopts, &longind);
      if (optc == '?')
	{
	  optind = last_optind;
	  optc = getopt_long (argc, argv, "-", really_longopts, &longind);
	}

      if (optc == -1)
	break;

      switch (optc)
	{
	case '?':
	  einfo (_("%P: unrecognized option '%s'\n"), argv[last_optind]);
	default:
	  einfo (_("%P%F: use the --help option for usage information\n"));

	case 1:			/* File name.  */
	  lang_add_input_file (optarg, lang_input_file_is_file_enum,
			       (char *) NULL);
	  break;

	case OPTION_IGNORE:
	  break;
	case 'a':
	  /* For HP/UX compatibility.  Actually -a shared should mean
             ``use only shared libraries'' but, then, we don't
             currently support shared libraries on HP/UX anyhow.  */
	  if (strcmp (optarg, "archive") == 0)
	    config.dynamic_link = false;
	  else if (strcmp (optarg, "shared") == 0
		   || strcmp (optarg, "default") == 0)
	    config.dynamic_link = true;
	  else
	    einfo (_("%P%F: unrecognized -a option `%s'\n"), optarg);
	  break;
	case OPTION_ASSERT:
	  /* FIXME: We just ignore these, but we should handle them.  */
	  if (strcmp (optarg, "definitions") == 0)
	    ;
	  else if (strcmp (optarg, "nodefinitions") == 0)
	    ;
	  else if (strcmp (optarg, "nosymbolic") == 0)
	    ;
	  else if (strcmp (optarg, "pure-text") == 0)
	    ;
	  else
	    einfo (_("%P%F: unrecognized -assert option `%s'\n"), optarg);
	  break;
	case 'A':
	  ldfile_add_arch (optarg);
	  break;
	case 'b':
	  lang_add_target (optarg);
	  break;
	case 'c':
	  ldfile_open_command_file (optarg);
	  parser_input = input_mri_script;
	  yyparse ();
	  break;
	case OPTION_CALL_SHARED:
	  config.dynamic_link = true;
	  break;
	case OPTION_NON_SHARED:
	  config.dynamic_link = false;
	  break;
	case OPTION_CREF:
	  command_line.cref = true;
	  link_info.notice_all = true;
	  break;
	case 'd':
	  command_line.force_common_definition = true;
	  break;
	case OPTION_DEFSYM:
	  lex_string = optarg;
	  lex_redirect (optarg);
	  parser_input = input_defsym;
	  parsing_defsym = 1;
	  yyparse ();
	  parsing_defsym = 0;
	  lex_string = NULL;
	  break;
	case OPTION_DEMANGLE:
	  demangling = true;
	  if (optarg != NULL)
	    {
	      enum demangling_styles style;

	      style = cplus_demangle_name_to_style (optarg);
	      if (style == unknown_demangling)
		einfo (_("%F%P: unknown demangling style `%s'"),
		       optarg);

	      cplus_demangle_set_style (style);
	    }
	  break;
	case 'I':		/* Used on Solaris.  */
	case OPTION_DYNAMIC_LINKER:
	  command_line.interpreter = optarg;
	  break;
	case OPTION_EB:
	  command_line.endian = ENDIAN_BIG;
	  break;
	case OPTION_EL:
	  command_line.endian = ENDIAN_LITTLE;
	  break;
	case OPTION_EMBEDDED_RELOCS:
	  command_line.embedded_relocs = true;
	  break;
	case OPTION_EXPORT_DYNAMIC:
	case 'E': /* HP/UX compatibility.  */
	  link_info.export_dynamic = true;
	  break;
	case 'e':
	  lang_add_entry (optarg, true);
	  break;
	case 'f':
	  if (command_line.auxiliary_filters == NULL)
	    {
	      command_line.auxiliary_filters =
		(char **) xmalloc (2 * sizeof (char *));
	      command_line.auxiliary_filters[0] = optarg;
	      command_line.auxiliary_filters[1] = NULL;
	    }
	  else
	    {
	      int c;
	      char **p;

	      c = 0;
	      for (p = command_line.auxiliary_filters; *p != NULL; p++)
		++c;
	      command_line.auxiliary_filters =
		(char **) xrealloc (command_line.auxiliary_filters,
				    (c + 2) * sizeof (char *));
	      command_line.auxiliary_filters[c] = optarg;
	      command_line.auxiliary_filters[c + 1] = NULL;
	    }
	  break;
	case 'F':
	  command_line.filter_shlib = optarg;
	  break;
	case OPTION_FORCE_EXE_SUFFIX:
	  command_line.force_exe_suffix = true;
	  break;
	case 'G':
	  {
	    char *end;
	    g_switch_value = strtoul (optarg, &end, 0);
	    if (*end)
	      einfo (_("%P%F: invalid number `%s'\n"), optarg);
	  }
	  break;
	case 'g':
	  /* Ignore.  */
	  break;
	case OPTION_GC_SECTIONS:
	  command_line.gc_sections = true;
	  break;
	case OPTION_HELP:
	  help ();
	  xexit (0);
	  break;
	case 'L':
	  ldfile_add_library_path (optarg, true);
	  break;
	case 'l':
	  lang_add_input_file (optarg, lang_input_file_is_l_enum,
			       (char *) NULL);
	  break;
	case 'M':
	  config.map_filename = "-";
	  break;
	case 'm':
	  /* Ignore.  Was handled in a pre-parse.   */
	  break;
	case OPTION_MAP:
	  config.map_filename = optarg;
	  break;
	case 'N':
	  config.text_read_only = false;
	  config.magic_demand_paged = false;
	  config.dynamic_link = false;
	  break;
	case 'n':
	  config.magic_demand_paged = false;
	  config.dynamic_link = false;
	  break;
	case OPTION_NO_DEFINE_COMMON:
	  command_line.inhibit_common_definition = true;
	  break;
	case OPTION_NO_DEMANGLE:
	  demangling = false;
	  break;
	case OPTION_NO_GC_SECTIONS:
	  command_line.gc_sections = false;
	  break;
	case OPTION_NO_KEEP_MEMORY:
	  link_info.keep_memory = false;
	  break;
	case OPTION_NO_UNDEFINED:
	  link_info.no_undefined = true;
	  break;
	case OPTION_ALLOW_SHLIB_UNDEFINED:
	  link_info.allow_shlib_undefined = true;
	  break;
	case OPTION_ALLOW_MULTIPLE_DEFINITION:
	  link_info.allow_multiple_definition = true;
	  break;
	case OPTION_NO_UNDEFINED_VERSION:
	  link_info.allow_undefined_version = false;
	  break;
	case OPTION_NO_WARN_MISMATCH:
	  command_line.warn_mismatch = false;
	  break;
	case OPTION_NOINHIBIT_EXEC:
	  force_make_executable = true;
	  break;
	case OPTION_NOSTDLIB:
	  config.only_cmd_line_lib_dirs = true;
	  break;
	case OPTION_NO_WHOLE_ARCHIVE:
	  whole_archive = false;
	  break;
	case 'O':
	  /* FIXME "-O<non-digits> <value>" used to set the address of
	     section <non-digits>.  Was this for compatibility with
	     something, or can we create a new option to do that
	     (with a syntax similar to -defsym)?
	     getopt can't handle two args to an option without kludges.  */

	  /* Enable optimizations of output files.  */
	  link_info.optimize = strtoul (optarg, NULL, 0) ? true : false;
	  break;
	case 'o':
	  lang_add_output (optarg, 0);
	  break;
	case OPTION_OFORMAT:
	  lang_add_output_format (optarg, (char *) NULL, (char *) NULL, 0);
	  break;
	case 'q':
	  link_info.emitrelocations = true;
	  break;
	case 'i':
	case 'r':
	  if (optind == last_optind)
	    /* This can happen if the user put "-rpath,a" on the command
	       line.  (Or something similar.  The comma is important).
	       Getopt becomes confused and thinks that this is a -r option
	       but it cannot parse the text after the -r so it refuses to
	       increment the optind counter.  Detect this case and issue
	       an error message here.  We cannot just make this a warning,
	       increment optind, and continue because getopt is too confused
	       and will seg-fault the next time around.  */
	    einfo(_("%P%F: bad -rpath option\n"));

	  link_info.relocateable = true;
	  config.build_constructors = false;
	  config.magic_demand_paged = false;
	  config.text_read_only = false;
	  config.dynamic_link = false;
	  break;
	case 'R':
	  /* The GNU linker traditionally uses -R to mean to include
	     only the symbols from a file.  The Solaris linker uses -R
	     to set the path used by the runtime linker to find
	     libraries.  This is the GNU linker -rpath argument.  We
	     try to support both simultaneously by checking the file
	     named.  If it is a directory, rather than a regular file,
	     we assume -rpath was meant.  */
	  {
	    struct stat s;

	    if (stat (optarg, &s) >= 0
		&& ! S_ISDIR (s.st_mode))
	      {
		lang_add_input_file (optarg,
				     lang_input_file_is_symbols_only_enum,
				     (char *) NULL);
		break;
	      }
	  }
	  /* Fall through.  */
	case OPTION_RPATH:
	  if (command_line.rpath == NULL)
	    command_line.rpath = xstrdup (optarg);
	  else
	    {
	      size_t rpath_len = strlen (command_line.rpath);
	      size_t optarg_len = strlen (optarg);
	      char *buf;
	      char *cp = command_line.rpath;

	      /* First see whether OPTARG is already in the path.  */
	      do
		{
		  size_t idx = 0;

		  while (optarg[idx] != '\0' && optarg[idx] == cp[idx])
		    ++idx;
		  if (optarg[idx] == '\0'
		      && (cp[idx] == '\0' || cp[idx] == ':'))
		    /* We found it.  */
		    break;

		  /* Not yet found.  */
		  cp = strchr (cp, ':');
		  if (cp != NULL)
		    ++cp;
		}
	      while (cp != NULL);

	      if (cp == NULL)
		{
		  buf = xmalloc (rpath_len + optarg_len + 2);
		  sprintf (buf, "%s:%s", command_line.rpath, optarg);
		  free (command_line.rpath);
		  command_line.rpath = buf;
		}
	    }
	  break;
	case OPTION_RPATH_LINK:
	  if (command_line.rpath_link == NULL)
	    command_line.rpath_link = xstrdup (optarg);
	  else
	    {
	      char *buf;

	      buf = xmalloc (strlen (command_line.rpath_link)
			     + strlen (optarg)
			     + 2);
	      sprintf (buf, "%s:%s", command_line.rpath_link, optarg);
	      free (command_line.rpath_link);
	      command_line.rpath_link = buf;
	    }
	  break;
	case OPTION_RELAX:
	  command_line.relax = true;
	  break;
	case OPTION_RETAIN_SYMBOLS_FILE:
	  add_keepsyms_file (optarg);
	  break;
	case 'S':
	  link_info.strip = strip_debugger;
	  break;
	case 's':
	  link_info.strip = strip_all;
	  break;
	case OPTION_SHARED:
	  if (config.has_shared)
	    link_info.shared = true;
	  else
	    einfo (_("%P%F: -shared not supported\n"));
	  break;
	case 'h':		/* Used on Solaris.  */
	case OPTION_SONAME:
	  command_line.soname = optarg;
	  break;
	case OPTION_SORT_COMMON:
	  config.sort_common = true;
	  break;
	case OPTION_STATS:
	  config.stats = true;
	  break;
	case OPTION_SYMBOLIC:
	  link_info.symbolic = true;
	  break;
	case 't':
	  trace_files = true;
	  break;
	case 'T':
	  ldfile_open_command_file (optarg);
	  parser_input = input_script;
	  yyparse ();
	  break;
	case OPTION_SECTION_START:
	  {
	    char *optarg2;
	    char *sec_name;
	    int len;

	    /* Check for <something>=<somthing>...  */
	    optarg2 = strchr (optarg, '=');
	    if (optarg2 == NULL)
	      einfo (_("%P%F: invalid argument to option \"--section-start\"\n"));

	    optarg2++;

	    /* So far so good.  Are all the args present?  */
	    if ((*optarg == '\0') || (*optarg2 == '\0'))
	      einfo (_("%P%F: missing argument(s) to option \"--section-start\"\n"));

	    /* We must copy the section name as set_section_start
	       doesn't do it for us.  */
	    len = optarg2 - optarg;
	    sec_name = xmalloc (len);
	    memcpy (sec_name, optarg, len - 1);
	    sec_name[len - 1] = 0;

	    /* Then set it...  */
	    set_section_start (sec_name, optarg2);
	  }
	  break;
	case OPTION_TARGET_HELP:
	  /* Mention any target specific options.  */
	  ldemul_list_emulation_options (stdout);
	  exit (0);
	case OPTION_TBSS:
	  set_section_start (".bss", optarg);
	  break;
	case OPTION_TDATA:
	  set_section_start (".data", optarg);
	  break;
	case OPTION_TTEXT:
	  set_section_start (".text", optarg);
	  break;
	case OPTION_TRADITIONAL_FORMAT:
	  link_info.traditional_format = true;
	  break;
	case OPTION_TASK_LINK:
	  link_info.task_link = true;
	  /* Fall through - do an implied -r option.  */
	case OPTION_UR:
	  link_info.relocateable = true;
	  config.build_constructors = true;
	  config.magic_demand_paged = false;
	  config.text_read_only = false;
	  config.dynamic_link = false;
	  break;
	case 'u':
	  ldlang_add_undef (optarg);
	  break;
	case OPTION_UNIQUE:
	  if (optarg != NULL)
	    lang_add_unique (optarg);
	  else
	    config.unique_orphan_sections = true;
	  break;
	case OPTION_VERBOSE:
	  ldversion (1);
	  version_printed = true;
	  trace_file_tries = true;
	  break;
	case 'v':
	  ldversion (0);
	  version_printed = true;
	  break;
	case 'V':
	  ldversion (1);
	  version_printed = true;
	  break;
	case OPTION_VERSION:
	  ldversion (2);
	  xexit (0);
	  break;
	case OPTION_VERSION_SCRIPT:
	  /* This option indicates a small script that only specifies
             version information.  Read it, but don't assume that
             we've seen a linker script.  */
	  {
	    FILE *hold_script_handle;

	    hold_script_handle = saved_script_handle;
	    ldfile_open_command_file (optarg);
	    saved_script_handle = hold_script_handle;
	    parser_input = input_version_script;
	    yyparse ();
	  }
	  break;
	case OPTION_VERSION_EXPORTS_SECTION:
	  /* This option records a version symbol to be applied to the
	     symbols listed for export to be found in the object files
	     .exports sections.  */
	  command_line.version_exports_section = optarg;
	  break;
	case OPTION_WARN_COMMON:
	  config.warn_common = true;
	  break;
	case OPTION_WARN_CONSTRUCTORS:
	  config.warn_constructors = true;
	  break;
	case OPTION_WARN_FATAL:
	  config.fatal_warnings = true;
	  break;
	case OPTION_WARN_MULTIPLE_GP:
	  config.warn_multiple_gp = true;
	  break;
	case OPTION_WARN_ONCE:
	  config.warn_once = true;
	  break;
	case OPTION_WARN_SECTION_ALIGN:
	  config.warn_section_align = true;
	  break;
	case OPTION_WHOLE_ARCHIVE:
	  whole_archive = true;
	  break;
	case OPTION_WRAP:
	  add_wrap (optarg);
	  break;
	case OPTION_DISCARD_NONE:
	  link_info.discard = discard_none;
	  break;
	case 'X':
	  link_info.discard = discard_l;
	  break;
	case 'x':
	  link_info.discard = discard_all;
	  break;
	case 'Y':
	  if (strncmp (optarg, "P,", 2) == 0)
	    optarg += 2;
	  default_dirlist = xstrdup (optarg);
	  break;
	case 'y':
	  add_ysym (optarg);
	  break;
	case OPTION_SPARE_DYNAMIC_TAGS:
	  link_info.spare_dynamic_tags = strtoul (optarg, NULL, 0);
	  break;
	case OPTION_SPLIT_BY_RELOC:
	  if (optarg != NULL)
	    config.split_by_reloc = strtoul (optarg, NULL, 0);
	  else
	    config.split_by_reloc = 32768;
	  break;
	case OPTION_SPLIT_BY_FILE:
	  if (optarg != NULL)
	    config.split_by_file = bfd_scan_vma (optarg, NULL, 0);
	  else
	    config.split_by_file = 1;
	  break;
	case OPTION_CHECK_SECTIONS:
	  command_line.check_section_addresses = true;
	  break;
	case OPTION_NO_CHECK_SECTIONS:
	  command_line.check_section_addresses = false;
	  break;
	case '(':
	  if (ingroup)
	    einfo (_("%P%F: may not nest groups (--help for usage)\n"));

	  lang_enter_group ();
	  ingroup = 1;
	  break;
	case ')':
	  if (! ingroup)
	    einfo (_("%P%F: group ended before it began (--help for usage)\n"));

	  lang_leave_group ();
	  ingroup = 0;
	  break;
	case OPTION_MPC860C0:
	  /* Default value (in bytes).  */
	  link_info.mpc860c0 = 20;
	  if (optarg)
	    {
	      unsigned words;

	      words = is_num (optarg, 1, 10, 0);
	      if (words == 0)
		einfo (_("%P%F: invalid argument to option \"mpc860c0\"\n"));

	      /* Convert words to bytes.  */
	      link_info.mpc860c0 = words * 4;
	    }
	  command_line.relax = true;
	  break;

	case OPTION_INIT:
	  link_info.init_function = optarg;
	  break;

	case OPTION_FINI:
	  link_info.fini_function = optarg;
	  break;
	}
    }

  if (ingroup)
    lang_leave_group ();

  if (default_dirlist != NULL)
    set_default_dirlist (default_dirlist);

}

/* Add the (colon-separated) elements of DIRLIST_PTR to the
   library search path.  */

static void
set_default_dirlist (dirlist_ptr)
     char *dirlist_ptr;
{
  char *p;

  while (1)
    {
      p = strchr (dirlist_ptr, PATH_SEPARATOR);
      if (p != NULL)
	*p = '\0';
      if (*dirlist_ptr != '\0')
	ldfile_add_library_path (dirlist_ptr, true);
      if (p == NULL)
	break;
      dirlist_ptr = p + 1;
    }
}

static void
set_section_start (sect, valstr)
     char *sect, *valstr;
{
  const char *end;
  bfd_vma val = bfd_scan_vma (valstr, &end, 16);
  if (*end)
    einfo (_("%P%F: invalid hex number `%s'\n"), valstr);
  lang_section_start (sect, exp_intop (val));
}

/* Print help messages for the options.  */

static void
help ()
{
  unsigned i;
  const char **targets, **pp;

  printf (_("Usage: %s [options] file...\n"), program_name);

  printf (_("Options:\n"));
  for (i = 0; i < OPTION_COUNT; i++)
    {
      if (ld_options[i].doc != NULL)
	{
	  boolean comma;
	  int len;
	  unsigned j;

	  printf ("  ");

	  comma = false;
	  len = 2;

	  j = i;
	  do
	    {
	      if (ld_options[j].shortopt != '\0'
		  && ld_options[j].control != NO_HELP)
		{
		  printf ("%s-%c", comma ? ", " : "", ld_options[j].shortopt);
		  len += (comma ? 2 : 0) + 2;
		  if (ld_options[j].arg != NULL)
		    {
		      if (ld_options[j].opt.has_arg != optional_argument)
			{
			  printf (" ");
			  ++len;
			}
		      printf ("%s", _(ld_options[j].arg));
		      len += strlen (_(ld_options[j].arg));
		    }
		  comma = true;
		}
	      ++j;
	    }
	  while (j < OPTION_COUNT && ld_options[j].doc == NULL);

	  j = i;
	  do
	    {
	      if (ld_options[j].opt.name != NULL
		  && ld_options[j].control != NO_HELP)
		{
		  int two_dashes =
		    (ld_options[j].control == TWO_DASHES
		     || ld_options[j].control == EXACTLY_TWO_DASHES);

		  printf ("%s-%s%s",
			  comma ? ", " : "",
			  two_dashes ? "-" : "",
			  ld_options[j].opt.name);
		  len += ((comma ? 2 : 0)
			  + 1
			  + (two_dashes ? 1 : 0)
			  + strlen (ld_options[j].opt.name));
		  if (ld_options[j].arg != NULL)
		    {
		      printf (" %s", _(ld_options[j].arg));
		      len += 1 + strlen (_(ld_options[j].arg));
		    }
		  comma = true;
		}
	      ++j;
	    }
	  while (j < OPTION_COUNT && ld_options[j].doc == NULL);

	  if (len >= 30)
	    {
	      printf ("\n");
	      len = 0;
	    }

	  for (; len < 30; len++)
	    putchar (' ');

	  printf ("%s\n", _(ld_options[i].doc));
	}
    }

  /* Note: Various tools (such as libtool) depend upon the
     format of the listings below - do not change them.  */
  /* xgettext:c-format */
  printf (_("%s: supported targets:"), program_name);
  targets = bfd_target_list ();
  for (pp = targets; *pp != NULL; pp++)
    printf (" %s", *pp);
  free (targets);
  printf ("\n");

  /* xgettext:c-format */
  printf (_("%s: supported emulations: "), program_name);
  ldemul_list_emulations (stdout);
  printf ("\n");

  /* xgettext:c-format */
  printf (_("%s: emulation specific options:\n"), program_name);
  ldemul_list_emulation_options (stdout);
  printf ("\n");

  printf (_("Report bugs to %s\n"), REPORT_BUGS_TO);
}
