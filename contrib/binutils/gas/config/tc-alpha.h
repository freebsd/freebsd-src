/* This file is tc-alpha.h
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.
   Written by Ken Raeburn <raeburn@cygnus.com>.

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
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#define TC_ALPHA

#define TARGET_BYTES_BIG_ENDIAN 0

#define WORKING_DOT_WORD

#define TARGET_ARCH			bfd_arch_alpha

#ifdef TE_FreeBSD
#define ELF_TARGET_FORMAT	"elf64-alpha-freebsd"
#endif
#ifndef ELF_TARGET_FORMAT
#define ELF_TARGET_FORMAT	"elf64-alpha"
#endif

#define TARGET_FORMAT (OUTPUT_FLAVOR == bfd_target_ecoff_flavour	\
		       ? "ecoff-littlealpha"				\
		       : OUTPUT_FLAVOR == bfd_target_elf_flavour	\
		       ? ELF_TARGET_FORMAT				\
		       : OUTPUT_FLAVOR == bfd_target_evax_flavour	\
		       ? "vms-alpha"					\
		       : "unknown-format")

#define NEED_LITERAL_POOL
#define REPEAT_CONS_EXPRESSIONS

extern int alpha_force_relocation PARAMS ((struct fix *));
extern int alpha_fix_adjustable PARAMS ((struct fix *));

extern unsigned long alpha_gprmask, alpha_fprmask;
extern valueT alpha_gp_value;

#define TC_FORCE_RELOCATION(FIXP)	alpha_force_relocation (FIXP)
#define tc_fix_adjustable(FIXP)		alpha_fix_adjustable (FIXP)
#define RELOC_REQUIRES_SYMBOL

/* This expression evaluates to false if the relocation is for a local
   object for which we still want to do the relocation at runtime.
   True if we are willing to perform this relocation while building
   the .o file.  This is only used for pcrel relocations.  */

#define TC_RELOC_RTSYM_LOC_FIXUP(FIX)				\
  ((FIX)->fx_addsy == NULL					\
   || (! S_IS_EXTERNAL ((FIX)->fx_addsy)			\
       && ! S_IS_WEAK ((FIX)->fx_addsy)				\
       && S_IS_DEFINED ((FIX)->fx_addsy)			\
       && ! S_IS_COMMON ((FIX)->fx_addsy)))

#define md_convert_frag(b,s,f)		as_fatal ("alpha convert_frag\n")
#define md_estimate_size_before_relax(f,s) \
			(as_fatal ("estimate_size_before_relax called"),1)
#define md_operand(x)

#ifdef OBJ_EVAX

/* This field keeps the symbols position in the link section.  */
#define OBJ_SYMFIELD_TYPE valueT

#define TC_CONS_FIX_NEW(FRAG,OFF,LEN,EXP) \
      fix_new_exp (FRAG, OFF, (int)LEN, EXP, 0, \
	LEN == 2 ? BFD_RELOC_16 \
	: LEN == 4 ? BFD_RELOC_32 \
	: LEN == 8 ? BFD_RELOC_64 \
	: BFD_RELOC_ALPHA_LINKAGE);
#endif

#define md_number_to_chars		number_to_chars_littleendian

extern int tc_get_register PARAMS ((int frame));
extern void alpha_frob_ecoff_data PARAMS ((void));

#define tc_frob_label(sym) alpha_define_label (sym)
extern void alpha_define_label PARAMS ((symbolS *));

#define md_cons_align(nbytes) alpha_cons_align (nbytes)
extern void alpha_cons_align PARAMS ((int));

#define HANDLE_ALIGN(fragp) alpha_handle_align (fragp)
extern void alpha_handle_align PARAMS ((struct frag *));

#define MAX_MEM_FOR_RS_ALIGN_CODE  (3 + 4 + 8)

#ifdef OBJ_ECOFF
#define tc_frob_file_before_adjust() alpha_frob_file_before_adjust ()
extern void alpha_frob_file_before_adjust PARAMS ((void));
#endif

#define DIFF_EXPR_OK   /* foo-. gets turned into PC relative relocs */

#ifdef OBJ_ELF
#define ELF_TC_SPECIAL_SECTIONS \
  { ".sdata",   SHT_PROGBITS,   SHF_ALLOC + SHF_WRITE + SHF_ALPHA_GPREL  }, \
  { ".sbss",    SHT_NOBITS,     SHF_ALLOC + SHF_WRITE + SHF_ALPHA_GPREL  },

#define md_elf_section_letter		alpha_elf_section_letter
extern int alpha_elf_section_letter PARAMS ((int, char **));
#define md_elf_section_flags		alpha_elf_section_flags
extern flagword alpha_elf_section_flags PARAMS ((flagword, int, int));
#endif

/* Whether to add support for explict !relocation_op!sequence_number.  At the
   moment, only do this for ELF, though ECOFF could use it as well.  */

#ifdef OBJ_ELF
#define RELOC_OP_P
#endif

/* Before the relocations are written, reorder them, so that user
   supplied !lituse relocations follow the appropriate !literal
   relocations.  Also convert the gas-internal relocations to the
   appropriate linker relocations.  */
#define tc_adjust_symtab() alpha_adjust_symtab ()
extern void alpha_adjust_symtab PARAMS ((void));

/* New fields for supporting explicit relocations (such as !literal to mark
   where a pointer is loaded from the global table, and !lituse_base to track
   all of the normal uses of that pointer).  */

#define TC_FIX_TYPE struct alpha_fix_tag

struct alpha_fix_tag
{
  struct fix *next_reloc;		/* next !lituse or !gpdisp */
  struct alpha_reloc_tag *info;		/* other members with same sequence */
};

/* Initialize the TC_FIX_TYPE field.  */
#define TC_INIT_FIX_DATA(fixP)						\
do {									\
  fixP->tc_fix_data.next_reloc = (struct fix *)0;			\
  fixP->tc_fix_data.info = (struct alpha_reloc_tag *)0;			\
} while (0)

/* Work with DEBUG5 to print fields in tc_fix_type.  */
#define TC_FIX_DATA_PRINT(stream,fixP)					\
do {									\
  if (fixP->tc_fix_data.info)						\
    fprintf (stderr, "\tinfo = 0x%lx, next_reloc = 0x%lx\n", \
	     (long)fixP->tc_fix_data.info,				\
	     (long)fixP->tc_fix_data.next_reloc);			\
} while (0)

#define DWARF2_LINE_MIN_INSN_LENGTH 4
