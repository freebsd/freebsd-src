/* Definitions for DEC Alpha/AXP running FreeBSD using the ELF format
   Copyright (C) 2000, 2002 Free Software Foundation, Inc.
   Contributed by David E. O'Brien <obrien@FreeBSD.org> and BSDi.

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


/* Provide a CPP_SPEC appropriate for FreeBSD/alpha.  Besides the dealing with
   the GCC option `-posix', and PIC issues as on all FreeBSD platforms, we must
   deal with the Alpha's FP issues.  */

#undef  CPP_SPEC
#define CPP_SPEC "%(cpp_cpu) %(cpp_subtarget) -D__ELF__			\
  %{fPIC:-D__PIC__ -D__pic__} %{fpic:-D__PIC__ -D__pic__}		\
  %{posix:-D_POSIX_SOURCE}						\
  %{mieee:-D_IEEE_FP}							\
  %{mieee-with-inexact:-D_IEEE_FP -D_IEEE_FP_INEXACT}"

#undef  LINK_SPEC
#define LINK_SPEC "%{G*} %{relax:-relax}				\
  %{p:%e`-p' not supported; use `-pg' and gprof(1)}			\
  %{Wl,*:%*}								\
  %{assert*} %{R*} %{rpath*} %{defsym*}					\
  %{shared:-Bshareable %{h*} %{soname*}}				\
  %{symbolic:-Bsymbolic}						\
  %{!shared:								\
    %{!static:								\
      %{rdynamic:-export-dynamic}					\
      %{!dynamic-linker:-dynamic-linker /usr/libexec/ld-elf.so.1}}	\
    %{static:-Bstatic}}"

/* We now have to provide a STARTFILE_SPEC because of a moronic pigheaded
   Linuxism(glibc'ism) that was added to alpha/elf.h.  */

 #undef	 STARTFILE_SPEC
 #define STARTFILE_SPEC \
   "%{!shared: \
      %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}}\
   crti.o%s %{shared:crtbeginS.o%s}%{!shared:crtbegin.o%s}"


/************************[  Target stuff  ]***********************************/

/* Define the actual types of some ANSI-mandated types.  
   Needs to agree with <machine/ansi.h>.  GCC defaults come from c-decl.c,
   c-common.c, and config/<arch>/<arch>.h.  */

/* alpha.h gets this wrong for FreeBSD.  We use the GCC defaults instead.  */
#undef WCHAR_TYPE

#undef  WCHAR_UNSIGNED
#define WCHAR_UNSIGNED	0

#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE	32

/* Handle cross-compilation on 32-bits machines (such as i386) for 64-bits
   machines (Alpha in this case).  */

#if defined(__i386__)
#undef  HOST_BITS_PER_LONG
#define HOST_BITS_PER_LONG	32
#undef  HOST_WIDE_INT
#define HOST_WIDE_INT		long long
#undef  HOST_BITS_PER_WIDE_INT
#define HOST_BITS_PER_WIDE_INT	64
#endif

/* This is the pseudo-op used to generate a 64-bit word of data with a
   specific value in some section.  */

#undef  TARGET_VERSION
#define TARGET_VERSION	fprintf (stderr, " (FreeBSD/Alpha ELF)");

#define TARGET_ELF		1

#undef OBJECT_FORMAT_COFF
#undef EXTENDED_COFF

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT	(MASK_FP | MASK_FPREGS | MASK_GAS)

#undef HAS_INIT_SECTION

/* Show that we need a GP when profiling.  */
#undef  TARGET_PROFILING_NEEDS_GP
#define TARGET_PROFILING_NEEDS_GP 1

/* We always use gas here, so we don't worry about ECOFF assembler problems.  */
#undef  TARGET_GAS
#define TARGET_GAS	1


/************************[  Assembler stuff  ]********************************/



/************************[  Debugger stuff  ]*********************************/

/* This is the char to use for continuation (in case we need to turn
   continuation back on).  */

#undef  DBX_CONTIN_CHAR
#define DBX_CONTIN_CHAR	'?'

/* Don't default to pcc-struct-return, we want to retain compatibility with
   older FreeBSD releases AND pcc-struct-return may not be reentrant.  */

#undef  DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0
