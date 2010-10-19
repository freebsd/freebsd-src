/* NLM (NetWare Loadable Module) support for BFD.
   Copyright 1993, 2001 Free Software Foundation, Inc.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */


/* This file is part of NLM support for BFD, and contains the portions
   that are common to both the internal and external representations. */

/* If NLM_ARCH_SIZE is not defined, default to 32.  NLM_ARCH_SIZE is
   optionally defined by the application. */

#ifndef NLM_ARCH_SIZE
#  define NLM_ARCH_SIZE			32
#endif

/* Due to horrible details of ANSI macro expansion, we can't use CONCAT4
   for NLM_NAME.  CONCAT2 is used in BFD_JUMP_TABLE macros, and some of
   them will expand to tokens that themselves are macros defined in terms
   of NLM_NAME.  If NLM_NAME were defined using CONCAT4 (which is itself
   defined in bfd-in.h using CONCAT2), ANSI preprocessor rules say that
   the CONCAT2 within NLM_NAME should not be expanded.
   So use another name.  */
#if defined (__STDC__) || defined (ALMOST_STDC) || defined (HAVE_STRINGIZE)
#ifdef SABER
#define NLM_CAT4(a,b,c,d) a##b##c##d
#else
/* This hack is to avoid a problem with some strict ANSI C preprocessors.
   The problem is, "32_" is not a valid preprocessing token, and we don't
   want extra underscores (e.g., "nlm_32_").  The NLM_XCAT2 macro will
   cause the inner CAT2 macros to be evaluated first, producing
   still-valid pp-tokens.  Then the final concatenation can be done.  */
#define NLM_CAT2(a,b)	  a##b
#define NLM_XCAT2(a,b)	  NLM_CAT2(a,b)
#define NLM_CAT4(a,b,c,d) NLM_XCAT2(NLM_CAT2(a,b),NLM_CAT2(c,d))
#endif
#else
#define NLM_CAT4(a,b,c,d) a/**/b/**/c/**/d
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
