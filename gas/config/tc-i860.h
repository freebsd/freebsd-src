/* tc-i860.h -- Header file for the i860.
   Copyright 1991, 1992, 1995, 1998, 2000, 2001, 2002, 2003, 2005
   Free Software Foundation, Inc.

   Brought back from the dead and completely reworked
   by Jason Eckhardt <jle@cygnus.com>.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with GAS; see the file COPYING.  If not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef TC_I860
#define TC_I860 1

enum i860_fix_info
{
  OP_NONE	 = 0x00000,
  OP_IMM_U5	 = 0x00001,
  OP_IMM_S16	 = 0x00002,
  OP_IMM_U16	 = 0x00004,
  OP_IMM_SPLIT16 = 0x00008,
  OP_IMM_BR26	 = 0x00010,
  OP_IMM_BR16	 = 0x00020,
  OP_ENCODE1	 = 0x00040,
  OP_ENCODE2	 = 0x00080,
  OP_ENCODE3	 = 0x00100,
  OP_SEL_HA	 = 0x00200,
  OP_SEL_H	 = 0x00400,
  OP_SEL_L	 = 0x00800,
  OP_SEL_GOT	 = 0x01000,
  OP_SEL_GOTOFF	 = 0x02000,
  OP_SEL_PLT	 = 0x04000,
  OP_ALIGN2	 = 0x08000,
  OP_ALIGN4	 = 0x10000,
  OP_ALIGN8	 = 0x20000,
  OP_ALIGN16	 = 0x40000
};

/* Set the endianness we are using.  Default to little endian.  */
#ifndef TARGET_BYTES_BIG_ENDIAN
#define TARGET_BYTES_BIG_ENDIAN 0
#endif

/* Whether or not the target is big endian.  */
extern int target_big_endian;

/* BFD target architecture.  */
#define TARGET_ARCH     bfd_arch_i860

/* The target BFD format.  */
#ifdef OBJ_ELF
#define TARGET_FORMAT (target_big_endian ? "elf32-i860" : "elf32-i860-little")
#else
#error i860 GAS currently supports only the ELF object format
#endif

#define WORKING_DOT_WORD
#define DIFF_EXPR_OK

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB		1
#define LISTING_HEADER		"GAS for i860"

#define md_convert_frag(b,s,f)  as_fatal (_("i860_convert_frag\n"));

/* Values passed to md_apply_fix don't include the symbol value.  */
#define MD_APPLY_SYM_VALUE(FIX) 0

/* No shared lib support, so we don't need to ensure externally
   visible symbols can be overridden.  */
#define EXTERN_FORCE_RELOC 0

/* Bits for post-processing of a user defined label to check if
   it has a double colon (Intel syntax only).  */
extern void i860_check_label (symbolS *labelsym);
#define tc_check_label(ls)	i860_check_label (ls)

/* Bits for filling in rs_align_code fragments with NOPs.  */
extern void i860_handle_align (struct frag *);
#define HANDLE_ALIGN(fragp) i860_handle_align (fragp)

#define MAX_MEM_FOR_RS_ALIGN_CODE  (3 + 4 + 4)

#endif /* TC_I860 */
