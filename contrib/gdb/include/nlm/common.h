/* NLM (NetWare Loadable Module) support for BFD.
   Copyright (C) 1993 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* This file is part of NLM support for BFD, and contains the portions
   that are common to both the internal and external representations. */

/* Semi-portable string concatenation in cpp.
   The NLM_CAT4 hack is to avoid a problem with some strict ANSI C
   preprocessors.  The problem is, "32_" or "64_" are not a valid
   preprocessing tokens, and we don't want extra underscores (e.g.,
   "nlm_32_").  The XNLM_CAT2 macro will cause the inner NLM_CAT macros
   to be evaluated first, producing still-valid pp-tokens.  Then the
   final concatenation can be done.  (Sigh.)  */

#ifdef SABER
#  define NLM_CAT(a,b)		a##b
#  define NLM_CAT3(a,b,c)	a##b##c
#  define NLM_CAT4(a,b,c,d)	a##b##c##d
#else
#  ifdef __STDC__
#    define NLM_CAT(a,b)	a##b
#    define NLM_CAT3(a,b,c)	a##b##c
#    define XNLM_CAT2(a,b)	NLM_CAT(a,b)
#    define NLM_CAT4(a,b,c,d)	XNLM_CAT2(NLM_CAT(a,b),NLM_CAT(c,d))
#  else
#    define NLM_CAT(a,b)	a/**/b
#    define NLM_CAT3(a,b,c)	a/**/b/**/c
#    define NLM_CAT4(a,b,c,d)	a/**/b/**/c/**/d
#  endif
#endif

/* If NLM_ARCH_SIZE is not defined, default to 32.  NLM_ARCH_SIZE is
   optionally defined by the application. */

#ifndef NLM_ARCH_SIZE
#  define NLM_ARCH_SIZE			32
#endif

#if NLM_ARCH_SIZE == 32
#  define NLM_TARGET_LONG_SIZE		4
#  define NLM_TARGET_ADDRESS_SIZE	4
#  define NLM_NAME(x,y)			NLM_CAT4(x,32,_,y)
#  define NLM_HIBIT			(((bfd_vma) 1) << 31)
#endif
#if NLM_ARCH_SIZE == 64
#  define NLM_TARGET_LONG_SIZE		8
#  define NLM_TARGET_ADDRESS_SIZE	8
#  define NLM_NAME(x,y)			NLM_CAT4(x,64,_,y)
#  define NLM_HIBIT			(((bfd_vma) 1) << 63)
#endif

#define NlmNAME(X)		NLM_NAME(Nlm,X)
#define nlmNAME(X)		NLM_NAME(nlm,X)

/* Give names to things that should not change. */

#define NLM_MAX_DESCRIPTION_LENGTH		127
#define NLM_MAX_SCREEN_NAME_LENGTH		71
#define NLM_MAX_THREAD_NAME_LENGTH		71
#define NLM_MAX_COPYRIGHT_MESSAGE_LENGTH	255
#define NLM_OTHER_DATA_LENGTH 			400		/* FIXME */
#define NLM_OLD_THREAD_NAME_LENGTH		5
#define NLM_SIGNATURE_SIZE			24
#define NLM_HEADER_VERSION			4
#define NLM_MODULE_NAME_SIZE			14
#define NLM_DEFAULT_STACKSIZE			(8 * 1024)

/* Alpha information.  This should probably be in a separate Alpha
   header file, but it can't go in alpha-ext.h because some of it is
   needed by nlmconv.c.  */

/* Magic number in Alpha prefix header.  */
#define NLM32_ALPHA_MAGIC (0x83561840)

/* The r_type field in an Alpha reloc is one of the following values.  */
#define ALPHA_R_IGNORE		0
#define ALPHA_R_REFLONG		1
#define ALPHA_R_REFQUAD		2
#define ALPHA_R_GPREL32		3
#define ALPHA_R_LITERAL		4
#define ALPHA_R_LITUSE		5
#define ALPHA_R_GPDISP		6
#define ALPHA_R_BRADDR		7
#define ALPHA_R_HINT		8
#define ALPHA_R_SREL16		9
#define ALPHA_R_SREL32	       10
#define ALPHA_R_SREL64	       11
#define ALPHA_R_OP_PUSH	       12
#define ALPHA_R_OP_STORE       13
#define ALPHA_R_OP_PSUB	       14
#define ALPHA_R_OP_PRSHIFT     15
#define ALPHA_R_GPVALUE	       16
#define ALPHA_R_NW_RELOC      250

/* A local reloc, other than ALPHA_R_GPDISP or ALPHA_R_IGNORE, must be
   against one of these symbol indices.  */
#define ALPHA_RELOC_SECTION_TEXT	1
#define ALPHA_RELOC_SECTION_DATA	3

/* An ALPHA_R_NW_RELOC has one of these values in the size field.  If
   it is SETGP, the r_vaddr field holds the GP value to use.  If it is
   LITA, the r_vaddr field holds the address of the .lita section and
   the r_symndx field holds the size of the .lita section.  */
#define ALPHA_R_NW_RELOC_SETGP	1
#define ALPHA_R_NW_RELOC_LITA	2
