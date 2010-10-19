/* tc-mn10300.h -- Header file for tc-mn10300.c.
   Copyright 1996, 1997, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#define TC_MN10300

#define TARGET_BYTES_BIG_ENDIAN 0

#define DIFF_EXPR_OK
#define GLOBAL_OFFSET_TABLE_NAME "_GLOBAL_OFFSET_TABLE_"

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)				\
  ((FIX)->fx_r_type != BFD_RELOC_32_PLT_PCREL			\
   && (FIX)->fx_r_type != BFD_RELOC_MN10300_GOT32		\
   && (FIX)->fx_r_type != BFD_RELOC_32_GOT_PCREL		\
   && ((FIX)->fx_addsy == NULL					\
       || (! S_IS_EXTERNAL ((FIX)->fx_addsy)			\
	   && ! S_IS_WEAK ((FIX)->fx_addsy)			\
	   && S_IS_DEFINED ((FIX)->fx_addsy)			\
	   && ! S_IS_COMMON ((FIX)->fx_addsy))))

#define md_parse_name(name, exprP, mode, nextcharP) \
    mn10300_parse_name ((name), (exprP), (mode), (nextcharP))
int mn10300_parse_name PARAMS ((char const *, expressionS *,
				enum expr_mode, char *));

#define TC_CONS_FIX_NEW(FRAG, OFF, LEN, EXP) \
     mn10300_cons_fix_new ((FRAG), (OFF), (LEN), (EXP))
void mn10300_cons_fix_new PARAMS ((fragS *, int, int, expressionS *));

/* This is used to construct expressions out of @GOTOFF, @PLT and @GOT
   symbols.  The relocation type is stored in X_md.  */
#define O_PIC_reloc O_md1

/* The target BFD architecture.  */
#define TARGET_ARCH bfd_arch_mn10300

#ifdef TE_LINUX
#define TARGET_FORMAT "elf32-am33lin"
#else
#define TARGET_FORMAT "elf32-mn10300"
#endif


/* Do not adjust relocations involving symbols in code sections,
   because it breaks linker relaxations.  This could be fixed in the
   linker, but this fix is simpler, and it pretty much only affects
   object size a little bit.  */
#define TC_FORCE_RELOCATION_SUB_SAME(FIX, SEC)	\
  (((SEC)->flags & SEC_CODE) != 0		\
   || ! SEG_NORMAL (SEC)			\
   || TC_FORCE_RELOCATION (FIX))

/* We validate subtract arguments within tc_gen_reloc(), so don't
   report errors at this point.  */
#define TC_VALIDATE_FIX_SUB(FIX) 1

/* Fixup debug sections since we will never relax them.  Ideally, we
   could do away with this and instead check every single fixup with
   TC_FORCE_RELOCATION and TC_FORCE_RELOCATION_SUB_NAME, verifying
   that the sections of the referenced symbols (and not the sections
   in which the fixup appears) may be subject to relaxation.  We'd
   still have to check the section in which the fixup appears, because
   we want to do some simplifications in debugging info that might
   break in real code.

   Using the infrastructure in write.c to simplify subtraction fixups
   would enable us to remove a lot of code from tc_gen_reloc(), but
   this is simpler, faster, and produces almost the same effect.
   Also, in the macros above, we can't check whether the fixup is in a
   debugging section or not, so we have to use this for now.  */
#define TC_LINKRELAX_FIXUP(seg) (seg->flags & SEC_ALLOC)

#define md_operand(x)

/* Permit temporary numeric labels.  */
#define LOCAL_LABELS_FB 1

/* We don't need to handle .word strangely.  */
#define WORKING_DOT_WORD

#define md_number_to_chars number_to_chars_littleendian

/* Don't bother to adjust relocs.  */
/* #define tc_fix_adjustable(FIX) 0 */
#define tc_fix_adjustable(FIX) mn10300_fix_adjustable (FIX)
extern bfd_boolean mn10300_fix_adjustable PARAMS ((struct fix *));

/* We do relaxing in the assembler as well as the linker.  */
extern const struct relax_type md_relax_table[];
#define TC_GENERIC_RELAX_TABLE md_relax_table

#define DWARF2_LINE_MIN_INSN_LENGTH 1
