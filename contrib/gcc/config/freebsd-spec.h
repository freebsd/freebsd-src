/* Base configuration file for all FreeBSD targets.
   Copyright (C) 1999, 2000, 2001 Free Software Foundation, Inc.

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

/* Common FreeBSD configuration. 
   All FreeBSD architectures should include this file, which will specify
   their commonalities.
   Adapted from gcc/config/freebsd.h by 
   David O'Brien <obrien@FreeBSD.org>
   Loren J. Rittle <ljrittle@acm.org>.  */


/* In case we need to know.  */
#define USING_CONFIG_FREEBSD_SPEC 1

/* This defines which switch letters take arguments.  On FreeBSD, most of
   the normal cases (defined in gcc.c) apply, and we also have -h* and
   -z* options (for the linker) (coming from SVR4).
   We also have -R (alias --rpath), no -z, --soname (-h), --assert etc.  */

#define FBSD_SWITCH_TAKES_ARG(CHAR)					\
  (DEFAULT_SWITCH_TAKES_ARG (CHAR)					\
    || (CHAR) == 'h'							\
    || (CHAR) == 'z' /* ignored by ld */				\
    || (CHAR) == 'R')

/* This defines which multi-letter switches take arguments.  */

#define FBSD_WORD_SWITCH_TAKES_ARG(STR)					\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)					\
   || !strcmp ((STR), "rpath") || !strcmp ((STR), "rpath-link")		\
   || !strcmp ((STR), "soname") || !strcmp ((STR), "defsym") 		\
   || !strcmp ((STR), "assert") || !strcmp ((STR), "dynamic-linker"))

#define FBSD_TARGET_OS_CPP_BUILTINS()					\
  do									\
    {									\
	if (FBSD_MAJOR == 6)						\
	  builtin_define ("__FreeBSD__=6");			       	\
	else if (FBSD_MAJOR == 5)	       				\
	  builtin_define ("__FreeBSD__=5");			       	\
	else if (FBSD_MAJOR == 4)			       		\
	  builtin_define ("__FreeBSD__=4");			       	\
	else if (FBSD_MAJOR == 3)	       				\
	  builtin_define ("__FreeBSD__=3");			       	\
	else								\
	  builtin_define ("__FreeBSD__");			       	\
	builtin_define_std ("unix");					\
	builtin_define ("__ELF__");					\
	builtin_define ("__KPRINTF_ATTRIBUTE__");		       	\
	builtin_assert ("system=unix");					\
	builtin_assert ("system=bsd");					\
	builtin_assert ("system=FreeBSD");				\
	FBSD_NATIVE_TARGET_OS_CPP_BUILTINS();				\
	FBSD_TARGET_CPU_CPP_BUILTINS();					\
    }									\
  while (0)

/* Define the default FreeBSD-specific per-CPU hook code. */
#define FBSD_TARGET_CPU_CPP_BUILTINS() do {} while (0)

#ifdef FREEBSD_NATIVE
#define FBSD_NATIVE_TARGET_OS_CPP_BUILTINS()				\
  do {									\
	builtin_define_std ("__FreeBSD_cc_version=500006");		\
  } while (0)
#else
#define FBSD_NATIVE_TARGET_OS_CPP_BUILTINS()				\
  do {} while (0)
#endif

/* Provide a CPP_SPEC appropriate for FreeBSD.  We just deal with the GCC 
   option `-posix', and PIC issues.  Try to detect support for the
   `long long' type.  Unfortunately the GCC spec parser will not allow us
   to properly detect the "iso9899:1990" and "iso9899:199409" forms of
   -std=c89.  Because of the ':' in the -std argument. :-(  I have left
   them in the spec as a place holder in hopes someone knows a way to make
   the detection of them work.  */

#define FBSD_CPP_SPEC "							\
  %(cpp_cpu)								\
  %{fPIC:-D__PIC__ -D__pic__} %{fpic:-D__PIC__ -D__pic__}		\
  %{!ansi:%{!std=c89:%{!std=iso9899.1990:%{!std=iso9899.199409:-D_LONGLONG}}}} \
  %{posix:-D_POSIX_SOURCE}"

/* Provide a STARTFILE_SPEC appropriate for FreeBSD.  Here we add the magical
   crtbegin.o file (see crtstuff.c) which provides part of the support for
   getting C++ file-scope static object constructed before entering `main'.  */
   
#define FBSD_STARTFILE_SPEC "\
  %{!shared: \
    %{pg:gcrt1.o%s} \
    %{!pg: \
      %{p:gcrt1.o%s} \
      %{!p: \
	%{profile:gcrt1.o%s} \
	%{!profile:crt1.o%s}}}} \
  crti.o%s \
  %{!shared:crtbegin.o%s} \
  %{shared:crtbeginS.o%s}"

/* Provide an ENDFILE_SPEC appropriate for FreeBSD/i386.  Here we tack on
   our own magical crtend.o file (see crtstuff.c) which provides part of
   the support for getting C++ file-scope static object constructed before
   entering `main', followed by the normal "finalizer" file, `crtn.o'.  */

#define FBSD_ENDFILE_SPEC "\
  %{!shared:crtend.o%s} \
  %{shared:crtendS.o%s} \
  crtn.o%s "

/* Provide a LIB_SPEC appropriate for FreeBSD.  Just select the appropriate
   libc, depending on whether we're doing profiling or not.
   (simular to the default, except no -lg, and no -p).  */

#define FBSD_LIB_SPEC "							\
  %{pthread: %eThe -pthread option is deprecated.}			\
  %{!shared:								\
    %{!pg: -lc}								\
    %{pg:  -lc_p}							\
  }"
