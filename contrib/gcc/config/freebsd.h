/* $Id: freebsd.h,v 1.9 1999/04/27 15:43:56 obrien Exp $ */
/* Base configuration file for all FreeBSD targets.
   Copyright (C) 1999 Free Software Foundation, Inc.

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
   Adapted from /usr/src/contrib/gcc/config/i386/freebsd.h & 
   egcs/gcc/config/i386/freebsd-elf.h version by David O'Brien  */


/* Don't assume anything about the header files. */
#undef NO_IMPLICIT_EXTERN_C
#define NO_IMPLICIT_EXTERN_C

/* This defines which switch letters take arguments.  On FreeBSD, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker) (comming from svr4).
   We also have -R (alias --rpath), no -z, --soname (-h), --assert etc. */

#define FBSD_SWITCH_TAKES_ARG(CHAR) \
  (DEFAULT_SWITCH_TAKES_ARG (CHAR) \
   || (CHAR) == 'h' \
   || (CHAR) == 'z' /* ignored by ld */ \
   || (CHAR) == 'R')

#define FBSD_WORD_SWITCH_TAKES_ARG(STR)					\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)					\
   || !strcmp (STR, "rpath") || !strcmp (STR, "rpath-link")		\
   || !strcmp (STR, "soname") || !strcmp (STR, "defsym") 		\
   || !strcmp (STR, "assert") || !strcmp (STR, "dynamic-linker"))

/* Place spaces around this string.  We depend on string splicing to produce
   the final CPP_PREDEFINES value.  */
#define CPP_FBSD_PREDEFINES " -Dunix -D__FreeBSD__=4 -D__FreeBSD_cc_version=400002 -Asystem(unix) -Asystem(FreeBSD) "

/* Provide a LIB_SPEC appropriate for FreeBSD.  Just select the appropriate
   libc, depending on whether we're doing profiling. 
   (like the default, except no -lg, and no -p.  */
#undef LIB_SPEC
#define LIB_SPEC "%{!shared:%{!pg:%{!pthread:%{!kthread:-lc}%{kthread:-lpthread -lc}}%{pthread:-lc_r}}%{pg:%{!pthread:%{!kthread:-lc_p}%{kthread:-lpthread_p -lc_p}}%{pthread:-lc_r_p}}}"

/* Let gcc locate this for us according to the -m rules */
#undef LIBGCC_SPEC
#define LIBGCC_SPEC \
 "%{!shared:%{!pthread:%{!kthread:libgcc.a%s}}%{pthread|kthread:libgcc_r.a%s}}"


/* Code generation parameters.  */

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  
   (even though the svr4 ABI for the i386 says that records and unions are
   returned in memory)  */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Ensure we the configuration knows our system correctly so we can link with
   libraries compiled with the native cc. */
#undef NO_DOLLAR_IN_LABEL

/* Wait until EGCS 1.2 to use more efficient ``thunks'' to implement
   C++ vtables. */
#undef DEFAULT_VTABLE_THUNKS
#define DEFAULT_VTABLE_THUNKS 0


/* Miscellaneous parameters.  */

/* Tell libgcc2.c that FreeBSD targets support atexit(3).  */
#define HAVE_ATEXIT


/* FREEBSD_NATIVE is defined when gcc is integrated into the FreeBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

#ifdef FREEBSD_NATIVE

/* Look for the include files in the system-defined places.  */

#define GPLUSPLUS_INCLUDE_DIR		"/usr/include/g++"
#define GCC_INCLUDE_DIR			"/usr/include"

/* Now that GCC knows what the include path applies to, put the G++ one first.
   C++ can now have include files that override the default C ones.  */
#undef INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS			\
  {						\
    { GPLUSPLUS_INCLUDE_DIR, "C++", 1, 1 },	\
    { GCC_INCLUDE_DIR, "GCC", 0, 0 },		\
    { 0, 0, 0, 0 }				\
  }

/* Under FreeBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.  */

#undef STANDARD_EXEC_PREFIX
#undef TOOLDIR_BASE_PREFIX
#undef MD_EXEC_PREFIX

#define STANDARD_EXEC_PREFIX		"/usr/libexec/"
#define TOOLDIR_BASE_PREFIX		"/usr/libexec/"
#define MD_EXEC_PREFIX			"/usr/libexec/"

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX	"/usr/lib/"

/* FreeBSD is 4.4BSD derived */
#define bsd4_4

#endif /* FREEBSD_NATIVE */
