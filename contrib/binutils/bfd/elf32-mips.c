/* MIPS-specific support for 32-bit ELF
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Most of the information added by Ian Lance Taylor, Cygnus Support,
   <ian@cygnus.com>.
   N32/64 ABI support added by Mark Mitchell, CodeSourcery, LLC.
   <mark@codesourcery.com>
   Traditional MIPS targets support added by Koundinya.K, Dansk Data
   Elektronik & Operations Research Group. <kk@ddeorg.soft.net>

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

/* This file handles MIPS ELF targets.  SGI Irix 5 uses a slightly
   different MIPS ELF from other targets.  This matters when linking.
   This file supports both, switching at runtime.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "elf-bfd.h"
#include "elf/mips.h"

/* Get the ECOFF swapping routines.  */
#include "coff/sym.h"
#include "coff/symconst.h"
#include "coff/internal.h"
#include "coff/ecoff.h"
#include "coff/mips.h"
#define ECOFF_SIGNED_32
#include "ecoffswap.h"

/* This structure is used to hold .got information when linking.  It
   is stored in the tdata field of the bfd_elf_section_data structure.  */

struct mips_got_info
{
  /* The global symbol in the GOT with the lowest index in the dynamic
     symbol table.  */
  struct elf_link_hash_entry *global_gotsym;
  /* The number of global .got entries.  */
  unsigned int global_gotno;
  /* The number of local .got entries.  */
  unsigned int local_gotno;
  /* The number of local .got entries we have used.  */
  unsigned int assigned_gotno;
};

/* The MIPS ELF linker needs additional information for each symbol in
   the global hash table.  */

struct mips_elf_link_hash_entry
{
  struct elf_link_hash_entry root;

  /* External symbol information.  */
  EXTR esym;

  /* Number of R_MIPS_32, R_MIPS_REL32, or R_MIPS_64 relocs against
     this symbol.  */
  unsigned int possibly_dynamic_relocs;

  /* If the R_MIPS_32, R_MIPS_REL32, or R_MIPS_64 reloc is against
     a readonly section.  */
  boolean readonly_reloc;

  /* The index of the first dynamic relocation (in the .rel.dyn
     section) against this symbol.  */
  unsigned int min_dyn_reloc_index;

  /* We must not create a stub for a symbol that has relocations
     related to taking the function's address, i.e. any but
     R_MIPS_CALL*16 ones -- see "MIPS ABI Supplement, 3rd Edition",
     p. 4-20.  */
  boolean no_fn_stub;

  /* If there is a stub that 32 bit functions should use to call this
     16 bit function, this points to the section containing the stub.  */
  asection *fn_stub;

  /* Whether we need the fn_stub; this is set if this symbol appears
     in any relocs other than a 16 bit call.  */
  boolean need_fn_stub;

  /* If there is a stub that 16 bit functions should use to call this
     32 bit function, this points to the section containing the stub.  */
  asection *call_stub;

  /* This is like the call_stub field, but it is used if the function
     being called returns a floating point value.  */
  asection *call_fp_stub;
};

static bfd_reloc_status_type mips32_64bit_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *, bfd_reloc_code_real_type));
static reloc_howto_type *mips_rtype_to_howto
  PARAMS ((unsigned int));
static void mips_info_to_howto_rel
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rel *));
static void mips_info_to_howto_rela
  PARAMS ((bfd *, arelent *, Elf32_Internal_Rela *));
static void bfd_mips_elf32_swap_gptab_in
  PARAMS ((bfd *, const Elf32_External_gptab *, Elf32_gptab *));
static void bfd_mips_elf32_swap_gptab_out
  PARAMS ((bfd *, const Elf32_gptab *, Elf32_External_gptab *));
#if 0
static void bfd_mips_elf_swap_msym_in
  PARAMS ((bfd *, const Elf32_External_Msym *, Elf32_Internal_Msym *));
#endif
static void bfd_mips_elf_swap_msym_out
  PARAMS ((bfd *, const Elf32_Internal_Msym *, Elf32_External_Msym *));
static boolean mips_elf_sym_is_global PARAMS ((bfd *, asymbol *));
static boolean mips_elf_create_procedure_table
  PARAMS ((PTR, bfd *, struct bfd_link_info *, asection *,
	   struct ecoff_debug_info *));
static INLINE int elf_mips_isa PARAMS ((flagword));
static INLINE unsigned long elf_mips_mach PARAMS ((flagword));
static INLINE char* elf_mips_abi_name PARAMS ((bfd *));
static boolean mips_elf_is_local_label_name
  PARAMS ((bfd *, const char *));
static struct bfd_hash_entry *mips_elf_link_hash_newfunc
  PARAMS ((struct bfd_hash_entry *, struct bfd_hash_table *, const char *));
static int gptab_compare PARAMS ((const void *, const void *));
static bfd_reloc_status_type mips16_jump_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static bfd_reloc_status_type mips16_gprel_reloc
  PARAMS ((bfd *, arelent *, asymbol *, PTR, asection *, bfd *, char **));
static boolean mips_elf_create_compact_rel_section
  PARAMS ((bfd *, struct bfd_link_info *));
static boolean mips_elf_create_got_section
  PARAMS ((bfd *, struct bfd_link_info *));
static bfd_reloc_status_type mips_elf_final_gp
  PARAMS ((bfd *, asymbol *, boolean, char **, bfd_vma *));
static bfd_byte *elf32_mips_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, boolean, asymbol **));
static asection *mips_elf_create_msym_section
  PARAMS ((bfd *));
static void mips_elf_irix6_finish_dynamic_symbol
  PARAMS ((bfd *, const char *, Elf_Internal_Sym *));
static bfd_vma mips_elf_sign_extend PARAMS ((bfd_vma, int));
static boolean mips_elf_overflow_p PARAMS ((bfd_vma, int));
static bfd_vma mips_elf_high PARAMS ((bfd_vma));
static bfd_vma mips_elf_higher PARAMS ((bfd_vma));
static bfd_vma mips_elf_highest PARAMS ((bfd_vma));
static bfd_vma mips_elf_global_got_index
  PARAMS ((bfd *, struct elf_link_hash_entry *));
static bfd_vma mips_elf_local_got_index
  PARAMS ((bfd *, struct bfd_link_info *, bfd_vma));
static bfd_vma mips_elf_got_offset_from_index
  PARAMS ((bfd *, bfd *, bfd_vma));
static boolean mips_elf_record_global_got_symbol
  PARAMS ((struct elf_link_hash_entry *, struct bfd_link_info *,
	   struct mips_got_info *));
static bfd_vma mips_elf_got_page
  PARAMS ((bfd *, struct bfd_link_info *, bfd_vma, bfd_vma *));
static const Elf_Internal_Rela *mips_elf_next_relocation
  PARAMS ((unsigned int, const Elf_Internal_Rela *,
	   const Elf_Internal_Rela *));
static bfd_reloc_status_type mips_elf_calculate_relocation
  PARAMS ((bfd *, bfd *, asection *, struct bfd_link_info *,
	   const Elf_Internal_Rela *, bfd_vma, reloc_howto_type *,
	   Elf_Internal_Sym *, asection **, bfd_vma *, const char **,
	   boolean *));
static bfd_vma mips_elf_obtain_contents
  PARAMS ((reloc_howto_type *, const Elf_Internal_Rela *, bfd *, bfd_byte *));
static boolean mips_elf_perform_relocation
  PARAMS ((struct bfd_link_info *, reloc_howto_type *,
	   const Elf_Internal_Rela *, bfd_vma,
	   bfd *, asection *, bfd_byte *, boolean));
static boolean mips_elf_assign_gp PARAMS ((bfd *, bfd_vma *));
static boolean mips_elf_sort_hash_table_f
  PARAMS ((struct mips_elf_link_hash_entry *, PTR));
static boolean mips_elf_sort_hash_table
  PARAMS ((struct bfd_link_info *, unsigned long));
static asection * mips_elf_got_section PARAMS ((bfd *));
static struct mips_got_info *mips_elf_got_info
  PARAMS ((bfd *, asection **));
static boolean mips_elf_local_relocation_p
  PARAMS ((bfd *, const Elf_Internal_Rela *, asection **, boolean));
static bfd_vma mips_elf_create_local_got_entry
  PARAMS ((bfd *, struct mips_got_info *, asection *, bfd_vma));
static bfd_vma mips_elf_got16_entry
  PARAMS ((bfd *, struct bfd_link_info *, bfd_vma, boolean));
static boolean mips_elf_create_dynamic_relocation
  PARAMS ((bfd *, struct bfd_link_info *, const Elf_Internal_Rela *,
	   struct mips_elf_link_hash_entry *, asection *,
	   bfd_vma, bfd_vma *, asection *));
static void mips_elf_allocate_dynamic_relocations
  PARAMS ((bfd *, unsigned int));
static boolean mips_elf_stub_section_p
  PARAMS ((bfd *, asection *));
static int sort_dynamic_relocs
  PARAMS ((const void *, const void *));
static void _bfd_mips_elf_hide_symbol
  PARAMS ((struct bfd_link_info *, struct elf_link_hash_entry *, boolean));
static void _bfd_mips_elf_copy_indirect_symbol
  PARAMS ((struct elf_link_hash_entry *,
	   struct elf_link_hash_entry *));
static boolean _bfd_elf32_mips_grok_prstatus
  PARAMS ((bfd *, Elf_Internal_Note *));
static boolean _bfd_elf32_mips_grok_psinfo
  PARAMS ((bfd *, Elf_Internal_Note *));
static boolean _bfd_elf32_mips_discard_info
  PARAMS ((bfd *, struct elf_reloc_cookie *, struct bfd_link_info *));
static boolean _bfd_elf32_mips_ignore_discarded_relocs
  PARAMS ((asection *));
static boolean _bfd_elf32_mips_write_section
  PARAMS ((bfd *, asection *, bfd_byte *));

extern const bfd_target bfd_elf32_tradbigmips_vec;
extern const bfd_target bfd_elf32_tradlittlemips_vec;
#ifdef BFD64
extern const bfd_target bfd_elf64_tradbigmips_vec;
extern const bfd_target bfd_elf64_tradlittlemips_vec;
#endif

/* The level of IRIX compatibility we're striving for.  */

typedef enum {
  ict_none,
  ict_irix5,
  ict_irix6
} irix_compat_t;

/* This will be used when we sort the dynamic relocation records.  */
static bfd *reldyn_sorting_bfd;

/* Nonzero if ABFD is using the N32 ABI.  */

#define ABI_N32_P(abfd) \
  ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI2) != 0)

/* Nonzero if ABFD is using the 64-bit ABI. */
#define ABI_64_P(abfd) \
  ((elf_elfheader (abfd)->e_ident[EI_CLASS] == ELFCLASS64) != 0)

/* Depending on the target vector we generate some version of Irix
   executables or "normal" MIPS ELF ABI executables.  */
#ifdef BFD64
#define IRIX_COMPAT(abfd) \
  (((abfd->xvec == &bfd_elf64_tradbigmips_vec) || \
    (abfd->xvec == &bfd_elf64_tradlittlemips_vec) || \
    (abfd->xvec == &bfd_elf32_tradbigmips_vec) || \
    (abfd->xvec == &bfd_elf32_tradlittlemips_vec)) ? ict_none : \
  ((ABI_N32_P (abfd) || ABI_64_P (abfd)) ? ict_irix6 : ict_irix5))
#else
#define IRIX_COMPAT(abfd) \
  (((abfd->xvec == &bfd_elf32_tradbigmips_vec) || \
    (abfd->xvec == &bfd_elf32_tradlittlemips_vec)) ? ict_none : \
  ((ABI_N32_P (abfd) || ABI_64_P (abfd)) ? ict_irix6 : ict_irix5))
#endif

#define NEWABI_P(abfd) (ABI_N32_P(abfd) || ABI_64_P(abfd))

/* Whether we are trying to be compatible with IRIX at all.  */
#define SGI_COMPAT(abfd) \
  (IRIX_COMPAT (abfd) != ict_none)

/* The name of the msym section.  */
#define MIPS_ELF_MSYM_SECTION_NAME(abfd) ".msym"

/* The name of the srdata section.  */
#define MIPS_ELF_SRDATA_SECTION_NAME(abfd) ".srdata"

/* The name of the options section.  */
#define MIPS_ELF_OPTIONS_SECTION_NAME(abfd) \
  (IRIX_COMPAT (abfd) == ict_irix6 ? ".MIPS.options" : ".options")

/* The name of the stub section.  */
#define MIPS_ELF_STUB_SECTION_NAME(abfd) \
  (IRIX_COMPAT (abfd) == ict_irix6 ? ".MIPS.stubs" : ".stub")

/* The name of the dynamic relocation section.  */
#define MIPS_ELF_REL_DYN_SECTION_NAME(abfd) ".rel.dyn"

/* The size of an external REL relocation.  */
#define MIPS_ELF_REL_SIZE(abfd) \
  (get_elf_backend_data (abfd)->s->sizeof_rel)

/* The size of an external dynamic table entry.  */
#define MIPS_ELF_DYN_SIZE(abfd) \
  (get_elf_backend_data (abfd)->s->sizeof_dyn)

/* The size of a GOT entry.  */
#define MIPS_ELF_GOT_SIZE(abfd) \
  (get_elf_backend_data (abfd)->s->arch_size / 8)

/* The size of a symbol-table entry.  */
#define MIPS_ELF_SYM_SIZE(abfd) \
  (get_elf_backend_data (abfd)->s->sizeof_sym)

/* The default alignment for sections, as a power of two.  */
#define MIPS_ELF_LOG_FILE_ALIGN(abfd)				\
  (get_elf_backend_data (abfd)->s->file_align == 8 ? 3 : 2)

/* Get word-sized data.  */
#define MIPS_ELF_GET_WORD(abfd, ptr) \
  (ABI_64_P (abfd) ? bfd_get_64 (abfd, ptr) : bfd_get_32 (abfd, ptr))

/* Put out word-sized data.  */
#define MIPS_ELF_PUT_WORD(abfd, val, ptr)	\
  (ABI_64_P (abfd) 				\
   ? bfd_put_64 (abfd, val, ptr) 		\
   : bfd_put_32 (abfd, val, ptr))

/* Add a dynamic symbol table-entry.  */
#ifdef BFD64
#define MIPS_ELF_ADD_DYNAMIC_ENTRY(info, tag, val)			\
  (ABI_64_P (elf_hash_table (info)->dynobj)				\
   ? bfd_elf64_add_dynamic_entry (info, (bfd_vma) tag, (bfd_vma) val)	\
   : bfd_elf32_add_dynamic_entry (info, (bfd_vma) tag, (bfd_vma) val))
#else
#define MIPS_ELF_ADD_DYNAMIC_ENTRY(info, tag, val)			\
  (ABI_64_P (elf_hash_table (info)->dynobj)				\
   ? (boolean) (abort (), false)					\
   : bfd_elf32_add_dynamic_entry (info, (bfd_vma) tag, (bfd_vma) val))
#endif

/* The number of local .got entries we reserve.  */
#define MIPS_RESERVED_GOTNO (2)

/* Instructions which appear in a stub.  For some reason the stub is
   slightly different on an SGI system.  */
#define ELF_MIPS_GP_OFFSET(abfd) (SGI_COMPAT (abfd) ? 0x7ff0 : 0x8000)
#define STUB_LW(abfd)						\
  (SGI_COMPAT (abfd)						\
   ? (ABI_64_P (abfd)  						\
      ? 0xdf998010		/* ld t9,0x8010(gp) */		\
      : 0x8f998010)             /* lw t9,0x8010(gp) */		\
   : 0x8f998010)		/* lw t9,0x8000(gp) */
#define STUB_MOVE(abfd)                                         \
  (SGI_COMPAT (abfd) ? 0x03e07825 : 0x03e07821)         /* move t7,ra */
#define STUB_JALR 0x0320f809				/* jal t9 */
#define STUB_LI16(abfd)                                         \
  (SGI_COMPAT (abfd) ? 0x34180000 : 0x24180000)         /* ori t8,zero,0 */
#define MIPS_FUNCTION_STUB_SIZE (16)

#if 0
/* We no longer try to identify particular sections for the .dynsym
   section.  When we do, we wind up crashing if there are other random
   sections with relocations.  */

/* Names of sections which appear in the .dynsym section in an Irix 5
   executable.  */

static const char * const mips_elf_dynsym_sec_names[] =
{
  ".text",
  ".init",
  ".fini",
  ".data",
  ".rodata",
  ".sdata",
  ".sbss",
  ".bss",
  NULL
};

#define SIZEOF_MIPS_DYNSYM_SECNAMES \
  (sizeof mips_elf_dynsym_sec_names / sizeof mips_elf_dynsym_sec_names[0])

/* The number of entries in mips_elf_dynsym_sec_names which go in the
   text segment.  */

#define MIPS_TEXT_DYNSYM_SECNO (3)

#endif /* 0 */

/* The names of the runtime procedure table symbols used on Irix 5.  */

static const char * const mips_elf_dynsym_rtproc_names[] =
{
  "_procedure_table",
  "_procedure_string_table",
  "_procedure_table_size",
  NULL
};

/* These structures are used to generate the .compact_rel section on
   Irix 5.  */

typedef struct
{
  unsigned long id1;		/* Always one?  */
  unsigned long num;		/* Number of compact relocation entries.  */
  unsigned long id2;		/* Always two?  */
  unsigned long offset;		/* The file offset of the first relocation.  */
  unsigned long reserved0;	/* Zero?  */
  unsigned long reserved1;	/* Zero?  */
} Elf32_compact_rel;

typedef struct
{
  bfd_byte id1[4];
  bfd_byte num[4];
  bfd_byte id2[4];
  bfd_byte offset[4];
  bfd_byte reserved0[4];
  bfd_byte reserved1[4];
} Elf32_External_compact_rel;

typedef struct
{
  unsigned int ctype : 1;	/* 1: long 0: short format. See below.  */
  unsigned int rtype : 4;	/* Relocation types. See below.  */
  unsigned int dist2to : 8;
  unsigned int relvaddr : 19;	/* (VADDR - vaddr of the previous entry)/ 4 */
  unsigned long konst;		/* KONST field. See below.  */
  unsigned long vaddr;		/* VADDR to be relocated.  */
} Elf32_crinfo;

typedef struct
{
  unsigned int ctype : 1;	/* 1: long 0: short format. See below.  */
  unsigned int rtype : 4;	/* Relocation types. See below.  */
  unsigned int dist2to : 8;
  unsigned int relvaddr : 19;	/* (VADDR - vaddr of the previous entry)/ 4 */
  unsigned long konst;		/* KONST field. See below.  */
} Elf32_crinfo2;

typedef struct
{
  bfd_byte info[4];
  bfd_byte konst[4];
  bfd_byte vaddr[4];
} Elf32_External_crinfo;

typedef struct
{
  bfd_byte info[4];
  bfd_byte konst[4];
} Elf32_External_crinfo2;

/* These are the constants used to swap the bitfields in a crinfo.  */

#define CRINFO_CTYPE (0x1)
#define CRINFO_CTYPE_SH (31)
#define CRINFO_RTYPE (0xf)
#define CRINFO_RTYPE_SH (27)
#define CRINFO_DIST2TO (0xff)
#define CRINFO_DIST2TO_SH (19)
#define CRINFO_RELVADDR (0x7ffff)
#define CRINFO_RELVADDR_SH (0)

/* A compact relocation info has long (3 words) or short (2 words)
   formats.  A short format doesn't have VADDR field and relvaddr
   fields contains ((VADDR - vaddr of the previous entry) >> 2).  */
#define CRF_MIPS_LONG			1
#define CRF_MIPS_SHORT			0

/* There are 4 types of compact relocation at least. The value KONST
   has different meaning for each type:

   (type)		(konst)
   CT_MIPS_REL32	Address in data
   CT_MIPS_WORD		Address in word (XXX)
   CT_MIPS_GPHI_LO	GP - vaddr
   CT_MIPS_JMPAD	Address to jump
   */

#define CRT_MIPS_REL32			0xa
#define CRT_MIPS_WORD			0xb
#define CRT_MIPS_GPHI_LO		0xc
#define CRT_MIPS_JMPAD			0xd

#define mips_elf_set_cr_format(x,format)	((x).ctype = (format))
#define mips_elf_set_cr_type(x,type)		((x).rtype = (type))
#define mips_elf_set_cr_dist2to(x,v)		((x).dist2to = (v))
#define mips_elf_set_cr_relvaddr(x,d)		((x).relvaddr = (d)<<2)

static void bfd_elf32_swap_compact_rel_out
  PARAMS ((bfd *, const Elf32_compact_rel *, Elf32_External_compact_rel *));
static void bfd_elf32_swap_crinfo_out
  PARAMS ((bfd *, const Elf32_crinfo *, Elf32_External_crinfo *));

/* In case we're on a 32-bit machine, construct a 64-bit "-1" value
   from smaller values.  Start with zero, widen, *then* decrement.  */
#define MINUS_ONE	(((bfd_vma)0) - 1)

/* The relocation table used for SHT_REL sections.  */

static reloc_howto_type elf_mips_howto_table_rel[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit jump address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC + 4.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 true,			/* partial_inplace */
	 0x03ffffff,		/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_hi16_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_lo16_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_got16_reloc,	/* special_function */
	 "R_MIPS_GOT16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* The remaining relocs are defined on Irix 5, although they are
     not defined by the ABI.  */
  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 true,			/* partial_inplace */
	 0x000007c0,		/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  /* FIXME: This is not handled correctly; a special function is
     needed to put the most significant bit in the right place.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 true,			/* partial_inplace */
	 0x000007c4,		/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 mips32_64bit_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit subtraction.  Used in the N32 ABI.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Used to cause the linker to insert and delete instructions?  */
  EMPTY_HOWTO (R_MIPS_INSERT_A),
  EMPTY_HOWTO (R_MIPS_INSERT_B),
  EMPTY_HOWTO (R_MIPS_DELETE),

  /* Get the higher value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0x0000ffff,		/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement.  */
  HOWTO (R_MIPS_SCN_DISP,       /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",     /* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (R_MIPS_REL16),
  EMPTY_HOWTO (R_MIPS_ADD_IMMEDIATE),
  EMPTY_HOWTO (R_MIPS_PJUMP),
  EMPTY_HOWTO (R_MIPS_RELGOT),

  /* Protected jump conversion.  This is an optimization hint.  No
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0x00000000,		/* src_mask */
	 0x00000000,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The relocation table used for SHT_RELA sections.  */

static reloc_howto_type elf_mips_howto_table_rela[] =
{
  /* No relocation.  */
  HOWTO (R_MIPS_NONE,		/* type */
	 0,			/* rightshift */
	 0,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_NONE",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit relocation.  */
  HOWTO (R_MIPS_16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit relocation.  */
  HOWTO (R_MIPS_32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_32",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit symbol relative relocation.  */
  HOWTO (R_MIPS_REL32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 26 bit jump address.  */
  HOWTO (R_MIPS_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
				/* This needs complex overflow
				   detection, because the upper 36
				   bits must match the PC + 4.  */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_26",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x03ffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* R_MIPS_HI16 and R_MIPS_LO16 are unsupported for 64 bit REL.  */
  /* High 16 bits of symbol value.  */
  HOWTO (R_MIPS_HI16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_HI16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_MIPS_LO16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_LO16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* GP relative reference.  */
  HOWTO (R_MIPS_GPREL16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_GPREL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to literal section.  */
  HOWTO (R_MIPS_LITERAL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 _bfd_mips_elf_gprel16_reloc, /* special_function */
	 "R_MIPS_LITERAL",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Reference to global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_GOT16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_GOT16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 16 bit PC relative reference.  */
  HOWTO (R_MIPS_PC16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC16",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 true),			/* pcrel_offset */

  /* 16 bit call through global offset table.  */
  /* FIXME: This is not handled correctly.  */
  HOWTO (R_MIPS_CALL16,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 32 bit GP relative reference.  */
  HOWTO (R_MIPS_GPREL32,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_gprel32_reloc, /* special_function */
	 "R_MIPS_GPREL32",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  EMPTY_HOWTO (15),

  /* A 5 bit shift field.  */
  HOWTO (R_MIPS_SHIFT5,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 5,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SHIFT5",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000007c0,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* A 6 bit shift field.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SHIFT6,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 6,			/* bitsize */
	 false,			/* pc_relative */
	 6,			/* bitpos */
	 complain_overflow_bitfield, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_SHIFT6",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x000007c4,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit relocation.  */
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Displacement to page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_PAGE,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_PAGE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Offset from page pointer in the global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_OFST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_OFST",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_HI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_GOT_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GOT_LO16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* 64 bit substraction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SUB,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SUB",		/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_A,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_A",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Insert the addend as an instruction, and change all relocations
     to refer to the old instruction at the address.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_INSERT_B,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_INSERT_B",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Delete a 32 bit instruction.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_DELETE,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_DELETE",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the higher value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHER,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_HIGHER",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Get the highest value of a 64 bit addend.  */
  HOWTO (R_MIPS_HIGHEST,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc, /* special_function */
	 "R_MIPS_HIGHEST",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* High 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_HI16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Low 16 bits of displacement in global offset table.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_CALL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_CALL_LO16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0x0000ffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Section displacement, used by an associated event location section.  */
  /* FIXME: Not handled correctly.  */
  HOWTO (R_MIPS_SCN_DISP,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_SCN_DISP",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  HOWTO (R_MIPS_REL16,		/* type */
	 0,			/* rightshift */
	 1,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_REL16",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* These two are obsolete.  */
  EMPTY_HOWTO (R_MIPS_ADD_IMMEDIATE),
  EMPTY_HOWTO (R_MIPS_PJUMP),

  /* Similiar to R_MIPS_REL32, but used for relocations in a GOT section.
     It must be used for multigot GOT's (and only there).  */
  HOWTO (R_MIPS_RELGOT,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_RELGOT",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */

  /* Protected jump conversion.  This is an optimization hint.  No
     relocation is required for correctness.  */
  HOWTO (R_MIPS_JALR,	        /* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_JALR",	        /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false),		/* pcrel_offset */
};

/* The reloc used for BFD_RELOC_CTOR when doing a 64 bit link.  This
   is a hack to make the linker think that we need 64 bit values.  */
static reloc_howto_type elf_mips_ctor64_howto =
  HOWTO (R_MIPS_64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips32_64bit_reloc,	/* special_function */
	 "R_MIPS_64",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 false);		/* pcrel_offset */

/* The reloc used for the mips16 jump instruction.  */
static reloc_howto_type elf_mips16_jump_howto =
  HOWTO (R_MIPS16_26,		/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 26,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 			/* This needs complex overflow
				   detection, because the upper four
				   bits must match the PC.  */
	 mips16_jump_reloc,	/* special_function */
	 "R_MIPS16_26",		/* name */
	 true,			/* partial_inplace */
	 0x3ffffff,		/* src_mask */
	 0x3ffffff,		/* dst_mask */
	 false);		/* pcrel_offset */

/* The reloc used for the mips16 gprel instruction.  */
static reloc_howto_type elf_mips16_gprel_howto =
  HOWTO (R_MIPS16_GPREL,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 mips16_gprel_reloc,	/* special_function */
	 "R_MIPS16_GPREL",	/* name */
	 true,			/* partial_inplace */
	 0x07ff001f,		/* src_mask */
	 0x07ff001f,	        /* dst_mask */
	 false);		/* pcrel_offset */

/* GNU extensions for embedded-pic.  */
/* High 16 bits of symbol value, pc-relative.  */
static reloc_howto_type elf_mips_gnu_rel_hi16 =
  HOWTO (R_MIPS_GNU_REL_HI16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_hi16_reloc,	/* special_function */
	 "R_MIPS_GNU_REL_HI16",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true);			/* pcrel_offset */

/* Low 16 bits of symbol value, pc-relative.  */
static reloc_howto_type elf_mips_gnu_rel_lo16 =
  HOWTO (R_MIPS_GNU_REL_LO16,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_mips_elf_lo16_reloc,	/* special_function */
	 "R_MIPS_GNU_REL_LO16",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true);			/* pcrel_offset */

/* 16 bit offset for pc-relative branches.  */
static reloc_howto_type elf_mips_gnu_rel16_s2 =
  HOWTO (R_MIPS_GNU_REL16_S2,	/* type */
	 2,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 16,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_GNU_REL16_S2",	/* name */
	 true,			/* partial_inplace */
	 0xffff,		/* src_mask */
	 0xffff,		/* dst_mask */
	 true);			/* pcrel_offset */

/* 64 bit pc-relative.  */
static reloc_howto_type elf_mips_gnu_pcrel64 =
  HOWTO (R_MIPS_PC64,		/* type */
	 0,			/* rightshift */
	 4,			/* size (0 = byte, 1 = short, 2 = long) */
	 64,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC64",		/* name */
	 true,			/* partial_inplace */
	 MINUS_ONE,		/* src_mask */
	 MINUS_ONE,		/* dst_mask */
	 true);			/* pcrel_offset */

/* 32 bit pc-relative.  */
static reloc_howto_type elf_mips_gnu_pcrel32 =
  HOWTO (R_MIPS_PC32,		/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 32,			/* bitsize */
	 true,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_signed, /* complain_on_overflow */
	 bfd_elf_generic_reloc,	/* special_function */
	 "R_MIPS_PC32",		/* name */
	 true,			/* partial_inplace */
	 0xffffffff,		/* src_mask */
	 0xffffffff,		/* dst_mask */
	 true);			/* pcrel_offset */

/* GNU extension to record C++ vtable hierarchy */
static reloc_howto_type elf_mips_gnu_vtinherit_howto =
  HOWTO (R_MIPS_GNU_VTINHERIT,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 NULL,			/* special_function */
	 "R_MIPS_GNU_VTINHERIT", /* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false);		/* pcrel_offset */

/* GNU extension to record C++ vtable member usage */
static reloc_howto_type elf_mips_gnu_vtentry_howto =
  HOWTO (R_MIPS_GNU_VTENTRY,	/* type */
	 0,			/* rightshift */
	 2,			/* size (0 = byte, 1 = short, 2 = long) */
	 0,			/* bitsize */
	 false,			/* pc_relative */
	 0,			/* bitpos */
	 complain_overflow_dont, /* complain_on_overflow */
	 _bfd_elf_rel_vtable_reloc_fn, /* special_function */
	 "R_MIPS_GNU_VTENTRY",	/* name */
	 false,			/* partial_inplace */
	 0,			/* src_mask */
	 0,			/* dst_mask */
	 false);		/* pcrel_offset */

/* Do a R_MIPS_HI16 relocation.  This has to be done in combination
   with a R_MIPS_LO16 reloc, because there is a carry from the LO16 to
   the HI16.  Here we just save the information we need; we do the
   actual relocation when we see the LO16.

   MIPS ELF requires that the LO16 immediately follow the HI16.  As a
   GNU extension, for non-pc-relative relocations, we permit an
   arbitrary number of HI16 relocs to be associated with a single LO16
   reloc.  This extension permits gcc to output the HI and LO relocs
   itself.

   This cannot be done for PC-relative relocations because both the HI16
   and LO16 parts of the relocations must be done relative to the LO16
   part, and there can be carry to or borrow from the HI16 part.  */

struct mips_hi16
{
  struct mips_hi16 *next;
  bfd_byte *addr;
  bfd_vma addend;
};

/* FIXME: This should not be a static variable.  */

static struct mips_hi16 *mips_hi16_list;

bfd_reloc_status_type
_bfd_mips_elf_hi16_reloc (abfd,
		     reloc_entry,
		     symbol,
		     data,
		     input_section,
		     output_bfd,
		     error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_reloc_status_type ret;
  bfd_vma relocation;
  struct mips_hi16 *n;

  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  ret = bfd_reloc_ok;

  if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
    {
      boolean relocateable;
      bfd_vma gp;

      if (ret == bfd_reloc_undefined)
	abort ();

      if (output_bfd != NULL)
	relocateable = true;
      else
	{
	  relocateable = false;
	  output_bfd = symbol->section->output_section->owner;
	}

      ret = mips_elf_final_gp (output_bfd, symbol, relocateable,
			       error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;

      relocation = gp - reloc_entry->address;
    }
  else
    {
      if (bfd_is_und_section (symbol->section)
	  && output_bfd == (bfd *) NULL)
	ret = bfd_reloc_undefined;

      if (bfd_is_com_section (symbol->section))
	relocation = 0;
      else
	relocation = symbol->value;
    }

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc_entry->addend;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Save the information, and let LO16 do the actual relocation.  */
  n = (struct mips_hi16 *) bfd_malloc ((bfd_size_type) sizeof *n);
  if (n == NULL)
    return bfd_reloc_outofrange;
  n->addr = (bfd_byte *) data + reloc_entry->address;
  n->addend = relocation;
  n->next = mips_hi16_list;
  mips_hi16_list = n;

  if (output_bfd != (bfd *) NULL)
    reloc_entry->address += input_section->output_offset;

  return ret;
}

/* Do a R_MIPS_LO16 relocation.  This is a straightforward 16 bit
   inplace relocation; this function exists in order to do the
   R_MIPS_HI16 relocation described above.  */

bfd_reloc_status_type
_bfd_mips_elf_lo16_reloc (abfd,
		     reloc_entry,
		     symbol,
		     data,
		     input_section,
		     output_bfd,
		     error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  arelent gp_disp_relent;

  if (mips_hi16_list != NULL)
    {
      struct mips_hi16 *l;

      l = mips_hi16_list;
      while (l != NULL)
	{
	  unsigned long insn;
	  unsigned long val;
	  unsigned long vallo;
	  struct mips_hi16 *next;

	  /* Do the HI16 relocation.  Note that we actually don't need
	     to know anything about the LO16 itself, except where to
	     find the low 16 bits of the addend needed by the LO16.  */
	  insn = bfd_get_32 (abfd, l->addr);
	  vallo = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

	  /* The low order 16 bits are always treated as a signed
	     value.  */
	  vallo = ((vallo & 0xffff) ^ 0x8000) - 0x8000;
	  val = ((insn & 0xffff) << 16) + vallo;
	  val += l->addend;

	  /* If PC-relative, we need to subtract out the address of the LO
	     half of the HI/LO.  (The actual relocation is relative
	     to that instruction.)  */
	  if (reloc_entry->howto->pc_relative)
	    val -= reloc_entry->address;

	  /* At this point, "val" has the value of the combined HI/LO
	     pair.  If the low order 16 bits (which will be used for
	     the LO16 insn) are negative, then we will need an
	     adjustment for the high order 16 bits.  */
	  val += 0x8000;
	  val = (val >> 16) & 0xffff;

	  insn &= ~ (bfd_vma) 0xffff;
	  insn |= val;
	  bfd_put_32 (abfd, (bfd_vma) insn, l->addr);

	  if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
	    {
	      gp_disp_relent = *reloc_entry;
	      reloc_entry = &gp_disp_relent;
	      reloc_entry->addend = l->addend;
	    }

	  next = l->next;
	  free (l);
	  l = next;
	}

      mips_hi16_list = NULL;
    }
  else if (strcmp (bfd_asymbol_name (symbol), "_gp_disp") == 0)
    {
      bfd_reloc_status_type ret;
      bfd_vma gp, relocation;

      /* FIXME: Does this case ever occur?  */

      ret = mips_elf_final_gp (output_bfd, symbol, true, error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;

      relocation = gp - reloc_entry->address;
      relocation += symbol->section->output_section->vma;
      relocation += symbol->section->output_offset;
      relocation += reloc_entry->addend;

      if (reloc_entry->address > input_section->_cooked_size)
	return bfd_reloc_outofrange;

      gp_disp_relent = *reloc_entry;
      reloc_entry = &gp_disp_relent;
      reloc_entry->addend = relocation - 4;
    }

  /* Now do the LO16 reloc in the usual way.  */
  return bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
				input_section, output_bfd, error_message);
}

/* Do a R_MIPS_GOT16 reloc.  This is a reloc against the global offset
   table used for PIC code.  If the symbol is an external symbol, the
   instruction is modified to contain the offset of the appropriate
   entry in the global offset table.  If the symbol is a section
   symbol, the next reloc is a R_MIPS_LO16 reloc.  The two 16 bit
   addends are combined to form the real addend against the section
   symbol; the GOT16 is modified to contain the offset of an entry in
   the global offset table, and the LO16 is modified to offset it
   appropriately.  Thus an offset larger than 16 bits requires a
   modified value in the global offset table.

   This implementation suffices for the assembler, but the linker does
   not yet know how to create global offset tables.  */

bfd_reloc_status_type
_bfd_mips_elf_got16_reloc (abfd,
		      reloc_entry,
		      symbol,
		      data,
		      input_section,
		      output_bfd,
		      error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  /* If we're relocating, and this an external symbol, we don't want
     to change anything.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* If we're relocating, and this is a local symbol, we can handle it
     just like HI16.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) != 0)
    return _bfd_mips_elf_hi16_reloc (abfd, reloc_entry, symbol, data,
				     input_section, output_bfd, error_message);

  abort ();
}

/* Set the GP value for OUTPUT_BFD.  Returns false if this is a
   dangerous relocation.  */

static boolean
mips_elf_assign_gp (output_bfd, pgp)
     bfd *output_bfd;
     bfd_vma *pgp;
{
  unsigned int count;
  asymbol **sym;
  unsigned int i;

  /* If we've already figured out what GP will be, just return it.  */
  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp)
    return true;

  count = bfd_get_symcount (output_bfd);
  sym = bfd_get_outsymbols (output_bfd);

  /* The linker script will have created a symbol named `_gp' with the
     appropriate value.  */
  if (sym == (asymbol **) NULL)
    i = count;
  else
    {
      for (i = 0; i < count; i++, sym++)
	{
	  register const char *name;

	  name = bfd_asymbol_name (*sym);
	  if (*name == '_' && strcmp (name, "_gp") == 0)
	    {
	      *pgp = bfd_asymbol_value (*sym);
	      _bfd_set_gp_value (output_bfd, *pgp);
	      break;
	    }
	}
    }

  if (i >= count)
    {
      /* Only get the error once.  */
      *pgp = 4;
      _bfd_set_gp_value (output_bfd, *pgp);
      return false;
    }

  return true;
}

/* We have to figure out the gp value, so that we can adjust the
   symbol value correctly.  We look up the symbol _gp in the output
   BFD.  If we can't find it, we're stuck.  We cache it in the ELF
   target data.  We don't need to adjust the symbol value for an
   external symbol if we are producing relocateable output.  */

static bfd_reloc_status_type
mips_elf_final_gp (output_bfd, symbol, relocateable, error_message, pgp)
     bfd *output_bfd;
     asymbol *symbol;
     boolean relocateable;
     char **error_message;
     bfd_vma *pgp;
{
  if (bfd_is_und_section (symbol->section)
      && ! relocateable)
    {
      *pgp = 0;
      return bfd_reloc_undefined;
    }

  *pgp = _bfd_get_gp_value (output_bfd);
  if (*pgp == 0
      && (! relocateable
	  || (symbol->flags & BSF_SECTION_SYM) != 0))
    {
      if (relocateable)
	{
	  /* Make up a value.  */
	  *pgp = symbol->section->output_section->vma + 0x4000;
	  _bfd_set_gp_value (output_bfd, *pgp);
	}
      else if (!mips_elf_assign_gp (output_bfd, pgp))
	{
	  *error_message =
	    (char *) _("GP relative relocation when _gp not defined");
	  return bfd_reloc_dangerous;
	}
    }

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL16 relocation.  This is a 16 bit value which must
   become the offset from the gp register.  This function also handles
   R_MIPS_LITERAL relocations, although those can be handled more
   cleverly because the entries in the .lit8 and .lit4 sections can be
   merged.  */

static bfd_reloc_status_type gprel16_with_gp PARAMS ((bfd *, asymbol *,
						      arelent *, asection *,
						      boolean, PTR, bfd_vma));

bfd_reloc_status_type
_bfd_mips_elf_gprel16_reloc (abfd, reloc_entry, symbol, data, input_section,
			     output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != (bfd *) NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
			   &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  return gprel16_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, gp);
}

static bfd_reloc_status_type
gprel16_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long insn;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  insn = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case occurs with the 64-bit MIPS ELF ABI.  */
      val = reloc_entry->addend;
    }
  else
    {
      val = ((insn & 0xffff) + reloc_entry->addend) & 0xffff;
      if (val & 0x8000)
	val -= 0x10000;
    }

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  insn = (insn & ~0xffff) | (val & 0xffff);
  bfd_put_32 (abfd, insn, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  /* Make sure it fit in 16 bits.  */
  if ((long) val >= 0x8000 || (long) val < -0x8000)
    return bfd_reloc_overflow;

  return bfd_reloc_ok;
}

/* Do a R_MIPS_GPREL32 relocation.  Is this 32 bit value the offset
   from the gp register? XXX */

static bfd_reloc_status_type gprel32_with_gp PARAMS ((bfd *, asymbol *,
						      arelent *, asection *,
						      boolean, PTR, bfd_vma));

bfd_reloc_status_type
_bfd_mips_elf_gprel32_reloc (abfd,
			reloc_entry,
			symbol,
			data,
			input_section,
			output_bfd,
			error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      *error_message = (char *)
	_("32bits gp relative relocation occurs for an external symbol");
      return bfd_reloc_outofrange;
    }

  if (output_bfd != (bfd *) NULL)
    {
      relocateable = true;
      gp = _bfd_get_gp_value (output_bfd);
    }
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;

      ret = mips_elf_final_gp (output_bfd, symbol, relocateable,
			       error_message, &gp);
      if (ret != bfd_reloc_ok)
	return ret;
    }

  return gprel32_with_gp (abfd, symbol, reloc_entry, input_section,
			  relocateable, data, gp);
}

static bfd_reloc_status_type
gprel32_with_gp (abfd, symbol, reloc_entry, input_section, relocateable, data,
		 gp)
     bfd *abfd;
     asymbol *symbol;
     arelent *reloc_entry;
     asection *input_section;
     boolean relocateable;
     PTR data;
     bfd_vma gp;
{
  bfd_vma relocation;
  unsigned long val;

  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  if (reloc_entry->howto->src_mask == 0)
    {
      /* This case arises with the 64-bit MIPS ELF ABI.  */
      val = 0;
    }
  else
    val = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);

  /* Set val to the offset into the section or symbol.  */
  val += reloc_entry->addend;

  /* Adjust val for the final section location and GP value.  If we
     are producing relocateable output, we don't want to do this for
     an external symbol.  */
  if (! relocateable
      || (symbol->flags & BSF_SECTION_SYM) != 0)
    val += relocation - gp;

  bfd_put_32 (abfd, (bfd_vma) val, (bfd_byte *) data + reloc_entry->address);

  if (relocateable)
    reloc_entry->address += input_section->output_offset;

  return bfd_reloc_ok;
}

/* Handle a 64 bit reloc in a 32 bit MIPS ELF file.  These are
   generated when addresses are 64 bits.  The upper 32 bits are a simple
   sign extension.  */

static bfd_reloc_status_type
mips32_64bit_reloc (abfd, reloc_entry, symbol, data, input_section,
		    output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  bfd_reloc_status_type r;
  arelent reloc32;
  unsigned long val;
  bfd_size_type addr;

  r = bfd_elf_generic_reloc (abfd, reloc_entry, symbol, data,
			     input_section, output_bfd, error_message);
  if (r != bfd_reloc_continue)
    return r;

  /* Do a normal 32 bit relocation on the lower 32 bits.  */
  reloc32 = *reloc_entry;
  if (bfd_big_endian (abfd))
    reloc32.address += 4;
  reloc32.howto = &elf_mips_howto_table_rel[R_MIPS_32];
  r = bfd_perform_relocation (abfd, &reloc32, data, input_section,
			      output_bfd, error_message);

  /* Sign extend into the upper 32 bits.  */
  val = bfd_get_32 (abfd, (bfd_byte *) data + reloc32.address);
  if ((val & 0x80000000) != 0)
    val = 0xffffffff;
  else
    val = 0;
  addr = reloc_entry->address;
  if (bfd_little_endian (abfd))
    addr += 4;
  bfd_put_32 (abfd, (bfd_vma) val, (bfd_byte *) data + addr);

  return r;
}

/* Handle a mips16 jump.  */

static bfd_reloc_status_type
mips16_jump_reloc (abfd, reloc_entry, symbol, data, input_section,
		   output_bfd, error_message)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd *output_bfd;
     char **error_message ATTRIBUTE_UNUSED;
{
  if (output_bfd != (bfd *) NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* FIXME.  */
  {
    static boolean warned;

    if (! warned)
      (*_bfd_error_handler)
	(_("Linking mips16 objects into %s format is not supported"),
	 bfd_get_target (input_section->output_section->owner));
    warned = true;
  }

  return bfd_reloc_undefined;
}

/* Handle a mips16 GP relative reloc.  */

static bfd_reloc_status_type
mips16_gprel_reloc (abfd, reloc_entry, symbol, data, input_section,
		    output_bfd, error_message)
     bfd *abfd;
     arelent *reloc_entry;
     asymbol *symbol;
     PTR data;
     asection *input_section;
     bfd *output_bfd;
     char **error_message;
{
  boolean relocateable;
  bfd_reloc_status_type ret;
  bfd_vma gp;
  unsigned short extend, insn;
  unsigned long final;

  /* If we're relocating, and this is an external symbol with no
     addend, we don't want to change anything.  We will only have an
     addend if this is a newly created reloc, not read from an ELF
     file.  */
  if (output_bfd != NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && reloc_entry->addend == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  if (output_bfd != NULL)
    relocateable = true;
  else
    {
      relocateable = false;
      output_bfd = symbol->section->output_section->owner;
    }

  ret = mips_elf_final_gp (output_bfd, symbol, relocateable, error_message,
			   &gp);
  if (ret != bfd_reloc_ok)
    return ret;

  if (reloc_entry->address > input_section->_cooked_size)
    return bfd_reloc_outofrange;

  /* Pick up the mips16 extend instruction and the real instruction.  */
  extend = bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address);
  insn = bfd_get_16 (abfd, (bfd_byte *) data + reloc_entry->address + 2);

  /* Stuff the current addend back as a 32 bit value, do the usual
     relocation, and then clean up.  */
  bfd_put_32 (abfd,
	      (bfd_vma) (((extend & 0x1f) << 11)
			 | (extend & 0x7e0)
			 | (insn & 0x1f)),
	      (bfd_byte *) data + reloc_entry->address);

  ret = gprel16_with_gp (abfd, symbol, reloc_entry, input_section,
			 relocateable, data, gp);

  final = bfd_get_32 (abfd, (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd,
	      (bfd_vma) ((extend & 0xf800)
			 | ((final >> 11) & 0x1f)
			 | (final & 0x7e0)),
	      (bfd_byte *) data + reloc_entry->address);
  bfd_put_16 (abfd,
	      (bfd_vma) ((insn & 0xffe0)
			 | (final & 0x1f)),
	      (bfd_byte *) data + reloc_entry->address + 2);

  return ret;
}

/* Return the ISA for a MIPS e_flags value.  */

static INLINE int
elf_mips_isa (flags)
     flagword flags;
{
  switch (flags & EF_MIPS_ARCH)
    {
    case E_MIPS_ARCH_1:
      return 1;
    case E_MIPS_ARCH_2:
      return 2;
    case E_MIPS_ARCH_3:
      return 3;
    case E_MIPS_ARCH_4:
      return 4;
    case E_MIPS_ARCH_5:
      return 5;
    case E_MIPS_ARCH_32:
      return 32;
    case E_MIPS_ARCH_64:
      return 64;
    }
  return 4;
}

/* Return the MACH for a MIPS e_flags value.  */

static INLINE unsigned long
elf_mips_mach (flags)
     flagword flags;
{
  switch (flags & EF_MIPS_MACH)
    {
    case E_MIPS_MACH_3900:
      return bfd_mach_mips3900;

    case E_MIPS_MACH_4010:
      return bfd_mach_mips4010;

    case E_MIPS_MACH_4100:
      return bfd_mach_mips4100;

    case E_MIPS_MACH_4111:
      return bfd_mach_mips4111;

    case E_MIPS_MACH_4650:
      return bfd_mach_mips4650;

    case E_MIPS_MACH_SB1:
      return bfd_mach_mips_sb1;

    default:
      switch (flags & EF_MIPS_ARCH)
	{
	default:
	case E_MIPS_ARCH_1:
	  return bfd_mach_mips3000;
	  break;

	case E_MIPS_ARCH_2:
	  return bfd_mach_mips6000;
	  break;

	case E_MIPS_ARCH_3:
	  return bfd_mach_mips4000;
	  break;

	case E_MIPS_ARCH_4:
	  return bfd_mach_mips8000;
	  break;

	case E_MIPS_ARCH_5:
	  return bfd_mach_mips5;
	  break;

	case E_MIPS_ARCH_32:
	  return bfd_mach_mipsisa32;
	  break;

	case E_MIPS_ARCH_64:
	  return bfd_mach_mipsisa64;
	  break;
	}
    }

  return 0;
}

/* Return printable name for ABI.  */

static INLINE char *
elf_mips_abi_name (abfd)
     bfd *abfd;
{
  flagword flags;

  flags = elf_elfheader (abfd)->e_flags;
  switch (flags & EF_MIPS_ABI)
    {
    case 0:
      if (ABI_N32_P (abfd))
	return "N32";
      else if (ABI_64_P (abfd))
	return "64";
      else
	return "none";
    case E_MIPS_ABI_O32:
      return "O32";
    case E_MIPS_ABI_O64:
      return "O64";
    case E_MIPS_ABI_EABI32:
      return "EABI32";
    case E_MIPS_ABI_EABI64:
      return "EABI64";
    default:
      return "unknown abi";
    }
}

/* A mapping from BFD reloc types to MIPS ELF reloc types.  */

struct elf_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  enum elf_mips_reloc_type elf_reloc_val;
};

static const struct elf_reloc_map mips_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MIPS_NONE, },
  { BFD_RELOC_16, R_MIPS_16 },
  { BFD_RELOC_32, R_MIPS_32 },
  { BFD_RELOC_64, R_MIPS_64 },
  { BFD_RELOC_MIPS_JMP, R_MIPS_26 },
  { BFD_RELOC_HI16_S, R_MIPS_HI16 },
  { BFD_RELOC_LO16, R_MIPS_LO16 },
  { BFD_RELOC_GPREL16, R_MIPS_GPREL16 },
  { BFD_RELOC_MIPS_LITERAL, R_MIPS_LITERAL },
  { BFD_RELOC_MIPS_GOT16, R_MIPS_GOT16 },
  { BFD_RELOC_16_PCREL, R_MIPS_PC16 },
  { BFD_RELOC_MIPS_CALL16, R_MIPS_CALL16 },
  { BFD_RELOC_GPREL32, R_MIPS_GPREL32 },
  { BFD_RELOC_MIPS_GOT_HI16, R_MIPS_GOT_HI16 },
  { BFD_RELOC_MIPS_GOT_LO16, R_MIPS_GOT_LO16 },
  { BFD_RELOC_MIPS_CALL_HI16, R_MIPS_CALL_HI16 },
  { BFD_RELOC_MIPS_CALL_LO16, R_MIPS_CALL_LO16 },
  { BFD_RELOC_MIPS_SUB, R_MIPS_SUB },
  { BFD_RELOC_MIPS_GOT_PAGE, R_MIPS_GOT_PAGE },
  { BFD_RELOC_MIPS_GOT_OFST, R_MIPS_GOT_OFST },
  { BFD_RELOC_MIPS_GOT_DISP, R_MIPS_GOT_DISP }
};

/* Given a BFD reloc type, return a howto structure.  */

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0; i < sizeof (mips_reloc_map) / sizeof (struct elf_reloc_map); i++)
    {
      if (mips_reloc_map[i].bfd_reloc_val == code)
	return &elf_mips_howto_table_rel[(int) mips_reloc_map[i].elf_reloc_val];
    }

  switch (code)
    {
    default:
      bfd_set_error (bfd_error_bad_value);
      return NULL;

    case BFD_RELOC_CTOR:
      /* We need to handle BFD_RELOC_CTOR specially.
	 Select the right relocation (R_MIPS_32 or R_MIPS_64) based on the
	 size of addresses on this architecture.  */
      if (bfd_arch_bits_per_address (abfd) == 32)
	return &elf_mips_howto_table_rel[(int) R_MIPS_32];
      else
	return &elf_mips_ctor64_howto;

    case BFD_RELOC_MIPS16_JMP:
      return &elf_mips16_jump_howto;
    case BFD_RELOC_MIPS16_GPREL:
      return &elf_mips16_gprel_howto;
    case BFD_RELOC_VTABLE_INHERIT:
      return &elf_mips_gnu_vtinherit_howto;
    case BFD_RELOC_VTABLE_ENTRY:
      return &elf_mips_gnu_vtentry_howto;
    case BFD_RELOC_PCREL_HI16_S:
      return &elf_mips_gnu_rel_hi16;
    case BFD_RELOC_PCREL_LO16:
      return &elf_mips_gnu_rel_lo16;
    case BFD_RELOC_16_PCREL_S2:
      return &elf_mips_gnu_rel16_s2;
    case BFD_RELOC_64_PCREL:
      return &elf_mips_gnu_pcrel64;
    case BFD_RELOC_32_PCREL:
      return &elf_mips_gnu_pcrel32;
    }
}

/* Given a MIPS Elf32_Internal_Rel, fill in an arelent structure.  */

static reloc_howto_type *
mips_rtype_to_howto (r_type)
     unsigned int r_type;
{
  switch (r_type)
    {
    case R_MIPS16_26:
      return &elf_mips16_jump_howto;
      break;
    case R_MIPS16_GPREL:
      return &elf_mips16_gprel_howto;
      break;
    case R_MIPS_GNU_VTINHERIT:
      return &elf_mips_gnu_vtinherit_howto;
      break;
    case R_MIPS_GNU_VTENTRY:
      return &elf_mips_gnu_vtentry_howto;
      break;
    case R_MIPS_GNU_REL_HI16:
      return &elf_mips_gnu_rel_hi16;
      break;
    case R_MIPS_GNU_REL_LO16:
      return &elf_mips_gnu_rel_lo16;
      break;
    case R_MIPS_GNU_REL16_S2:
      return &elf_mips_gnu_rel16_s2;
      break;
    case R_MIPS_PC64:
      return &elf_mips_gnu_pcrel64;
      break;
    case R_MIPS_PC32:
      return &elf_mips_gnu_pcrel32;
      break;

    default:
      BFD_ASSERT (r_type < (unsigned int) R_MIPS_max);
      return &elf_mips_howto_table_rel[r_type];
      break;
    }
}

/* Given a MIPS Elf32_Internal_Rel, fill in an arelent structure.  */

static void
mips_info_to_howto_rel (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rel *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  cache_ptr->howto = mips_rtype_to_howto (r_type);

  /* The addend for a GPREL16 or LITERAL relocation comes from the GP
     value for the object file.  We get the addend now, rather than
     when we do the relocation, because the symbol manipulations done
     by the linker may cause us to lose track of the input BFD.  */
  if (((*cache_ptr->sym_ptr_ptr)->flags & BSF_SECTION_SYM) != 0
      && (r_type == (unsigned int) R_MIPS_GPREL16
	  || r_type == (unsigned int) R_MIPS_LITERAL))
    cache_ptr->addend = elf_gp (abfd);
}

/* Given a MIPS Elf32_Internal_Rela, fill in an arelent structure.  */

static void
mips_info_to_howto_rela (abfd, cache_ptr, dst)
     bfd *abfd;
     arelent *cache_ptr;
     Elf32_Internal_Rela *dst;
{
  /* Since an Elf32_Internal_Rel is an initial prefix of an
     Elf32_Internal_Rela, we can just use mips_info_to_howto_rel
     above.  */
  mips_info_to_howto_rel (abfd, cache_ptr, (Elf32_Internal_Rel *) dst);

  /* If we ever need to do any extra processing with dst->r_addend
     (the field omitted in an Elf32_Internal_Rel) we can do it here.  */
}

/* A .reginfo section holds a single Elf32_RegInfo structure.  These
   routines swap this structure in and out.  They are used outside of
   BFD, so they are globally visible.  */

void
bfd_mips_elf32_swap_reginfo_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_RegInfo *ex;
     Elf32_RegInfo *in;
{
  in->ri_gprmask = H_GET_32 (abfd, ex->ri_gprmask);
  in->ri_cprmask[0] = H_GET_32 (abfd, ex->ri_cprmask[0]);
  in->ri_cprmask[1] = H_GET_32 (abfd, ex->ri_cprmask[1]);
  in->ri_cprmask[2] = H_GET_32 (abfd, ex->ri_cprmask[2]);
  in->ri_cprmask[3] = H_GET_32 (abfd, ex->ri_cprmask[3]);
  in->ri_gp_value = H_GET_32 (abfd, ex->ri_gp_value);
}

void
bfd_mips_elf32_swap_reginfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_RegInfo *in;
     Elf32_External_RegInfo *ex;
{
  H_PUT_32 (abfd, in->ri_gprmask, ex->ri_gprmask);
  H_PUT_32 (abfd, in->ri_cprmask[0], ex->ri_cprmask[0]);
  H_PUT_32 (abfd, in->ri_cprmask[1], ex->ri_cprmask[1]);
  H_PUT_32 (abfd, in->ri_cprmask[2], ex->ri_cprmask[2]);
  H_PUT_32 (abfd, in->ri_cprmask[3], ex->ri_cprmask[3]);
  H_PUT_32 (abfd, in->ri_gp_value, ex->ri_gp_value);
}

/* In the 64 bit ABI, the .MIPS.options section holds register
   information in an Elf64_Reginfo structure.  These routines swap
   them in and out.  They are globally visible because they are used
   outside of BFD.  These routines are here so that gas can call them
   without worrying about whether the 64 bit ABI has been included.  */

void
bfd_mips_elf64_swap_reginfo_in (abfd, ex, in)
     bfd *abfd;
     const Elf64_External_RegInfo *ex;
     Elf64_Internal_RegInfo *in;
{
  in->ri_gprmask = H_GET_32 (abfd, ex->ri_gprmask);
  in->ri_pad = H_GET_32 (abfd, ex->ri_pad);
  in->ri_cprmask[0] = H_GET_32 (abfd, ex->ri_cprmask[0]);
  in->ri_cprmask[1] = H_GET_32 (abfd, ex->ri_cprmask[1]);
  in->ri_cprmask[2] = H_GET_32 (abfd, ex->ri_cprmask[2]);
  in->ri_cprmask[3] = H_GET_32 (abfd, ex->ri_cprmask[3]);
  in->ri_gp_value = H_GET_64 (abfd, ex->ri_gp_value);
}

void
bfd_mips_elf64_swap_reginfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf64_Internal_RegInfo *in;
     Elf64_External_RegInfo *ex;
{
  H_PUT_32 (abfd, in->ri_gprmask, ex->ri_gprmask);
  H_PUT_32 (abfd, in->ri_pad, ex->ri_pad);
  H_PUT_32 (abfd, in->ri_cprmask[0], ex->ri_cprmask[0]);
  H_PUT_32 (abfd, in->ri_cprmask[1], ex->ri_cprmask[1]);
  H_PUT_32 (abfd, in->ri_cprmask[2], ex->ri_cprmask[2]);
  H_PUT_32 (abfd, in->ri_cprmask[3], ex->ri_cprmask[3]);
  H_PUT_64 (abfd, in->ri_gp_value, ex->ri_gp_value);
}

/* Swap an entry in a .gptab section.  Note that these routines rely
   on the equivalence of the two elements of the union.  */

static void
bfd_mips_elf32_swap_gptab_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_gptab *ex;
     Elf32_gptab *in;
{
  in->gt_entry.gt_g_value = H_GET_32 (abfd, ex->gt_entry.gt_g_value);
  in->gt_entry.gt_bytes = H_GET_32 (abfd, ex->gt_entry.gt_bytes);
}

static void
bfd_mips_elf32_swap_gptab_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_gptab *in;
     Elf32_External_gptab *ex;
{
  H_PUT_32 (abfd, in->gt_entry.gt_g_value, ex->gt_entry.gt_g_value);
  H_PUT_32 (abfd, in->gt_entry.gt_bytes, ex->gt_entry.gt_bytes);
}

static void
bfd_elf32_swap_compact_rel_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_compact_rel *in;
     Elf32_External_compact_rel *ex;
{
  H_PUT_32 (abfd, in->id1, ex->id1);
  H_PUT_32 (abfd, in->num, ex->num);
  H_PUT_32 (abfd, in->id2, ex->id2);
  H_PUT_32 (abfd, in->offset, ex->offset);
  H_PUT_32 (abfd, in->reserved0, ex->reserved0);
  H_PUT_32 (abfd, in->reserved1, ex->reserved1);
}

static void
bfd_elf32_swap_crinfo_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_crinfo *in;
     Elf32_External_crinfo *ex;
{
  unsigned long l;

  l = (((in->ctype & CRINFO_CTYPE) << CRINFO_CTYPE_SH)
       | ((in->rtype & CRINFO_RTYPE) << CRINFO_RTYPE_SH)
       | ((in->dist2to & CRINFO_DIST2TO) << CRINFO_DIST2TO_SH)
       | ((in->relvaddr & CRINFO_RELVADDR) << CRINFO_RELVADDR_SH));
  H_PUT_32 (abfd, l, ex->info);
  H_PUT_32 (abfd, in->konst, ex->konst);
  H_PUT_32 (abfd, in->vaddr, ex->vaddr);
}

/* Swap in an options header.  */

void
bfd_mips_elf_swap_options_in (abfd, ex, in)
     bfd *abfd;
     const Elf_External_Options *ex;
     Elf_Internal_Options *in;
{
  in->kind = H_GET_8 (abfd, ex->kind);
  in->size = H_GET_8 (abfd, ex->size);
  in->section = H_GET_16 (abfd, ex->section);
  in->info = H_GET_32 (abfd, ex->info);
}

/* Swap out an options header.  */

void
bfd_mips_elf_swap_options_out (abfd, in, ex)
     bfd *abfd;
     const Elf_Internal_Options *in;
     Elf_External_Options *ex;
{
  H_PUT_8 (abfd, in->kind, ex->kind);
  H_PUT_8 (abfd, in->size, ex->size);
  H_PUT_16 (abfd, in->section, ex->section);
  H_PUT_32 (abfd, in->info, ex->info);
}
#if 0
/* Swap in an MSYM entry.  */

static void
bfd_mips_elf_swap_msym_in (abfd, ex, in)
     bfd *abfd;
     const Elf32_External_Msym *ex;
     Elf32_Internal_Msym *in;
{
  in->ms_hash_value = H_GET_32 (abfd, ex->ms_hash_value);
  in->ms_info = H_GET_32 (abfd, ex->ms_info);
}
#endif
/* Swap out an MSYM entry.  */

static void
bfd_mips_elf_swap_msym_out (abfd, in, ex)
     bfd *abfd;
     const Elf32_Internal_Msym *in;
     Elf32_External_Msym *ex;
{
  H_PUT_32 (abfd, in->ms_hash_value, ex->ms_hash_value);
  H_PUT_32 (abfd, in->ms_info, ex->ms_info);
}

/* Determine whether a symbol is global for the purposes of splitting
   the symbol table into global symbols and local symbols.  At least
   on Irix 5, this split must be between section symbols and all other
   symbols.  On most ELF targets the split is between static symbols
   and externally visible symbols.  */

static boolean
mips_elf_sym_is_global (abfd, sym)
     bfd *abfd ATTRIBUTE_UNUSED;
     asymbol *sym;
{
  if (SGI_COMPAT (abfd))
    return (sym->flags & BSF_SECTION_SYM) == 0;
  else
    return ((sym->flags & (BSF_GLOBAL | BSF_WEAK)) != 0
	    || bfd_is_und_section (bfd_get_section (sym))
	    || bfd_is_com_section (bfd_get_section (sym)));
}

/* Set the right machine number for a MIPS ELF file.  This is used for
   both the 32-bit and the 64-bit ABI.  */

boolean
_bfd_mips_elf_object_p (abfd)
     bfd *abfd;
{
  /* Irix 5 and 6 are broken.  Object file symbol tables are not always
     sorted correctly such that local symbols precede global symbols,
     and the sh_info field in the symbol table is not always right.  */
  if (SGI_COMPAT(abfd))
    elf_bad_symtab (abfd) = true;

  bfd_default_set_arch_mach (abfd, bfd_arch_mips,
			     elf_mips_mach (elf_elfheader (abfd)->e_flags));
  return true;
}

/* The final processing done just before writing out a MIPS ELF object
   file.  This gets the MIPS architecture right based on the machine
   number.  This is used by both the 32-bit and the 64-bit ABI.  */

void
_bfd_mips_elf_final_write_processing (abfd, linker)
     bfd *abfd;
     boolean linker ATTRIBUTE_UNUSED;
{
  unsigned long val;
  unsigned int i;
  Elf_Internal_Shdr **hdrpp;
  const char *name;
  asection *sec;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_mips3000:
      val = E_MIPS_ARCH_1;
      break;

    case bfd_mach_mips3900:
      val = E_MIPS_ARCH_1 | E_MIPS_MACH_3900;
      break;

    case bfd_mach_mips6000:
      val = E_MIPS_ARCH_2;
      break;

    case bfd_mach_mips4000:
    case bfd_mach_mips4300:
    case bfd_mach_mips4400:
    case bfd_mach_mips4600:
      val = E_MIPS_ARCH_3;
      break;

    case bfd_mach_mips4010:
      val = E_MIPS_ARCH_3 | E_MIPS_MACH_4010;
      break;

    case bfd_mach_mips4100:
      val = E_MIPS_ARCH_3 | E_MIPS_MACH_4100;
      break;

    case bfd_mach_mips4111:
      val = E_MIPS_ARCH_3 | E_MIPS_MACH_4111;
      break;

    case bfd_mach_mips4650:
      val = E_MIPS_ARCH_3 | E_MIPS_MACH_4650;
      break;

    case bfd_mach_mips5000:
    case bfd_mach_mips8000:
    case bfd_mach_mips10000:
    case bfd_mach_mips12000:
      val = E_MIPS_ARCH_4;
      break;

    case bfd_mach_mips5:
      val = E_MIPS_ARCH_5;
      break;

    case bfd_mach_mips_sb1:
      val = E_MIPS_ARCH_64 | E_MIPS_MACH_SB1;
      break;

    case bfd_mach_mipsisa32:
      val = E_MIPS_ARCH_32;
      break;

    case bfd_mach_mipsisa64:
      val = E_MIPS_ARCH_64;
    }

  elf_elfheader (abfd)->e_flags &= ~(EF_MIPS_ARCH | EF_MIPS_MACH);
  elf_elfheader (abfd)->e_flags |= val;

  /* Set the sh_info field for .gptab sections and other appropriate
     info for each special section.  */
  for (i = 1, hdrpp = elf_elfsections (abfd) + 1;
       i < elf_numsections (abfd);
       i++, hdrpp++)
    {
      switch ((*hdrpp)->sh_type)
	{
	case SHT_MIPS_MSYM:
	case SHT_MIPS_LIBLIST:
	  sec = bfd_get_section_by_name (abfd, ".dynstr");
	  if (sec != NULL)
	    (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_GPTAB:
	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL
		      && strncmp (name, ".gptab.", sizeof ".gptab." - 1) == 0);
	  sec = bfd_get_section_by_name (abfd, name + sizeof ".gptab" - 1);
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_info = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_CONTENT:
	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL
		      && strncmp (name, ".MIPS.content",
				  sizeof ".MIPS.content" - 1) == 0);
	  sec = bfd_get_section_by_name (abfd,
					 name + sizeof ".MIPS.content" - 1);
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_SYMBOL_LIB:
	  sec = bfd_get_section_by_name (abfd, ".dynsym");
	  if (sec != NULL)
	    (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  sec = bfd_get_section_by_name (abfd, ".liblist");
	  if (sec != NULL)
	    (*hdrpp)->sh_info = elf_section_data (sec)->this_idx;
	  break;

	case SHT_MIPS_EVENTS:
	  BFD_ASSERT ((*hdrpp)->bfd_section != NULL);
	  name = bfd_get_section_name (abfd, (*hdrpp)->bfd_section);
	  BFD_ASSERT (name != NULL);
	  if (strncmp (name, ".MIPS.events", sizeof ".MIPS.events" - 1) == 0)
	    sec = bfd_get_section_by_name (abfd,
					   name + sizeof ".MIPS.events" - 1);
	  else
	    {
	      BFD_ASSERT (strncmp (name, ".MIPS.post_rel",
				   sizeof ".MIPS.post_rel" - 1) == 0);
	      sec = bfd_get_section_by_name (abfd,
					     (name
					      + sizeof ".MIPS.post_rel" - 1));
	    }
	  BFD_ASSERT (sec != NULL);
	  (*hdrpp)->sh_link = elf_section_data (sec)->this_idx;
	  break;

	}
    }
}

/* Function to keep MIPS specific file flags like as EF_MIPS_PIC.  */

boolean
_bfd_mips_elf_set_private_flags (abfd, flags)
     bfd *abfd;
     flagword flags;
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = true;
  return true;
}

/* Merge backend specific data from an object file to the output
   object file when linking.  */

boolean
_bfd_mips_elf_merge_private_bfd_data (ibfd, obfd)
     bfd *ibfd;
     bfd *obfd;
{
  flagword old_flags;
  flagword new_flags;
  boolean ok;
  boolean null_input_bfd = true;
  asection *sec;

  /* Check if we have the same endianess */
  if (_bfd_generic_verify_endian_match (ibfd, obfd) == false)
    return false;

  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return true;

  new_flags = elf_elfheader (ibfd)->e_flags;
  elf_elfheader (obfd)->e_flags |= new_flags & EF_MIPS_NOREORDER;
  old_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = true;
      elf_elfheader (obfd)->e_flags = new_flags;
      elf_elfheader (obfd)->e_ident[EI_CLASS]
	= elf_elfheader (ibfd)->e_ident[EI_CLASS];

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	{
	  if (! bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
				   bfd_get_mach (ibfd)))
	    return false;
	}

      return true;
    }

  /* Check flag compatibility.  */

  new_flags &= ~EF_MIPS_NOREORDER;
  old_flags &= ~EF_MIPS_NOREORDER;

  if (new_flags == old_flags)
    return true;

  /* Check to see if the input BFD actually contains any sections.
     If not, its flags may not have been initialised either, but it cannot
     actually cause any incompatibility.  */
  for (sec = ibfd->sections; sec != NULL; sec = sec->next)
    {
      /* Ignore synthetic sections and empty .text, .data and .bss sections
	  which are automatically generated by gas.  */
      if (strcmp (sec->name, ".reginfo")
	  && strcmp (sec->name, ".mdebug")
	  && ((!strcmp (sec->name, ".text")
	       || !strcmp (sec->name, ".data")
	       || !strcmp (sec->name, ".bss"))
	      && sec->_raw_size != 0))
	{
	  null_input_bfd = false;
	  break;
	}
    }
  if (null_input_bfd)
    return true;

  ok = true;

  if ((new_flags & EF_MIPS_PIC) != (old_flags & EF_MIPS_PIC))
    {
      new_flags &= ~EF_MIPS_PIC;
      old_flags &= ~EF_MIPS_PIC;
      (*_bfd_error_handler)
	(_("%s: linking PIC files with non-PIC files"),
	 bfd_archive_filename (ibfd));
      ok = false;
    }

  if ((new_flags & EF_MIPS_CPIC) != (old_flags & EF_MIPS_CPIC))
    {
      new_flags &= ~EF_MIPS_CPIC;
      old_flags &= ~EF_MIPS_CPIC;
      (*_bfd_error_handler)
	(_("%s: linking abicalls files with non-abicalls files"),
	 bfd_archive_filename (ibfd));
      ok = false;
    }

  /* Compare the ISA's.  */
  if ((new_flags & (EF_MIPS_ARCH | EF_MIPS_MACH))
      != (old_flags & (EF_MIPS_ARCH | EF_MIPS_MACH)))
    {
      int new_mach = new_flags & EF_MIPS_MACH;
      int old_mach = old_flags & EF_MIPS_MACH;
      int new_isa = elf_mips_isa (new_flags);
      int old_isa = elf_mips_isa (old_flags);

      /* If either has no machine specified, just compare the general isa's.
	 Some combinations of machines are ok, if the isa's match.  */
      if (! new_mach
	  || ! old_mach
	  || new_mach == old_mach
	  )
	{
	  /* Don't warn about mixing code using 32-bit ISAs, or mixing code
	     using 64-bit ISAs.  They will normally use the same data sizes
	     and calling conventions.  */

	  if ((  (new_isa == 1 || new_isa == 2 || new_isa == 32)
	       ^ (old_isa == 1 || old_isa == 2 || old_isa == 32)) != 0)
	    {
	      (*_bfd_error_handler)
	       (_("%s: ISA mismatch (-mips%d) with previous modules (-mips%d)"),
		bfd_archive_filename (ibfd), new_isa, old_isa);
	      ok = false;
	    }
	  else
	    {
	      /* Do we need to update the mach field?  */
	      if (old_mach == 0 && new_mach != 0) 
		elf_elfheader (obfd)->e_flags |= new_mach;

	      /* Do we need to update the ISA field?  */
	      if (new_isa > old_isa)
		{
		  elf_elfheader (obfd)->e_flags &= ~EF_MIPS_ARCH;
		  elf_elfheader (obfd)->e_flags
		    |= new_flags & EF_MIPS_ARCH;
		}
	    }
	}
      else
	{
	  (*_bfd_error_handler)
	    (_("%s: ISA mismatch (%d) with previous modules (%d)"),
	     bfd_archive_filename (ibfd),
	     elf_mips_mach (new_flags),
	     elf_mips_mach (old_flags));
	  ok = false;
	}

      new_flags &= ~(EF_MIPS_ARCH | EF_MIPS_MACH);
      old_flags &= ~(EF_MIPS_ARCH | EF_MIPS_MACH);
    }

  /* Compare ABI's.  The 64-bit ABI does not use EF_MIPS_ABI.  But, it
     does set EI_CLASS differently from any 32-bit ABI.  */
  if ((new_flags & EF_MIPS_ABI) != (old_flags & EF_MIPS_ABI)
      || (elf_elfheader (ibfd)->e_ident[EI_CLASS]
	  != elf_elfheader (obfd)->e_ident[EI_CLASS]))
    {
      /* Only error if both are set (to different values).  */
      if (((new_flags & EF_MIPS_ABI) && (old_flags & EF_MIPS_ABI))
	  || (elf_elfheader (ibfd)->e_ident[EI_CLASS]
	      != elf_elfheader (obfd)->e_ident[EI_CLASS]))
	{
	  (*_bfd_error_handler)
	    (_("%s: ABI mismatch: linking %s module with previous %s modules"),
	     bfd_archive_filename (ibfd),
	     elf_mips_abi_name (ibfd),
	     elf_mips_abi_name (obfd));
	  ok = false;
	}
      new_flags &= ~EF_MIPS_ABI;
      old_flags &= ~EF_MIPS_ABI;
    }

  /* Warn about any other mismatches */
  if (new_flags != old_flags)
    {
      (*_bfd_error_handler)
	(_("%s: uses different e_flags (0x%lx) fields than previous modules (0x%lx)"),
	 bfd_archive_filename (ibfd), (unsigned long) new_flags,
	 (unsigned long) old_flags);
      ok = false;
    }

  if (! ok)
    {
      bfd_set_error (bfd_error_bad_value);
      return false;
    }

  return true;
}

boolean
_bfd_mips_elf_print_private_bfd_data (abfd, ptr)
     bfd *abfd;
     PTR ptr;
{
  FILE *file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  /* Print normal ELF private data.  */
  _bfd_elf_print_private_bfd_data (abfd, ptr);

  /* xgettext:c-format */
  fprintf (file, _("private flags = %lx:"), elf_elfheader (abfd)->e_flags);

  if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI) == E_MIPS_ABI_O32)
    fprintf (file, _(" [abi=O32]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI) == E_MIPS_ABI_O64)
    fprintf (file, _(" [abi=O64]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI) == E_MIPS_ABI_EABI32)
    fprintf (file, _(" [abi=EABI32]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI) == E_MIPS_ABI_EABI64)
    fprintf (file, _(" [abi=EABI64]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ABI))
    fprintf (file, _(" [abi unknown]"));
  else if (ABI_N32_P (abfd))
    fprintf (file, _(" [abi=N32]"));
  else if (ABI_64_P (abfd))
    fprintf (file, _(" [abi=64]"));
  else
    fprintf (file, _(" [no abi set]"));

  if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_1)
    fprintf (file, _(" [mips1]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_2)
    fprintf (file, _(" [mips2]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_3)
    fprintf (file, _(" [mips3]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_4)
    fprintf (file, _(" [mips4]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_5)
    fprintf (file, _(" [mips5]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_32)
    fprintf (file, _(" [mips32]"));
  else if ((elf_elfheader (abfd)->e_flags & EF_MIPS_ARCH) == E_MIPS_ARCH_64)
    fprintf (file, _(" [mips64]"));
  else
    fprintf (file, _(" [unknown ISA]"));

  if (elf_elfheader (abfd)->e_flags & EF_MIPS_32BITMODE)
    fprintf (file, _(" [32bitmode]"));
  else
    fprintf (file, _(" [not 32bitmode]"));

  fputc ('\n', file);

  return true;
}

/* Handle a MIPS specific section when reading an object file.  This
   is called when elfcode.h finds a section with an unknown type.
   This routine supports both the 32-bit and 64-bit ELF ABI.

   FIXME: We need to handle the SHF_MIPS_GPREL flag, but I'm not sure
   how to.  */

boolean
_bfd_mips_elf_section_from_shdr (abfd, hdr, name)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
     char *name;
{
  flagword flags = 0;

  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  Fortunately, the ABI gives
     suggested names for all the MIPS specific sections, so we will
     probably get away with this.  */
  switch (hdr->sh_type)
    {
    case SHT_MIPS_LIBLIST:
      if (strcmp (name, ".liblist") != 0)
	return false;
      break;
    case SHT_MIPS_MSYM:
      if (strcmp (name, MIPS_ELF_MSYM_SECTION_NAME (abfd)) != 0)
	return false;
      break;
    case SHT_MIPS_CONFLICT:
      if (strcmp (name, ".conflict") != 0)
	return false;
      break;
    case SHT_MIPS_GPTAB:
      if (strncmp (name, ".gptab.", sizeof ".gptab." - 1) != 0)
	return false;
      break;
    case SHT_MIPS_UCODE:
      if (strcmp (name, ".ucode") != 0)
	return false;
      break;
    case SHT_MIPS_DEBUG:
      if (strcmp (name, ".mdebug") != 0)
	return false;
      flags = SEC_DEBUGGING;
      break;
    case SHT_MIPS_REGINFO:
      if (strcmp (name, ".reginfo") != 0
	  || hdr->sh_size != sizeof (Elf32_External_RegInfo))
	return false;
      flags = (SEC_LINK_ONCE | SEC_LINK_DUPLICATES_SAME_SIZE);
      break;
    case SHT_MIPS_IFACE:
      if (strcmp (name, ".MIPS.interfaces") != 0)
	return false;
      break;
    case SHT_MIPS_CONTENT:
      if (strncmp (name, ".MIPS.content", sizeof ".MIPS.content" - 1) != 0)
	return false;
      break;
    case SHT_MIPS_OPTIONS:
      if (strcmp (name, MIPS_ELF_OPTIONS_SECTION_NAME (abfd)) != 0)
	return false;
      break;
    case SHT_MIPS_DWARF:
      if (strncmp (name, ".debug_", sizeof ".debug_" - 1) != 0)
	return false;
      break;
    case SHT_MIPS_SYMBOL_LIB:
      if (strcmp (name, ".MIPS.symlib") != 0)
	return false;
      break;
    case SHT_MIPS_EVENTS:
      if (strncmp (name, ".MIPS.events", sizeof ".MIPS.events" - 1) != 0
	  && strncmp (name, ".MIPS.post_rel",
		      sizeof ".MIPS.post_rel" - 1) != 0)
	return false;
      break;
    default:
      return false;
    }

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name))
    return false;

  if (flags)
    {
      if (! bfd_set_section_flags (abfd, hdr->bfd_section,
				   (bfd_get_section_flags (abfd,
							   hdr->bfd_section)
				    | flags)))
	return false;
    }

  /* FIXME: We should record sh_info for a .gptab section.  */

  /* For a .reginfo section, set the gp value in the tdata information
     from the contents of this section.  We need the gp value while
     processing relocs, so we just get it now.  The .reginfo section
     is not used in the 64-bit MIPS ELF ABI.  */
  if (hdr->sh_type == SHT_MIPS_REGINFO)
    {
      Elf32_External_RegInfo ext;
      Elf32_RegInfo s;

      if (! bfd_get_section_contents (abfd, hdr->bfd_section, (PTR) &ext,
				      (file_ptr) 0,
				      (bfd_size_type) sizeof ext))
	return false;
      bfd_mips_elf32_swap_reginfo_in (abfd, &ext, &s);
      elf_gp (abfd) = s.ri_gp_value;
    }

  /* For a SHT_MIPS_OPTIONS section, look for a ODK_REGINFO entry, and
     set the gp value based on what we find.  We may see both
     SHT_MIPS_REGINFO and SHT_MIPS_OPTIONS/ODK_REGINFO; in that case,
     they should agree.  */
  if (hdr->sh_type == SHT_MIPS_OPTIONS)
    {
      bfd_byte *contents, *l, *lend;

      contents = (bfd_byte *) bfd_malloc (hdr->sh_size);
      if (contents == NULL)
	return false;
      if (! bfd_get_section_contents (abfd, hdr->bfd_section, contents,
				      (file_ptr) 0, hdr->sh_size))
	{
	  free (contents);
	  return false;
	}
      l = contents;
      lend = contents + hdr->sh_size;
      while (l + sizeof (Elf_External_Options) <= lend)
	{
	  Elf_Internal_Options intopt;

	  bfd_mips_elf_swap_options_in (abfd, (Elf_External_Options *) l,
					&intopt);
	  if (ABI_64_P (abfd) && intopt.kind == ODK_REGINFO)
	    {
	      Elf64_Internal_RegInfo intreg;

	      bfd_mips_elf64_swap_reginfo_in
		(abfd,
		 ((Elf64_External_RegInfo *)
		  (l + sizeof (Elf_External_Options))),
		 &intreg);
	      elf_gp (abfd) = intreg.ri_gp_value;
	    }
	  else if (intopt.kind == ODK_REGINFO)
	    {
	      Elf32_RegInfo intreg;

	      bfd_mips_elf32_swap_reginfo_in
		(abfd,
		 ((Elf32_External_RegInfo *)
		  (l + sizeof (Elf_External_Options))),
		 &intreg);
	      elf_gp (abfd) = intreg.ri_gp_value;
	    }
	  l += intopt.size;
	}
      free (contents);
    }

  return true;
}

/* Set the correct type for a MIPS ELF section.  We do this by the
   section name, which is a hack, but ought to work.  This routine is
   used by both the 32-bit and the 64-bit ABI.  */

boolean
_bfd_mips_elf_fake_sections (abfd, hdr, sec)
     bfd *abfd;
     Elf32_Internal_Shdr *hdr;
     asection *sec;
{
  register const char *name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".liblist") == 0)
    {
      hdr->sh_type = SHT_MIPS_LIBLIST;
      hdr->sh_info = sec->_raw_size / sizeof (Elf32_Lib);
      /* The sh_link field is set in final_write_processing.  */
    }
  else if (strcmp (name, ".conflict") == 0)
    hdr->sh_type = SHT_MIPS_CONFLICT;
  else if (strncmp (name, ".gptab.", sizeof ".gptab." - 1) == 0)
    {
      hdr->sh_type = SHT_MIPS_GPTAB;
      hdr->sh_entsize = sizeof (Elf32_External_gptab);
      /* The sh_info field is set in final_write_processing.  */
    }
  else if (strcmp (name, ".ucode") == 0)
    hdr->sh_type = SHT_MIPS_UCODE;
  else if (strcmp (name, ".mdebug") == 0)
    {
      hdr->sh_type = SHT_MIPS_DEBUG;
      /* In a shared object on Irix 5.3, the .mdebug section has an
         entsize of 0.  FIXME: Does this matter?  */
      if (SGI_COMPAT (abfd) && (abfd->flags & DYNAMIC) != 0)
	hdr->sh_entsize = 0;
      else
	hdr->sh_entsize = 1;
    }
  else if (strcmp (name, ".reginfo") == 0)
    {
      hdr->sh_type = SHT_MIPS_REGINFO;
      /* In a shared object on Irix 5.3, the .reginfo section has an
         entsize of 0x18.  FIXME: Does this matter?  */
      if (SGI_COMPAT (abfd))
	{
	  if ((abfd->flags & DYNAMIC) != 0)
	    hdr->sh_entsize = sizeof (Elf32_External_RegInfo);
	  else
	    hdr->sh_entsize = 1;
	}
      else
	hdr->sh_entsize = sizeof (Elf32_External_RegInfo);
    }
  else if (SGI_COMPAT (abfd)
	   && (strcmp (name, ".hash") == 0
	       || strcmp (name, ".dynamic") == 0
	       || strcmp (name, ".dynstr") == 0))
    {
      if (SGI_COMPAT (abfd))
	hdr->sh_entsize = 0;
#if 0
      /* This isn't how the Irix 6 linker behaves.  */
      hdr->sh_info = SIZEOF_MIPS_DYNSYM_SECNAMES;
#endif
    }
  else if (strcmp (name, ".got") == 0
	   || strcmp (name, MIPS_ELF_SRDATA_SECTION_NAME (abfd)) == 0
	   || strcmp (name, ".sdata") == 0
	   || strcmp (name, ".sbss") == 0
	   || strcmp (name, ".lit4") == 0
	   || strcmp (name, ".lit8") == 0)
    hdr->sh_flags |= SHF_MIPS_GPREL;
  else if (strcmp (name, ".MIPS.interfaces") == 0)
    {
      hdr->sh_type = SHT_MIPS_IFACE;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
    }
  else if (strncmp (name, ".MIPS.content", strlen (".MIPS.content")) == 0)
    {
      hdr->sh_type = SHT_MIPS_CONTENT;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
      /* The sh_info field is set in final_write_processing.  */
    }
  else if (strcmp (name, MIPS_ELF_OPTIONS_SECTION_NAME (abfd)) == 0)
    {
      hdr->sh_type = SHT_MIPS_OPTIONS;
      hdr->sh_entsize = 1;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
    }
  else if (strncmp (name, ".debug_", sizeof ".debug_" - 1) == 0)
    hdr->sh_type = SHT_MIPS_DWARF;
  else if (strcmp (name, ".MIPS.symlib") == 0)
    {
      hdr->sh_type = SHT_MIPS_SYMBOL_LIB;
      /* The sh_link and sh_info fields are set in
         final_write_processing.  */
    }
  else if (strncmp (name, ".MIPS.events", sizeof ".MIPS.events" - 1) == 0
	   || strncmp (name, ".MIPS.post_rel",
		       sizeof ".MIPS.post_rel" - 1) == 0)
    {
      hdr->sh_type = SHT_MIPS_EVENTS;
      hdr->sh_flags |= SHF_MIPS_NOSTRIP;
      /* The sh_link field is set in final_write_processing.  */
    }
  else if (strcmp (name, MIPS_ELF_MSYM_SECTION_NAME (abfd)) == 0)
    {
      hdr->sh_type = SHT_MIPS_MSYM;
      hdr->sh_flags |= SHF_ALLOC;
      hdr->sh_entsize = 8;
    }

  /* The generic elf_fake_sections will set up REL_HDR using the
     default kind of relocations.  But, we may actually need both
     kinds of relocations, so we set up the second header here.

     This is not necessary for the O32 ABI since that only uses Elf32_Rel
     relocations (cf. System V ABI, MIPS RISC Processor Supplement,
     3rd Edition, p. 4-17).  It breaks the IRIX 5/6 32-bit ld, since one
     of the resulting empty .rela.<section> sections starts with
     sh_offset == object size, and ld doesn't allow that.  While the check
     is arguably bogus for empty or SHT_NOBITS sections, it can easily be
     avoided by not emitting those useless sections in the first place.  */
  if (IRIX_COMPAT (abfd) != ict_irix5 && (sec->flags & SEC_RELOC) != 0)
    {
      struct bfd_elf_section_data *esd;
      bfd_size_type amt = sizeof (Elf_Internal_Shdr);

      esd = elf_section_data (sec);
      BFD_ASSERT (esd->rel_hdr2 == NULL);
      esd->rel_hdr2 = (Elf_Internal_Shdr *) bfd_zalloc (abfd, amt);
      if (!esd->rel_hdr2)
	return false;
      _bfd_elf_init_reloc_shdr (abfd, esd->rel_hdr2, sec,
				!elf_section_data (sec)->use_rela_p);
    }

  return true;
}

/* Given a BFD section, try to locate the corresponding ELF section
   index.  This is used by both the 32-bit and the 64-bit ABI.
   Actually, it's not clear to me that the 64-bit ABI supports these,
   but for non-PIC objects we will certainly want support for at least
   the .scommon section.  */

boolean
_bfd_mips_elf_section_from_bfd_section (abfd, sec, retval)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *sec;
     int *retval;
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    {
      *retval = SHN_MIPS_SCOMMON;
      return true;
    }
  if (strcmp (bfd_get_section_name (abfd, sec), ".acommon") == 0)
    {
      *retval = SHN_MIPS_ACOMMON;
      return true;
    }
  return false;
}

/* When are writing out the .options or .MIPS.options section,
   remember the bytes we are writing out, so that we can install the
   GP value in the section_processing routine.  */

boolean
_bfd_mips_elf_set_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (strcmp (section->name, MIPS_ELF_OPTIONS_SECTION_NAME (abfd)) == 0)
    {
      bfd_byte *c;

      if (elf_section_data (section) == NULL)
	{
	  bfd_size_type amt = sizeof (struct bfd_elf_section_data);
	  section->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
	  if (elf_section_data (section) == NULL)
	    return false;
	}
      c = (bfd_byte *) elf_section_data (section)->tdata;
      if (c == NULL)
	{
	  bfd_size_type size;

	  if (section->_cooked_size != 0)
	    size = section->_cooked_size;
	  else
	    size = section->_raw_size;
	  c = (bfd_byte *) bfd_zalloc (abfd, size);
	  if (c == NULL)
	    return false;
	  elf_section_data (section)->tdata = (PTR) c;
	}

      memcpy (c + offset, location, (size_t) count);
    }

  return _bfd_elf_set_section_contents (abfd, section, location, offset,
					count);
}

/* Work over a section just before writing it out.  This routine is
   used by both the 32-bit and the 64-bit ABI.  FIXME: We recognize
   sections that need the SHF_MIPS_GPREL flag by name; there has to be
   a better way.  */

boolean
_bfd_mips_elf_section_processing (abfd, hdr)
     bfd *abfd;
     Elf_Internal_Shdr *hdr;
{
  if (hdr->sh_type == SHT_MIPS_REGINFO
      && hdr->sh_size > 0)
    {
      bfd_byte buf[4];

      BFD_ASSERT (hdr->sh_size == sizeof (Elf32_External_RegInfo));
      BFD_ASSERT (hdr->contents == NULL);

      if (bfd_seek (abfd,
		    hdr->sh_offset + sizeof (Elf32_External_RegInfo) - 4,
		    SEEK_SET) != 0)
	return false;
      H_PUT_32 (abfd, elf_gp (abfd), buf);
      if (bfd_bwrite (buf, (bfd_size_type) 4, abfd) != 4)
	return false;
    }

  if (hdr->sh_type == SHT_MIPS_OPTIONS
      && hdr->bfd_section != NULL
      && elf_section_data (hdr->bfd_section) != NULL
      && elf_section_data (hdr->bfd_section)->tdata != NULL)
    {
      bfd_byte *contents, *l, *lend;

      /* We stored the section contents in the elf_section_data tdata
	 field in the set_section_contents routine.  We save the
	 section contents so that we don't have to read them again.
	 At this point we know that elf_gp is set, so we can look
	 through the section contents to see if there is an
	 ODK_REGINFO structure.  */

      contents = (bfd_byte *) elf_section_data (hdr->bfd_section)->tdata;
      l = contents;
      lend = contents + hdr->sh_size;
      while (l + sizeof (Elf_External_Options) <= lend)
	{
	  Elf_Internal_Options intopt;

	  bfd_mips_elf_swap_options_in (abfd, (Elf_External_Options *) l,
					&intopt);
	  if (ABI_64_P (abfd) && intopt.kind == ODK_REGINFO)
	    {
	      bfd_byte buf[8];

	      if (bfd_seek (abfd,
			    (hdr->sh_offset
			     + (l - contents)
			     + sizeof (Elf_External_Options)
			     + (sizeof (Elf64_External_RegInfo) - 8)),
			     SEEK_SET) != 0)
		return false;
	      H_PUT_64 (abfd, elf_gp (abfd), buf);
	      if (bfd_bwrite (buf, (bfd_size_type) 8, abfd) != 8)
		return false;
	    }
	  else if (intopt.kind == ODK_REGINFO)
	    {
	      bfd_byte buf[4];

	      if (bfd_seek (abfd,
			    (hdr->sh_offset
			     + (l - contents)
			     + sizeof (Elf_External_Options)
			     + (sizeof (Elf32_External_RegInfo) - 4)),
			    SEEK_SET) != 0)
		return false;
	      H_PUT_32 (abfd, elf_gp (abfd), buf);
	      if (bfd_bwrite (buf, (bfd_size_type) 4, abfd) != 4)
		return false;
	    }
	  l += intopt.size;
	}
    }

  if (hdr->bfd_section != NULL)
    {
      const char *name = bfd_get_section_name (abfd, hdr->bfd_section);

      if (strcmp (name, ".sdata") == 0
	  || strcmp (name, ".lit8") == 0
	  || strcmp (name, ".lit4") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".sbss") == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_NOBITS;
	}
      else if (strcmp (name, MIPS_ELF_SRDATA_SECTION_NAME (abfd)) == 0)
	{
	  hdr->sh_flags |= SHF_ALLOC | SHF_MIPS_GPREL;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".compact_rel") == 0)
	{
	  hdr->sh_flags = 0;
	  hdr->sh_type = SHT_PROGBITS;
	}
      else if (strcmp (name, ".rtproc") == 0)
	{
	  if (hdr->sh_addralign != 0 && hdr->sh_entsize == 0)
	    {
	      unsigned int adjust;

	      adjust = hdr->sh_size % hdr->sh_addralign;
	      if (adjust != 0)
		hdr->sh_size += hdr->sh_addralign - adjust;
	    }
	}
    }

  return true;
}

/* MIPS ELF uses two common sections.  One is the usual one, and the
   other is for small objects.  All the small objects are kept
   together, and then referenced via the gp pointer, which yields
   faster assembler code.  This is what we use for the small common
   section.  This approach is copied from ecoff.c.  */
static asection mips_elf_scom_section;
static asymbol mips_elf_scom_symbol;
static asymbol *mips_elf_scom_symbol_ptr;

/* MIPS ELF also uses an acommon section, which represents an
   allocated common symbol which may be overridden by a
   definition in a shared library.  */
static asection mips_elf_acom_section;
static asymbol mips_elf_acom_symbol;
static asymbol *mips_elf_acom_symbol_ptr;

/* Handle the special MIPS section numbers that a symbol may use.
   This is used for both the 32-bit and the 64-bit ABI.  */

void
_bfd_mips_elf_symbol_processing (abfd, asym)
     bfd *abfd;
     asymbol *asym;
{
  elf_symbol_type *elfsym;

  elfsym = (elf_symbol_type *) asym;
  switch (elfsym->internal_elf_sym.st_shndx)
    {
    case SHN_MIPS_ACOMMON:
      /* This section is used in a dynamically linked executable file.
	 It is an allocated common section.  The dynamic linker can
	 either resolve these symbols to something in a shared
	 library, or it can just leave them here.  For our purposes,
	 we can consider these symbols to be in a new section.  */
      if (mips_elf_acom_section.name == NULL)
	{
	  /* Initialize the acommon section.  */
	  mips_elf_acom_section.name = ".acommon";
	  mips_elf_acom_section.flags = SEC_ALLOC;
	  mips_elf_acom_section.output_section = &mips_elf_acom_section;
	  mips_elf_acom_section.symbol = &mips_elf_acom_symbol;
	  mips_elf_acom_section.symbol_ptr_ptr = &mips_elf_acom_symbol_ptr;
	  mips_elf_acom_symbol.name = ".acommon";
	  mips_elf_acom_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_acom_symbol.section = &mips_elf_acom_section;
	  mips_elf_acom_symbol_ptr = &mips_elf_acom_symbol;
	}
      asym->section = &mips_elf_acom_section;
      break;

    case SHN_COMMON:
      /* Common symbols less than the GP size are automatically
	 treated as SHN_MIPS_SCOMMON symbols on IRIX5.  */
      if (asym->value > elf_gp_size (abfd)
	  || IRIX_COMPAT (abfd) == ict_irix6)
	break;
      /* Fall through.  */
    case SHN_MIPS_SCOMMON:
      if (mips_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  mips_elf_scom_section.name = ".scommon";
	  mips_elf_scom_section.flags = SEC_IS_COMMON;
	  mips_elf_scom_section.output_section = &mips_elf_scom_section;
	  mips_elf_scom_section.symbol = &mips_elf_scom_symbol;
	  mips_elf_scom_section.symbol_ptr_ptr = &mips_elf_scom_symbol_ptr;
	  mips_elf_scom_symbol.name = ".scommon";
	  mips_elf_scom_symbol.flags = BSF_SECTION_SYM;
	  mips_elf_scom_symbol.section = &mips_elf_scom_section;
	  mips_elf_scom_symbol_ptr = &mips_elf_scom_symbol;
	}
      asym->section = &mips_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_MIPS_SUNDEFINED:
      asym->section = bfd_und_section_ptr;
      break;

#if 0 /* for SGI_COMPAT */
    case SHN_MIPS_TEXT:
      asym->section = mips_elf_text_section_ptr;
      break;

    case SHN_MIPS_DATA:
      asym->section = mips_elf_data_section_ptr;
      break;
#endif
    }
}

/* When creating an Irix 5 executable, we need REGINFO and RTPROC
   segments.  */

int
_bfd_mips_elf_additional_program_headers (abfd)
     bfd *abfd;
{
  asection *s;
  int ret = 0;

  /* See if we need a PT_MIPS_REGINFO segment.  */
  s = bfd_get_section_by_name (abfd, ".reginfo");
  if (s && (s->flags & SEC_LOAD))
    ++ret;

  /* See if we need a PT_MIPS_OPTIONS segment.  */
  if (IRIX_COMPAT (abfd) == ict_irix6
      && bfd_get_section_by_name (abfd,
				  MIPS_ELF_OPTIONS_SECTION_NAME (abfd)))
    ++ret;

  /* See if we need a PT_MIPS_RTPROC segment.  */
  if (IRIX_COMPAT (abfd) == ict_irix5
      && bfd_get_section_by_name (abfd, ".dynamic")
      && bfd_get_section_by_name (abfd, ".mdebug"))
    ++ret;

  return ret;
}

/* Modify the segment map for an Irix 5 executable.  */

boolean
_bfd_mips_elf_modify_segment_map (abfd)
     bfd *abfd;
{
  asection *s;
  struct elf_segment_map *m, **pm;
  bfd_size_type amt;

  /* If there is a .reginfo section, we need a PT_MIPS_REGINFO
     segment.  */
  s = bfd_get_section_by_name (abfd, ".reginfo");
  if (s != NULL && (s->flags & SEC_LOAD) != 0)
    {
      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
	if (m->p_type == PT_MIPS_REGINFO)
	  break;
      if (m == NULL)
	{
	  amt = sizeof *m;
	  m = (struct elf_segment_map *) bfd_zalloc (abfd, amt);
	  if (m == NULL)
	    return false;

	  m->p_type = PT_MIPS_REGINFO;
	  m->count = 1;
	  m->sections[0] = s;

	  /* We want to put it after the PHDR and INTERP segments.  */
	  pm = &elf_tdata (abfd)->segment_map;
	  while (*pm != NULL
		 && ((*pm)->p_type == PT_PHDR
		     || (*pm)->p_type == PT_INTERP))
	    pm = &(*pm)->next;

	  m->next = *pm;
	  *pm = m;
	}
    }

  /* For IRIX 6, we don't have .mdebug sections, nor does anything but
     .dynamic end up in PT_DYNAMIC.  However, we do have to insert a
     PT_OPTIONS segement immediately following the program header
     table.  */
  if (IRIX_COMPAT (abfd) == ict_irix6)
    {
      for (s = abfd->sections; s; s = s->next)
	if (elf_section_data (s)->this_hdr.sh_type == SHT_MIPS_OPTIONS)
	  break;

      if (s)
	{
	  struct elf_segment_map *options_segment;

	  /* Usually, there's a program header table.  But, sometimes
	     there's not (like when running the `ld' testsuite).  So,
	     if there's no program header table, we just put the
	     options segement at the end.  */
	  for (pm = &elf_tdata (abfd)->segment_map;
	       *pm != NULL;
	       pm = &(*pm)->next)
	    if ((*pm)->p_type == PT_PHDR)
	      break;

	  amt = sizeof (struct elf_segment_map);
	  options_segment = bfd_zalloc (abfd, amt);
	  options_segment->next = *pm;
	  options_segment->p_type = PT_MIPS_OPTIONS;
	  options_segment->p_flags = PF_R;
	  options_segment->p_flags_valid = true;
	  options_segment->count = 1;
	  options_segment->sections[0] = s;
	  *pm = options_segment;
	}
    }
  else
    {
      if (IRIX_COMPAT (abfd) == ict_irix5)
	{
	  /* If there are .dynamic and .mdebug sections, we make a room
	     for the RTPROC header.  FIXME: Rewrite without section names.  */
	  if (bfd_get_section_by_name (abfd, ".interp") == NULL
	      && bfd_get_section_by_name (abfd, ".dynamic") != NULL
	      && bfd_get_section_by_name (abfd, ".mdebug") != NULL)
	    {
	      for (m = elf_tdata (abfd)->segment_map; m != NULL; m = m->next)
		if (m->p_type == PT_MIPS_RTPROC)
		  break;
	      if (m == NULL)
		{
		  amt = sizeof *m;
		  m = (struct elf_segment_map *) bfd_zalloc (abfd, amt);
		  if (m == NULL)
		    return false;

		  m->p_type = PT_MIPS_RTPROC;

		  s = bfd_get_section_by_name (abfd, ".rtproc");
		  if (s == NULL)
		    {
		      m->count = 0;
		      m->p_flags = 0;
		      m->p_flags_valid = 1;
		    }
		  else
		    {
		      m->count = 1;
		      m->sections[0] = s;
		    }

		  /* We want to put it after the DYNAMIC segment.  */
		  pm = &elf_tdata (abfd)->segment_map;
		  while (*pm != NULL && (*pm)->p_type != PT_DYNAMIC)
		    pm = &(*pm)->next;
		  if (*pm != NULL)
		    pm = &(*pm)->next;

		  m->next = *pm;
		  *pm = m;
		}
	    }
	}
      /* On Irix 5, the PT_DYNAMIC segment includes the .dynamic,
	 .dynstr, .dynsym, and .hash sections, and everything in
	 between.  */
      for (pm = &elf_tdata (abfd)->segment_map; *pm != NULL;
	   pm = &(*pm)->next)
	if ((*pm)->p_type == PT_DYNAMIC)
	  break;
      m = *pm;
      if (m != NULL && IRIX_COMPAT (abfd) == ict_none)
	{
	  /* For a normal mips executable the permissions for the PT_DYNAMIC
	     segment are read, write and execute. We do that here since
	     the code in elf.c sets only the read permission. This matters
	     sometimes for the dynamic linker.  */
	  if (bfd_get_section_by_name (abfd, ".dynamic") != NULL)
	    {
	      m->p_flags = PF_R | PF_W | PF_X;
	      m->p_flags_valid = 1;
	    }
	}
      if (m != NULL
	  && m->count == 1 && strcmp (m->sections[0]->name, ".dynamic") == 0)
	{
	  static const char *sec_names[] =
	  {
	    ".dynamic", ".dynstr", ".dynsym", ".hash"
	  };
	  bfd_vma low, high;
	  unsigned int i, c;
	  struct elf_segment_map *n;

	  low = 0xffffffff;
	  high = 0;
	  for (i = 0; i < sizeof sec_names / sizeof sec_names[0]; i++)
	    {
	      s = bfd_get_section_by_name (abfd, sec_names[i]);
	      if (s != NULL && (s->flags & SEC_LOAD) != 0)
		{
		  bfd_size_type sz;

		  if (low > s->vma)
		    low = s->vma;
		  sz = s->_cooked_size;
		  if (sz == 0)
		    sz = s->_raw_size;
		  if (high < s->vma + sz)
		    high = s->vma + sz;
		}
	    }

	  c = 0;
	  for (s = abfd->sections; s != NULL; s = s->next)
	    if ((s->flags & SEC_LOAD) != 0
		&& s->vma >= low
		&& ((s->vma
		     + (s->_cooked_size !=
			0 ? s->_cooked_size : s->_raw_size)) <= high))
	      ++c;

	  amt = sizeof *n + (bfd_size_type) (c - 1) * sizeof (asection *);
	  n = (struct elf_segment_map *) bfd_zalloc (abfd, amt);
	  if (n == NULL)
	    return false;
	  *n = *m;
	  n->count = c;

	  i = 0;
	  for (s = abfd->sections; s != NULL; s = s->next)
	    {
	      if ((s->flags & SEC_LOAD) != 0
		  && s->vma >= low
		  && ((s->vma
		       + (s->_cooked_size != 0 ?
			  s->_cooked_size : s->_raw_size)) <= high))
		{
		  n->sections[i] = s;
		  ++i;
		}
	    }

	  *pm = n;
	}
    }

  return true;
}

/* The structure of the runtime procedure descriptor created by the
   loader for use by the static exception system.  */

typedef struct runtime_pdr {
	bfd_vma	adr;		/* memory address of start of procedure */
	long	regmask;	/* save register mask */
	long	regoffset;	/* save register offset */
	long	fregmask;	/* save floating point register mask */
	long	fregoffset;	/* save floating point register offset */
	long	frameoffset;	/* frame size */
	short	framereg;	/* frame pointer register */
	short	pcreg;		/* offset or reg of return pc */
	long	irpss;		/* index into the runtime string table */
	long	reserved;
	struct exception_info *exception_info;/* pointer to exception array */
} RPDR, *pRPDR;
#define cbRPDR sizeof (RPDR)
#define rpdNil ((pRPDR) 0)

/* Swap RPDR (runtime procedure table entry) for output.  */

static void ecoff_swap_rpdr_out
  PARAMS ((bfd *, const RPDR *, struct rpdr_ext *));

static void
ecoff_swap_rpdr_out (abfd, in, ex)
     bfd *abfd;
     const RPDR *in;
     struct rpdr_ext *ex;
{
  /* ECOFF_PUT_OFF was defined in ecoffswap.h.  */
  ECOFF_PUT_OFF (abfd, in->adr, ex->p_adr);
  H_PUT_32 (abfd, in->regmask, ex->p_regmask);
  H_PUT_32 (abfd, in->regoffset, ex->p_regoffset);
  H_PUT_32 (abfd, in->fregmask, ex->p_fregmask);
  H_PUT_32 (abfd, in->fregoffset, ex->p_fregoffset);
  H_PUT_32 (abfd, in->frameoffset, ex->p_frameoffset);

  H_PUT_16 (abfd, in->framereg, ex->p_framereg);
  H_PUT_16 (abfd, in->pcreg, ex->p_pcreg);

  H_PUT_32 (abfd, in->irpss, ex->p_irpss);
#if 0 /* FIXME */
  ECOFF_PUT_OFF (abfd, in->exception_info, ex->p_exception_info);
#endif
}

/* Read ECOFF debugging information from a .mdebug section into a
   ecoff_debug_info structure.  */

boolean
_bfd_mips_elf_read_ecoff_info (abfd, section, debug)
     bfd *abfd;
     asection *section;
     struct ecoff_debug_info *debug;
{
  HDRR *symhdr;
  const struct ecoff_debug_swap *swap;
  char *ext_hdr = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  memset (debug, 0, sizeof (*debug));

  ext_hdr = (char *) bfd_malloc (swap->external_hdr_size);
  if (ext_hdr == NULL && swap->external_hdr_size != 0)
    goto error_return;

  if (bfd_get_section_contents (abfd, section, ext_hdr, (file_ptr) 0,
				swap->external_hdr_size)
      == false)
    goto error_return;

  symhdr = &debug->symbolic_header;
  (*swap->swap_hdr_in) (abfd, ext_hdr, symhdr);

  /* The symbolic header contains absolute file offsets and sizes to
     read.  */
#define READ(ptr, offset, count, size, type)				\
  if (symhdr->count == 0)						\
    debug->ptr = NULL;							\
  else									\
    {									\
      bfd_size_type amt = (bfd_size_type) size * symhdr->count;		\
      debug->ptr = (type) bfd_malloc (amt);				\
      if (debug->ptr == NULL)						\
	goto error_return;						\
      if (bfd_seek (abfd, (file_ptr) symhdr->offset, SEEK_SET) != 0	\
	  || bfd_bread (debug->ptr, amt, abfd) != amt)			\
	goto error_return;						\
    }

  READ (line, cbLineOffset, cbLine, sizeof (unsigned char), unsigned char *);
  READ (external_dnr, cbDnOffset, idnMax, swap->external_dnr_size, PTR);
  READ (external_pdr, cbPdOffset, ipdMax, swap->external_pdr_size, PTR);
  READ (external_sym, cbSymOffset, isymMax, swap->external_sym_size, PTR);
  READ (external_opt, cbOptOffset, ioptMax, swap->external_opt_size, PTR);
  READ (external_aux, cbAuxOffset, iauxMax, sizeof (union aux_ext),
	union aux_ext *);
  READ (ss, cbSsOffset, issMax, sizeof (char), char *);
  READ (ssext, cbSsExtOffset, issExtMax, sizeof (char), char *);
  READ (external_fdr, cbFdOffset, ifdMax, swap->external_fdr_size, PTR);
  READ (external_rfd, cbRfdOffset, crfd, swap->external_rfd_size, PTR);
  READ (external_ext, cbExtOffset, iextMax, swap->external_ext_size, PTR);
#undef READ

  debug->fdr = NULL;
  debug->adjust = NULL;

  return true;

 error_return:
  if (ext_hdr != NULL)
    free (ext_hdr);
  if (debug->line != NULL)
    free (debug->line);
  if (debug->external_dnr != NULL)
    free (debug->external_dnr);
  if (debug->external_pdr != NULL)
    free (debug->external_pdr);
  if (debug->external_sym != NULL)
    free (debug->external_sym);
  if (debug->external_opt != NULL)
    free (debug->external_opt);
  if (debug->external_aux != NULL)
    free (debug->external_aux);
  if (debug->ss != NULL)
    free (debug->ss);
  if (debug->ssext != NULL)
    free (debug->ssext);
  if (debug->external_fdr != NULL)
    free (debug->external_fdr);
  if (debug->external_rfd != NULL)
    free (debug->external_rfd);
  if (debug->external_ext != NULL)
    free (debug->external_ext);
  return false;
}

/* MIPS ELF local labels start with '$', not 'L'.  */

static boolean
mips_elf_is_local_label_name (abfd, name)
     bfd *abfd;
     const char *name;
{
  if (name[0] == '$')
    return true;

  /* On Irix 6, the labels go back to starting with '.', so we accept
     the generic ELF local label syntax as well.  */
  return _bfd_elf_is_local_label_name (abfd, name);
}

/* MIPS ELF uses a special find_nearest_line routine in order the
   handle the ECOFF debugging information.  */

struct mips_elf_find_line
{
  struct ecoff_debug_info d;
  struct ecoff_find_line i;
};

boolean
_bfd_mips_elf_find_nearest_line (abfd, section, symbols, offset, filename_ptr,
				 functionname_ptr, line_ptr)
     bfd *abfd;
     asection *section;
     asymbol **symbols;
     bfd_vma offset;
     const char **filename_ptr;
     const char **functionname_ptr;
     unsigned int *line_ptr;
{
  asection *msec;

  if (_bfd_dwarf1_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr))
    return true;

  if (_bfd_dwarf2_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr,
				     (unsigned) (ABI_64_P (abfd) ? 8 : 0),
				     &elf_tdata (abfd)->dwarf2_find_line_info))
    return true;

  msec = bfd_get_section_by_name (abfd, ".mdebug");
  if (msec != NULL)
    {
      flagword origflags;
      struct mips_elf_find_line *fi;
      const struct ecoff_debug_swap * const swap =
	get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

      /* If we are called during a link, mips_elf_final_link may have
	 cleared the SEC_HAS_CONTENTS field.  We force it back on here
	 if appropriate (which it normally will be).  */
      origflags = msec->flags;
      if (elf_section_data (msec)->this_hdr.sh_type != SHT_NOBITS)
	msec->flags |= SEC_HAS_CONTENTS;

      fi = elf_tdata (abfd)->find_line_info;
      if (fi == NULL)
	{
	  bfd_size_type external_fdr_size;
	  char *fraw_src;
	  char *fraw_end;
	  struct fdr *fdr_ptr;
	  bfd_size_type amt = sizeof (struct mips_elf_find_line);

	  fi = (struct mips_elf_find_line *) bfd_zalloc (abfd, amt);
	  if (fi == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  if (! _bfd_mips_elf_read_ecoff_info (abfd, msec, &fi->d))
	    {
	      msec->flags = origflags;
	      return false;
	    }

	  /* Swap in the FDR information.  */
	  amt = fi->d.symbolic_header.ifdMax * sizeof (struct fdr);
	  fi->d.fdr = (struct fdr *) bfd_alloc (abfd, amt);
	  if (fi->d.fdr == NULL)
	    {
	      msec->flags = origflags;
	      return false;
	    }
	  external_fdr_size = swap->external_fdr_size;
	  fdr_ptr = fi->d.fdr;
	  fraw_src = (char *) fi->d.external_fdr;
	  fraw_end = (fraw_src
		      + fi->d.symbolic_header.ifdMax * external_fdr_size);
	  for (; fraw_src < fraw_end; fraw_src += external_fdr_size, fdr_ptr++)
	    (*swap->swap_fdr_in) (abfd, (PTR) fraw_src, fdr_ptr);

	  elf_tdata (abfd)->find_line_info = fi;

	  /* Note that we don't bother to ever free this information.
             find_nearest_line is either called all the time, as in
             objdump -l, so the information should be saved, or it is
             rarely called, as in ld error messages, so the memory
             wasted is unimportant.  Still, it would probably be a
             good idea for free_cached_info to throw it away.  */
	}

      if (_bfd_ecoff_locate_line (abfd, section, offset, &fi->d, swap,
				  &fi->i, filename_ptr, functionname_ptr,
				  line_ptr))
	{
	  msec->flags = origflags;
	  return true;
	}

      msec->flags = origflags;
    }

  /* Fall back on the generic ELF find_nearest_line routine.  */

  return _bfd_elf_find_nearest_line (abfd, section, symbols, offset,
				     filename_ptr, functionname_ptr,
				     line_ptr);
}

  /* The mips16 compiler uses a couple of special sections to handle
     floating point arguments.

     Section names that look like .mips16.fn.FNNAME contain stubs that
     copy floating point arguments from the fp regs to the gp regs and
     then jump to FNNAME.  If any 32 bit function calls FNNAME, the
     call should be redirected to the stub instead.  If no 32 bit
     function calls FNNAME, the stub should be discarded.  We need to
     consider any reference to the function, not just a call, because
     if the address of the function is taken we will need the stub,
     since the address might be passed to a 32 bit function.

     Section names that look like .mips16.call.FNNAME contain stubs
     that copy floating point arguments from the gp regs to the fp
     regs and then jump to FNNAME.  If FNNAME is a 32 bit function,
     then any 16 bit function that calls FNNAME should be redirected
     to the stub instead.  If FNNAME is not a 32 bit function, the
     stub should be discarded.

     .mips16.call.fp.FNNAME sections are similar, but contain stubs
     which call FNNAME and then copy the return value from the fp regs
     to the gp regs.  These stubs store the return value in $18 while
     calling FNNAME; any function which might call one of these stubs
     must arrange to save $18 around the call.  (This case is not
     needed for 32 bit functions that call 16 bit functions, because
     16 bit functions always return floating point values in both
     $f0/$f1 and $2/$3.)

     Note that in all cases FNNAME might be defined statically.
     Therefore, FNNAME is not used literally.  Instead, the relocation
     information will indicate which symbol the section is for.

     We record any stubs that we find in the symbol table.  */

#define FN_STUB ".mips16.fn."
#define CALL_STUB ".mips16.call."
#define CALL_FP_STUB ".mips16.call.fp."

/* MIPS ELF linker hash table.  */

struct mips_elf_link_hash_table
{
  struct elf_link_hash_table root;
#if 0
  /* We no longer use this.  */
  /* String section indices for the dynamic section symbols.  */
  bfd_size_type dynsym_sec_strindex[SIZEOF_MIPS_DYNSYM_SECNAMES];
#endif
  /* The number of .rtproc entries.  */
  bfd_size_type procedure_count;
  /* The size of the .compact_rel section (if SGI_COMPAT).  */
  bfd_size_type compact_rel_size;
  /* This flag indicates that the value of DT_MIPS_RLD_MAP dynamic
     entry is set to the address of __rld_obj_head as in Irix 5.  */
  boolean use_rld_obj_head;
  /* This is the value of the __rld_map or __rld_obj_head symbol.  */
  bfd_vma rld_value;
  /* This is set if we see any mips16 stub sections.  */
  boolean mips16_stubs_seen;
};

/* Look up an entry in a MIPS ELF linker hash table.  */

#define mips_elf_link_hash_lookup(table, string, create, copy, follow)	\
  ((struct mips_elf_link_hash_entry *)					\
   elf_link_hash_lookup (&(table)->root, (string), (create),		\
			 (copy), (follow)))

/* Traverse a MIPS ELF linker hash table.  */

#define mips_elf_link_hash_traverse(table, func, info)			\
  (elf_link_hash_traverse						\
   (&(table)->root,							\
    (boolean (*) PARAMS ((struct elf_link_hash_entry *, PTR))) (func),	\
    (info)))

/* Get the MIPS ELF linker hash table from a link_info structure.  */

#define mips_elf_hash_table(p) \
  ((struct mips_elf_link_hash_table *) ((p)->hash))

static boolean mips_elf_output_extsym
  PARAMS ((struct mips_elf_link_hash_entry *, PTR));

/* Create an entry in a MIPS ELF linker hash table.  */

static struct bfd_hash_entry *
mips_elf_link_hash_newfunc (entry, table, string)
     struct bfd_hash_entry *entry;
     struct bfd_hash_table *table;
     const char *string;
{
  struct mips_elf_link_hash_entry *ret =
    (struct mips_elf_link_hash_entry *) entry;

  /* Allocate the structure if it has not already been allocated by a
     subclass.  */
  if (ret == (struct mips_elf_link_hash_entry *) NULL)
    ret = ((struct mips_elf_link_hash_entry *)
	   bfd_hash_allocate (table,
			      sizeof (struct mips_elf_link_hash_entry)));
  if (ret == (struct mips_elf_link_hash_entry *) NULL)
    return (struct bfd_hash_entry *) ret;

  /* Call the allocation method of the superclass.  */
  ret = ((struct mips_elf_link_hash_entry *)
	 _bfd_elf_link_hash_newfunc ((struct bfd_hash_entry *) ret,
				     table, string));
  if (ret != (struct mips_elf_link_hash_entry *) NULL)
    {
      /* Set local fields.  */
      memset (&ret->esym, 0, sizeof (EXTR));
      /* We use -2 as a marker to indicate that the information has
	 not been set.  -1 means there is no associated ifd.  */
      ret->esym.ifd = -2;
      ret->possibly_dynamic_relocs = 0;
      ret->readonly_reloc = false;
      ret->min_dyn_reloc_index = 0;
      ret->no_fn_stub = false;
      ret->fn_stub = NULL;
      ret->need_fn_stub = false;
      ret->call_stub = NULL;
      ret->call_fp_stub = NULL;
    }

  return (struct bfd_hash_entry *) ret;
}

static void
_bfd_mips_elf_hide_symbol (info, entry, force_local)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *entry;
     boolean force_local;
{
  bfd *dynobj;
  asection *got;
  struct mips_got_info *g;
  struct mips_elf_link_hash_entry *h;
  h = (struct mips_elf_link_hash_entry *) entry;
  dynobj = elf_hash_table (info)->dynobj;
  got = bfd_get_section_by_name (dynobj, ".got");
  g = (struct mips_got_info *) elf_section_data (got)->tdata;

  _bfd_elf_link_hash_hide_symbol (info, &h->root, force_local);

  /* FIXME: Do we allocate too much GOT space here?  */
  g->local_gotno++;
  got->_raw_size += MIPS_ELF_GOT_SIZE (dynobj);
}

/* Create a MIPS ELF linker hash table.  */

struct bfd_link_hash_table *
_bfd_mips_elf_link_hash_table_create (abfd)
     bfd *abfd;
{
  struct mips_elf_link_hash_table *ret;
  bfd_size_type amt = sizeof (struct mips_elf_link_hash_table);

  ret = (struct mips_elf_link_hash_table *) bfd_alloc (abfd, amt);
  if (ret == (struct mips_elf_link_hash_table *) NULL)
    return NULL;

  if (! _bfd_elf_link_hash_table_init (&ret->root, abfd,
				       mips_elf_link_hash_newfunc))
    {
      bfd_release (abfd, ret);
      return NULL;
    }

#if 0
  /* We no longer use this.  */
  for (i = 0; i < SIZEOF_MIPS_DYNSYM_SECNAMES; i++)
    ret->dynsym_sec_strindex[i] = (bfd_size_type) -1;
#endif
  ret->procedure_count = 0;
  ret->compact_rel_size = 0;
  ret->use_rld_obj_head = false;
  ret->rld_value = 0;
  ret->mips16_stubs_seen = false;

  return &ret->root.root;
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special MIPS section numbers here.  */

boolean
_bfd_mips_elf_add_symbol_hook (abfd, info, sym, namep, flagsp, secp, valp)
     bfd *abfd;
     struct bfd_link_info *info;
     const Elf_Internal_Sym *sym;
     const char **namep;
     flagword *flagsp ATTRIBUTE_UNUSED;
     asection **secp;
     bfd_vma *valp;
{
  if (SGI_COMPAT (abfd)
      && (abfd->flags & DYNAMIC) != 0
      && strcmp (*namep, "_rld_new_interface") == 0)
    {
      /* Skip Irix 5 rld entry name.  */
      *namep = NULL;
      return true;
    }

  switch (sym->st_shndx)
    {
    case SHN_COMMON:
      /* Common symbols less than the GP size are automatically
	 treated as SHN_MIPS_SCOMMON symbols.  */
      if (sym->st_size > elf_gp_size (abfd)
	  || IRIX_COMPAT (abfd) == ict_irix6)
	break;
      /* Fall through.  */
    case SHN_MIPS_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_MIPS_TEXT:
      /* This section is used in a shared object.  */
      if (elf_tdata (abfd)->elf_text_section == NULL)
	{
	  asymbol *elf_text_symbol;
	  asection *elf_text_section;
	  bfd_size_type amt = sizeof (asection);

	  elf_text_section = bfd_zalloc (abfd, amt);
	  if (elf_text_section == NULL)
	    return false;

	  amt = sizeof (asymbol);
	  elf_text_symbol = bfd_zalloc (abfd, amt);
	  if (elf_text_symbol == NULL)
	    return false;

	  /* Initialize the section.  */

	  elf_tdata (abfd)->elf_text_section = elf_text_section;
	  elf_tdata (abfd)->elf_text_symbol = elf_text_symbol;

	  elf_text_section->symbol = elf_text_symbol;
	  elf_text_section->symbol_ptr_ptr = &elf_tdata (abfd)->elf_text_symbol;

	  elf_text_section->name = ".text";
	  elf_text_section->flags = SEC_NO_FLAGS;
	  elf_text_section->output_section = NULL;
	  elf_text_section->owner = abfd;
	  elf_text_symbol->name = ".text";
	  elf_text_symbol->flags = BSF_SECTION_SYM | BSF_DYNAMIC;
	  elf_text_symbol->section = elf_text_section;
	}
      /* This code used to do *secp = bfd_und_section_ptr if
         info->shared.  I don't know why, and that doesn't make sense,
         so I took it out.  */
      *secp = elf_tdata (abfd)->elf_text_section;
      break;

    case SHN_MIPS_ACOMMON:
      /* Fall through. XXX Can we treat this as allocated data?  */
    case SHN_MIPS_DATA:
      /* This section is used in a shared object.  */
      if (elf_tdata (abfd)->elf_data_section == NULL)
	{
	  asymbol *elf_data_symbol;
	  asection *elf_data_section;
	  bfd_size_type amt = sizeof (asection);

	  elf_data_section = bfd_zalloc (abfd, amt);
	  if (elf_data_section == NULL)
	    return false;

	  amt = sizeof (asymbol);
	  elf_data_symbol = bfd_zalloc (abfd, amt);
	  if (elf_data_symbol == NULL)
	    return false;

	  /* Initialize the section.  */

	  elf_tdata (abfd)->elf_data_section = elf_data_section;
	  elf_tdata (abfd)->elf_data_symbol = elf_data_symbol;

	  elf_data_section->symbol = elf_data_symbol;
	  elf_data_section->symbol_ptr_ptr = &elf_tdata (abfd)->elf_data_symbol;

	  elf_data_section->name = ".data";
	  elf_data_section->flags = SEC_NO_FLAGS;
	  elf_data_section->output_section = NULL;
	  elf_data_section->owner = abfd;
	  elf_data_symbol->name = ".data";
	  elf_data_symbol->flags = BSF_SECTION_SYM | BSF_DYNAMIC;
	  elf_data_symbol->section = elf_data_section;
	}
      /* This code used to do *secp = bfd_und_section_ptr if
         info->shared.  I don't know why, and that doesn't make sense,
         so I took it out.  */
      *secp = elf_tdata (abfd)->elf_data_section;
      break;

    case SHN_MIPS_SUNDEFINED:
      *secp = bfd_und_section_ptr;
      break;
    }

  if (SGI_COMPAT (abfd)
      && ! info->shared
      && info->hash->creator == abfd->xvec
      && strcmp (*namep, "__rld_obj_head") == 0)
    {
      struct elf_link_hash_entry *h;

      /* Mark __rld_obj_head as dynamic.  */
      h = NULL;
      if (! (_bfd_generic_link_add_one_symbol
	     (info, abfd, *namep, BSF_GLOBAL, *secp,
	      (bfd_vma) *valp, (const char *) NULL, false,
	      get_elf_backend_data (abfd)->collect,
	      (struct bfd_link_hash_entry **) &h)))
	return false;
      h->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_OBJECT;

      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	return false;

      mips_elf_hash_table (info)->use_rld_obj_head = true;
    }

  /* If this is a mips16 text symbol, add 1 to the value to make it
     odd.  This will cause something like .word SYM to come up with
     the right value when it is loaded into the PC.  */
  if (sym->st_other == STO_MIPS16)
    ++*valp;

  return true;
}

/* Structure used to pass information to mips_elf_output_extsym.  */

struct extsym_info
{
  bfd *abfd;
  struct bfd_link_info *info;
  struct ecoff_debug_info *debug;
  const struct ecoff_debug_swap *swap;
  boolean failed;
};

/* This routine is used to write out ECOFF debugging external symbol
   information.  It is called via mips_elf_link_hash_traverse.  The
   ECOFF external symbol information must match the ELF external
   symbol information.  Unfortunately, at this point we don't know
   whether a symbol is required by reloc information, so the two
   tables may wind up being different.  We must sort out the external
   symbol information before we can set the final size of the .mdebug
   section, and we must set the size of the .mdebug section before we
   can relocate any sections, and we can't know which symbols are
   required by relocation until we relocate the sections.
   Fortunately, it is relatively unlikely that any symbol will be
   stripped but required by a reloc.  In particular, it can not happen
   when generating a final executable.  */

static boolean
mips_elf_output_extsym (h, data)
     struct mips_elf_link_hash_entry *h;
     PTR data;
{
  struct extsym_info *einfo = (struct extsym_info *) data;
  boolean strip;
  asection *sec, *output_section;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct mips_elf_link_hash_entry *) h->root.root.u.i.link;

  if (h->root.indx == -2)
    strip = false;
  else if (((h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_DYNAMIC) != 0
	    || (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_DYNAMIC) != 0)
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0
	   && (h->root.elf_link_hash_flags & ELF_LINK_HASH_REF_REGULAR) == 0)
    strip = true;
  else if (einfo->info->strip == strip_all
	   || (einfo->info->strip == strip_some
	       && bfd_hash_lookup (einfo->info->keep_hash,
				   h->root.root.root.string,
				   false, false) == NULL))
    strip = true;
  else
    strip = false;

  if (strip)
    return true;

  if (h->esym.ifd == -2)
    {
      h->esym.jmptbl = 0;
      h->esym.cobol_main = 0;
      h->esym.weakext = 0;
      h->esym.reserved = 0;
      h->esym.ifd = ifdNil;
      h->esym.asym.value = 0;
      h->esym.asym.st = stGlobal;

      if (h->root.root.type == bfd_link_hash_undefined
	  || h->root.root.type == bfd_link_hash_undefweak)
	{
	  const char *name;

	  /* Use undefined class.  Also, set class and type for some
             special symbols.  */
	  name = h->root.root.root.string;
	  if (strcmp (name, mips_elf_dynsym_rtproc_names[0]) == 0
	      || strcmp (name, mips_elf_dynsym_rtproc_names[1]) == 0)
	    {
	      h->esym.asym.sc = scData;
	      h->esym.asym.st = stLabel;
	      h->esym.asym.value = 0;
	    }
	  else if (strcmp (name, mips_elf_dynsym_rtproc_names[2]) == 0)
	    {
	      h->esym.asym.sc = scAbs;
	      h->esym.asym.st = stLabel;
	      h->esym.asym.value =
		mips_elf_hash_table (einfo->info)->procedure_count;
	    }
	  else if (strcmp (name, "_gp_disp") == 0)
	    {
	      h->esym.asym.sc = scAbs;
	      h->esym.asym.st = stLabel;
	      h->esym.asym.value = elf_gp (einfo->abfd);
	    }
	  else
	    h->esym.asym.sc = scUndefined;
	}
      else if (h->root.root.type != bfd_link_hash_defined
	  && h->root.root.type != bfd_link_hash_defweak)
	h->esym.asym.sc = scAbs;
      else
	{
	  const char *name;

	  sec = h->root.root.u.def.section;
	  output_section = sec->output_section;

	  /* When making a shared library and symbol h is the one from
	     the another shared library, OUTPUT_SECTION may be null.  */
	  if (output_section == NULL)
	    h->esym.asym.sc = scUndefined;
	  else
	    {
	      name = bfd_section_name (output_section->owner, output_section);

	      if (strcmp (name, ".text") == 0)
		h->esym.asym.sc = scText;
	      else if (strcmp (name, ".data") == 0)
		h->esym.asym.sc = scData;
	      else if (strcmp (name, ".sdata") == 0)
		h->esym.asym.sc = scSData;
	      else if (strcmp (name, ".rodata") == 0
		       || strcmp (name, ".rdata") == 0)
		h->esym.asym.sc = scRData;
	      else if (strcmp (name, ".bss") == 0)
		h->esym.asym.sc = scBss;
	      else if (strcmp (name, ".sbss") == 0)
		h->esym.asym.sc = scSBss;
	      else if (strcmp (name, ".init") == 0)
		h->esym.asym.sc = scInit;
	      else if (strcmp (name, ".fini") == 0)
		h->esym.asym.sc = scFini;
	      else
		h->esym.asym.sc = scAbs;
	    }
	}

      h->esym.asym.reserved = 0;
      h->esym.asym.index = indexNil;
    }

  if (h->root.root.type == bfd_link_hash_common)
    h->esym.asym.value = h->root.root.u.c.size;
  else if (h->root.root.type == bfd_link_hash_defined
	   || h->root.root.type == bfd_link_hash_defweak)
    {
      if (h->esym.asym.sc == scCommon)
	h->esym.asym.sc = scBss;
      else if (h->esym.asym.sc == scSCommon)
	h->esym.asym.sc = scSBss;

      sec = h->root.root.u.def.section;
      output_section = sec->output_section;
      if (output_section != NULL)
	h->esym.asym.value = (h->root.root.u.def.value
			      + sec->output_offset
			      + output_section->vma);
      else
	h->esym.asym.value = 0;
    }
  else if ((h->root.elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      struct mips_elf_link_hash_entry *hd = h;
      boolean no_fn_stub = h->no_fn_stub;

      while (hd->root.root.type == bfd_link_hash_indirect)
	{
	  hd = (struct mips_elf_link_hash_entry *)h->root.root.u.i.link;
	  no_fn_stub = no_fn_stub || hd->no_fn_stub;
	}

      if (!no_fn_stub)
	{
	  /* Set type and value for a symbol with a function stub.  */
	  h->esym.asym.st = stProc;
	  sec = hd->root.root.u.def.section;
	  if (sec == NULL)
	    h->esym.asym.value = 0;
	  else
	    {
	      output_section = sec->output_section;
	      if (output_section != NULL)
		h->esym.asym.value = (hd->root.plt.offset
				      + sec->output_offset
				      + output_section->vma);
	      else
		h->esym.asym.value = 0;
	    }
#if 0 /* FIXME?  */
	  h->esym.ifd = 0;
#endif
	}
    }

  if (! bfd_ecoff_debug_one_external (einfo->abfd, einfo->debug, einfo->swap,
				      h->root.root.root.string,
				      &h->esym))
    {
      einfo->failed = true;
      return false;
    }

  return true;
}

/* Create a runtime procedure table from the .mdebug section.  */

static boolean
mips_elf_create_procedure_table (handle, abfd, info, s, debug)
     PTR handle;
     bfd *abfd;
     struct bfd_link_info *info;
     asection *s;
     struct ecoff_debug_info *debug;
{
  const struct ecoff_debug_swap *swap;
  HDRR *hdr = &debug->symbolic_header;
  RPDR *rpdr, *rp;
  struct rpdr_ext *erp;
  PTR rtproc;
  struct pdr_ext *epdr;
  struct sym_ext *esym;
  char *ss, **sv;
  char *str;
  bfd_size_type size;
  bfd_size_type count;
  unsigned long sindex;
  unsigned long i;
  PDR pdr;
  SYMR sym;
  const char *no_name_func = _("static procedure (no name)");

  epdr = NULL;
  rpdr = NULL;
  esym = NULL;
  ss = NULL;
  sv = NULL;

  swap = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;

  sindex = strlen (no_name_func) + 1;
  count = hdr->ipdMax;
  if (count > 0)
    {
      size = swap->external_pdr_size;

      epdr = (struct pdr_ext *) bfd_malloc (size * count);
      if (epdr == NULL)
	goto error_return;

      if (! _bfd_ecoff_get_accumulated_pdr (handle, (PTR) epdr))
	goto error_return;

      size = sizeof (RPDR);
      rp = rpdr = (RPDR *) bfd_malloc (size * count);
      if (rpdr == NULL)
	goto error_return;

      size = sizeof (char *);
      sv = (char **) bfd_malloc (size * count);
      if (sv == NULL)
	goto error_return;

      count = hdr->isymMax;
      size = swap->external_sym_size;
      esym = (struct sym_ext *) bfd_malloc (size * count);
      if (esym == NULL)
	goto error_return;

      if (! _bfd_ecoff_get_accumulated_sym (handle, (PTR) esym))
	goto error_return;

      count = hdr->issMax;
      ss = (char *) bfd_malloc (count);
      if (ss == NULL)
	goto error_return;
      if (! _bfd_ecoff_get_accumulated_ss (handle, (PTR) ss))
	goto error_return;

      count = hdr->ipdMax;
      for (i = 0; i < (unsigned long) count; i++, rp++)
	{
	  (*swap->swap_pdr_in) (abfd, (PTR) (epdr + i), &pdr);
	  (*swap->swap_sym_in) (abfd, (PTR) &esym[pdr.isym], &sym);
	  rp->adr = sym.value;
	  rp->regmask = pdr.regmask;
	  rp->regoffset = pdr.regoffset;
	  rp->fregmask = pdr.fregmask;
	  rp->fregoffset = pdr.fregoffset;
	  rp->frameoffset = pdr.frameoffset;
	  rp->framereg = pdr.framereg;
	  rp->pcreg = pdr.pcreg;
	  rp->irpss = sindex;
	  sv[i] = ss + sym.iss;
	  sindex += strlen (sv[i]) + 1;
	}
    }

  size = sizeof (struct rpdr_ext) * (count + 2) + sindex;
  size = BFD_ALIGN (size, 16);
  rtproc = (PTR) bfd_alloc (abfd, size);
  if (rtproc == NULL)
    {
      mips_elf_hash_table (info)->procedure_count = 0;
      goto error_return;
    }

  mips_elf_hash_table (info)->procedure_count = count + 2;

  erp = (struct rpdr_ext *) rtproc;
  memset (erp, 0, sizeof (struct rpdr_ext));
  erp++;
  str = (char *) rtproc + sizeof (struct rpdr_ext) * (count + 2);
  strcpy (str, no_name_func);
  str += strlen (no_name_func) + 1;
  for (i = 0; i < count; i++)
    {
      ecoff_swap_rpdr_out (abfd, rpdr + i, erp + i);
      strcpy (str, sv[i]);
      str += strlen (sv[i]) + 1;
    }
  ECOFF_PUT_OFF (abfd, -1, (erp + count)->p_adr);

  /* Set the size and contents of .rtproc section.  */
  s->_raw_size = size;
  s->contents = (bfd_byte *) rtproc;

  /* Skip this section later on (I don't think this currently
     matters, but someday it might).  */
  s->link_order_head = (struct bfd_link_order *) NULL;

  if (epdr != NULL)
    free (epdr);
  if (rpdr != NULL)
    free (rpdr);
  if (esym != NULL)
    free (esym);
  if (ss != NULL)
    free (ss);
  if (sv != NULL)
    free (sv);

  return true;

 error_return:
  if (epdr != NULL)
    free (epdr);
  if (rpdr != NULL)
    free (rpdr);
  if (esym != NULL)
    free (esym);
  if (ss != NULL)
    free (ss);
  if (sv != NULL)
    free (sv);
  return false;
}

/* A comparison routine used to sort .gptab entries.  */

static int
gptab_compare (p1, p2)
     const PTR p1;
     const PTR p2;
{
  const Elf32_gptab *a1 = (const Elf32_gptab *) p1;
  const Elf32_gptab *a2 = (const Elf32_gptab *) p2;

  return a1->gt_entry.gt_g_value - a2->gt_entry.gt_g_value;
}

/* We need to use a special link routine to handle the .reginfo and
   the .mdebug sections.  We need to merge all instances of these
   sections together, not write them all out sequentially.  */

boolean
_bfd_mips_elf_final_link (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  asection **secpp;
  asection *o;
  struct bfd_link_order *p;
  asection *reginfo_sec, *mdebug_sec, *gptab_data_sec, *gptab_bss_sec;
  asection *rtproc_sec;
  Elf32_RegInfo reginfo;
  struct ecoff_debug_info debug;
  const struct ecoff_debug_swap *swap
    = get_elf_backend_data (abfd)->elf_backend_ecoff_debug_swap;
  HDRR *symhdr = &debug.symbolic_header;
  PTR mdebug_handle = NULL;
  asection *s;
  EXTR esym;
  unsigned int i;
  bfd_size_type amt;

  static const char * const secname[] =
  {
    ".text", ".init", ".fini", ".data",
    ".rodata", ".sdata", ".sbss", ".bss"
  };
  static const int sc[] =
  {
    scText, scInit, scFini, scData,
    scRData, scSData, scSBss, scBss
  };

  /* If all the things we linked together were PIC, but we're
     producing an executable (rather than a shared object), then the
     resulting file is CPIC (i.e., it calls PIC code.)  */
  if (!info->shared
      && !info->relocateable
      && elf_elfheader (abfd)->e_flags & EF_MIPS_PIC)
    {
      elf_elfheader (abfd)->e_flags &= ~EF_MIPS_PIC;
      elf_elfheader (abfd)->e_flags |= EF_MIPS_CPIC;
    }

  /* We'd carefully arranged the dynamic symbol indices, and then the
     generic size_dynamic_sections renumbered them out from under us.
     Rather than trying somehow to prevent the renumbering, just do
     the sort again.  */
  if (elf_hash_table (info)->dynamic_sections_created)
    {
      bfd *dynobj;
      asection *got;
      struct mips_got_info *g;

      /* When we resort, we must tell mips_elf_sort_hash_table what
	 the lowest index it may use is.  That's the number of section
	 symbols we're going to add.  The generic ELF linker only
	 adds these symbols when building a shared object.  Note that
	 we count the sections after (possibly) removing the .options
	 section above.  */
      if (!mips_elf_sort_hash_table (info, (info->shared
					    ? bfd_count_sections (abfd) + 1
					    : 1)))
	return false;

      /* Make sure we didn't grow the global .got region.  */
      dynobj = elf_hash_table (info)->dynobj;
      got = bfd_get_section_by_name (dynobj, ".got");
      g = (struct mips_got_info *) elf_section_data (got)->tdata;

      if (g->global_gotsym != NULL)
	BFD_ASSERT ((elf_hash_table (info)->dynsymcount
		     - g->global_gotsym->dynindx)
		    <= g->global_gotno);
    }

  /* On IRIX5, we omit the .options section.  On IRIX6, however, we
     include it, even though we don't process it quite right.  (Some
     entries are supposed to be merged.)  Empirically, we seem to be
     better off including it then not.  */
  if (IRIX_COMPAT (abfd) == ict_irix5 || IRIX_COMPAT (abfd) == ict_none)
    for (secpp = &abfd->sections; *secpp != NULL; secpp = &(*secpp)->next)
      {
	if (strcmp ((*secpp)->name, MIPS_ELF_OPTIONS_SECTION_NAME (abfd)) == 0)
	  {
	    for (p = (*secpp)->link_order_head; p != NULL; p = p->next)
	      if (p->type == bfd_indirect_link_order)
		p->u.indirect.section->flags &= ~SEC_HAS_CONTENTS;
	    (*secpp)->link_order_head = NULL;
	    bfd_section_list_remove (abfd, secpp);
	    --abfd->section_count;

	    break;
	  }
      }

  /* Get a value for the GP register.  */
  if (elf_gp (abfd) == 0)
    {
      struct bfd_link_hash_entry *h;

      h = bfd_link_hash_lookup (info->hash, "_gp", false, false, true);
      if (h != (struct bfd_link_hash_entry *) NULL
	  && h->type == bfd_link_hash_defined)
	elf_gp (abfd) = (h->u.def.value
			 + h->u.def.section->output_section->vma
			 + h->u.def.section->output_offset);
      else if (info->relocateable)
	{
	  bfd_vma lo;

	  /* Find the GP-relative section with the lowest offset.  */
	  lo = (bfd_vma) -1;
	  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
	    if (o->vma < lo
		&& (elf_section_data (o)->this_hdr.sh_flags & SHF_MIPS_GPREL))
	      lo = o->vma;

	  /* And calculate GP relative to that.  */
	  elf_gp (abfd) = lo + ELF_MIPS_GP_OFFSET (abfd);
	}
      else
	{
	  /* If the relocate_section function needs to do a reloc
	     involving the GP value, it should make a reloc_dangerous
	     callback to warn that GP is not defined.  */
	}
    }

  /* Go through the sections and collect the .reginfo and .mdebug
     information.  */
  reginfo_sec = NULL;
  mdebug_sec = NULL;
  gptab_data_sec = NULL;
  gptab_bss_sec = NULL;
  for (o = abfd->sections; o != (asection *) NULL; o = o->next)
    {
      if (strcmp (o->name, ".reginfo") == 0)
	{
	  memset (&reginfo, 0, sizeof reginfo);

	  /* We have found the .reginfo section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      Elf32_External_RegInfo ext;
	      Elf32_RegInfo sub;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* The linker emulation code has probably clobbered the
                 size to be zero bytes.  */
	      if (input_section->_raw_size == 0)
		input_section->_raw_size = sizeof (Elf32_External_RegInfo);

	      if (! bfd_get_section_contents (input_bfd, input_section,
					      (PTR) &ext,
					      (file_ptr) 0,
					      (bfd_size_type) sizeof ext))
		return false;

	      bfd_mips_elf32_swap_reginfo_in (input_bfd, &ext, &sub);

	      reginfo.ri_gprmask |= sub.ri_gprmask;
	      reginfo.ri_cprmask[0] |= sub.ri_cprmask[0];
	      reginfo.ri_cprmask[1] |= sub.ri_cprmask[1];
	      reginfo.ri_cprmask[2] |= sub.ri_cprmask[2];
	      reginfo.ri_cprmask[3] |= sub.ri_cprmask[3];

	      /* ri_gp_value is set by the function
		 mips_elf32_section_processing when the section is
		 finally written out.  */

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &= ~SEC_HAS_CONTENTS;
	    }

	  /* Size has been set in mips_elf_always_size_sections  */
	  BFD_ASSERT(o->_raw_size == sizeof (Elf32_External_RegInfo));

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  reginfo_sec = o;
	}

      if (strcmp (o->name, ".mdebug") == 0)
	{
	  struct extsym_info einfo;
	  bfd_vma last;

	  /* We have found the .mdebug section in the output file.
	     Look through all the link_orders comprising it and merge
	     the information together.  */
	  symhdr->magic = swap->sym_magic;
	  /* FIXME: What should the version stamp be?  */
	  symhdr->vstamp = 0;
	  symhdr->ilineMax = 0;
	  symhdr->cbLine = 0;
	  symhdr->idnMax = 0;
	  symhdr->ipdMax = 0;
	  symhdr->isymMax = 0;
	  symhdr->ioptMax = 0;
	  symhdr->iauxMax = 0;
	  symhdr->issMax = 0;
	  symhdr->issExtMax = 0;
	  symhdr->ifdMax = 0;
	  symhdr->crfd = 0;
	  symhdr->iextMax = 0;

	  /* We accumulate the debugging information itself in the
	     debug_info structure.  */
	  debug.line = NULL;
	  debug.external_dnr = NULL;
	  debug.external_pdr = NULL;
	  debug.external_sym = NULL;
	  debug.external_opt = NULL;
	  debug.external_aux = NULL;
	  debug.ss = NULL;
	  debug.ssext = debug.ssext_end = NULL;
	  debug.external_fdr = NULL;
	  debug.external_rfd = NULL;
	  debug.external_ext = debug.external_ext_end = NULL;

	  mdebug_handle = bfd_ecoff_debug_init (abfd, &debug, swap, info);
	  if (mdebug_handle == (PTR) NULL)
	    return false;

	  esym.jmptbl = 0;
	  esym.cobol_main = 0;
	  esym.weakext = 0;
	  esym.reserved = 0;
	  esym.ifd = ifdNil;
	  esym.asym.iss = issNil;
	  esym.asym.st = stLocal;
	  esym.asym.reserved = 0;
	  esym.asym.index = indexNil;
	  last = 0;
	  for (i = 0; i < sizeof (secname) / sizeof (secname[0]); i++)
	    {
	      esym.asym.sc = sc[i];
	      s = bfd_get_section_by_name (abfd, secname[i]);
	      if (s != NULL)
		{
		  esym.asym.value = s->vma;
		  last = s->vma + s->_raw_size;
		}
	      else
		esym.asym.value = last;
	      if (!bfd_ecoff_debug_one_external (abfd, &debug, swap,
						 secname[i], &esym))
		return false;
	    }

	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      const struct ecoff_debug_swap *input_swap;
	      struct ecoff_debug_info input_debug;
	      char *eraw_src;
	      char *eraw_end;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      if (bfd_get_flavour (input_bfd) != bfd_target_elf_flavour
		  || (get_elf_backend_data (input_bfd)
		      ->elf_backend_ecoff_debug_swap) == NULL)
		{
		  /* I don't know what a non MIPS ELF bfd would be
		     doing with a .mdebug section, but I don't really
		     want to deal with it.  */
		  continue;
		}

	      input_swap = (get_elf_backend_data (input_bfd)
			    ->elf_backend_ecoff_debug_swap);

	      BFD_ASSERT (p->size == input_section->_raw_size);

	      /* The ECOFF linking code expects that we have already
		 read in the debugging information and set up an
		 ecoff_debug_info structure, so we do that now.  */
	      if (! _bfd_mips_elf_read_ecoff_info (input_bfd, input_section,
						   &input_debug))
		return false;

	      if (! (bfd_ecoff_debug_accumulate
		     (mdebug_handle, abfd, &debug, swap, input_bfd,
		      &input_debug, input_swap, info)))
		return false;

	      /* Loop through the external symbols.  For each one with
		 interesting information, try to find the symbol in
		 the linker global hash table and save the information
		 for the output external symbols.  */
	      eraw_src = input_debug.external_ext;
	      eraw_end = (eraw_src
			  + (input_debug.symbolic_header.iextMax
			     * input_swap->external_ext_size));
	      for (;
		   eraw_src < eraw_end;
		   eraw_src += input_swap->external_ext_size)
		{
		  EXTR ext;
		  const char *name;
		  struct mips_elf_link_hash_entry *h;

		  (*input_swap->swap_ext_in) (input_bfd, (PTR) eraw_src, &ext);
		  if (ext.asym.sc == scNil
		      || ext.asym.sc == scUndefined
		      || ext.asym.sc == scSUndefined)
		    continue;

		  name = input_debug.ssext + ext.asym.iss;
		  h = mips_elf_link_hash_lookup (mips_elf_hash_table (info),
						 name, false, false, true);
		  if (h == NULL || h->esym.ifd != -2)
		    continue;

		  if (ext.ifd != -1)
		    {
		      BFD_ASSERT (ext.ifd
				  < input_debug.symbolic_header.ifdMax);
		      ext.ifd = input_debug.ifdmap[ext.ifd];
		    }

		  h->esym = ext;
		}

	      /* Free up the information we just read.  */
	      free (input_debug.line);
	      free (input_debug.external_dnr);
	      free (input_debug.external_pdr);
	      free (input_debug.external_sym);
	      free (input_debug.external_opt);
	      free (input_debug.external_aux);
	      free (input_debug.ss);
	      free (input_debug.ssext);
	      free (input_debug.external_fdr);
	      free (input_debug.external_rfd);
	      free (input_debug.external_ext);

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &= ~SEC_HAS_CONTENTS;
	    }

	  if (SGI_COMPAT (abfd) && info->shared)
	    {
	      /* Create .rtproc section.  */
	      rtproc_sec = bfd_get_section_by_name (abfd, ".rtproc");
	      if (rtproc_sec == NULL)
		{
		  flagword flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY
				    | SEC_LINKER_CREATED | SEC_READONLY);

		  rtproc_sec = bfd_make_section (abfd, ".rtproc");
		  if (rtproc_sec == NULL
		      || ! bfd_set_section_flags (abfd, rtproc_sec, flags)
		      || ! bfd_set_section_alignment (abfd, rtproc_sec, 4))
		    return false;
		}

	      if (! mips_elf_create_procedure_table (mdebug_handle, abfd,
						     info, rtproc_sec, &debug))
		return false;
	    }

	  /* Build the external symbol information.  */
	  einfo.abfd = abfd;
	  einfo.info = info;
	  einfo.debug = &debug;
	  einfo.swap = swap;
	  einfo.failed = false;
	  mips_elf_link_hash_traverse (mips_elf_hash_table (info),
				       mips_elf_output_extsym,
				       (PTR) &einfo);
	  if (einfo.failed)
	    return false;

	  /* Set the size of the .mdebug section.  */
	  o->_raw_size = bfd_ecoff_debug_size (abfd, &debug, swap);

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;

	  mdebug_sec = o;
	}

      if (strncmp (o->name, ".gptab.", sizeof ".gptab." - 1) == 0)
	{
	  const char *subname;
	  unsigned int c;
	  Elf32_gptab *tab;
	  Elf32_External_gptab *ext_tab;
	  unsigned int j;

	  /* The .gptab.sdata and .gptab.sbss sections hold
	     information describing how the small data area would
	     change depending upon the -G switch.  These sections
	     not used in executables files.  */
	  if (! info->relocateable)
	    {
	      for (p = o->link_order_head;
		   p != (struct bfd_link_order *) NULL;
		   p = p->next)
		{
		  asection *input_section;

		  if (p->type != bfd_indirect_link_order)
		    {
		      if (p->type == bfd_fill_link_order)
			continue;
		      abort ();
		    }

		  input_section = p->u.indirect.section;

		  /* Hack: reset the SEC_HAS_CONTENTS flag so that
		     elf_link_input_bfd ignores this section.  */
		  input_section->flags &= ~SEC_HAS_CONTENTS;
		}

	      /* Skip this section later on (I don't think this
		 currently matters, but someday it might).  */
	      o->link_order_head = (struct bfd_link_order *) NULL;

	      /* Really remove the section.  */
	      for (secpp = &abfd->sections;
		   *secpp != o;
		   secpp = &(*secpp)->next)
		;
	      bfd_section_list_remove (abfd, secpp);
	      --abfd->section_count;

	      continue;
	    }

	  /* There is one gptab for initialized data, and one for
	     uninitialized data.  */
	  if (strcmp (o->name, ".gptab.sdata") == 0)
	    gptab_data_sec = o;
	  else if (strcmp (o->name, ".gptab.sbss") == 0)
	    gptab_bss_sec = o;
	  else
	    {
	      (*_bfd_error_handler)
		(_("%s: illegal section name `%s'"),
		 bfd_get_filename (abfd), o->name);
	      bfd_set_error (bfd_error_nonrepresentable_section);
	      return false;
	    }

	  /* The linker script always combines .gptab.data and
	     .gptab.sdata into .gptab.sdata, and likewise for
	     .gptab.bss and .gptab.sbss.  It is possible that there is
	     no .sdata or .sbss section in the output file, in which
	     case we must change the name of the output section.  */
	  subname = o->name + sizeof ".gptab" - 1;
	  if (bfd_get_section_by_name (abfd, subname) == NULL)
	    {
	      if (o == gptab_data_sec)
		o->name = ".gptab.data";
	      else
		o->name = ".gptab.bss";
	      subname = o->name + sizeof ".gptab" - 1;
	      BFD_ASSERT (bfd_get_section_by_name (abfd, subname) != NULL);
	    }

	  /* Set up the first entry.  */
	  c = 1;
	  amt = c * sizeof (Elf32_gptab);
	  tab = (Elf32_gptab *) bfd_malloc (amt);
	  if (tab == NULL)
	    return false;
	  tab[0].gt_header.gt_current_g_value = elf_gp_size (abfd);
	  tab[0].gt_header.gt_unused = 0;

	  /* Combine the input sections.  */
	  for (p = o->link_order_head;
	       p != (struct bfd_link_order *) NULL;
	       p = p->next)
	    {
	      asection *input_section;
	      bfd *input_bfd;
	      bfd_size_type size;
	      unsigned long last;
	      bfd_size_type gpentry;

	      if (p->type != bfd_indirect_link_order)
		{
		  if (p->type == bfd_fill_link_order)
		    continue;
		  abort ();
		}

	      input_section = p->u.indirect.section;
	      input_bfd = input_section->owner;

	      /* Combine the gptab entries for this input section one
		 by one.  We know that the input gptab entries are
		 sorted by ascending -G value.  */
	      size = bfd_section_size (input_bfd, input_section);
	      last = 0;
	      for (gpentry = sizeof (Elf32_External_gptab);
		   gpentry < size;
		   gpentry += sizeof (Elf32_External_gptab))
		{
		  Elf32_External_gptab ext_gptab;
		  Elf32_gptab int_gptab;
		  unsigned long val;
		  unsigned long add;
		  boolean exact;
		  unsigned int look;

		  if (! (bfd_get_section_contents
			 (input_bfd, input_section, (PTR) &ext_gptab,
			  (file_ptr) gpentry,
			  (bfd_size_type) sizeof (Elf32_External_gptab))))
		    {
		      free (tab);
		      return false;
		    }

		  bfd_mips_elf32_swap_gptab_in (input_bfd, &ext_gptab,
						&int_gptab);
		  val = int_gptab.gt_entry.gt_g_value;
		  add = int_gptab.gt_entry.gt_bytes - last;

		  exact = false;
		  for (look = 1; look < c; look++)
		    {
		      if (tab[look].gt_entry.gt_g_value >= val)
			tab[look].gt_entry.gt_bytes += add;

		      if (tab[look].gt_entry.gt_g_value == val)
			exact = true;
		    }

		  if (! exact)
		    {
		      Elf32_gptab *new_tab;
		      unsigned int max;

		      /* We need a new table entry.  */
		      amt = (bfd_size_type) (c + 1) * sizeof (Elf32_gptab);
		      new_tab = (Elf32_gptab *) bfd_realloc ((PTR) tab, amt);
		      if (new_tab == NULL)
			{
			  free (tab);
			  return false;
			}
		      tab = new_tab;
		      tab[c].gt_entry.gt_g_value = val;
		      tab[c].gt_entry.gt_bytes = add;

		      /* Merge in the size for the next smallest -G
			 value, since that will be implied by this new
			 value.  */
		      max = 0;
		      for (look = 1; look < c; look++)
			{
			  if (tab[look].gt_entry.gt_g_value < val
			      && (max == 0
				  || (tab[look].gt_entry.gt_g_value
				      > tab[max].gt_entry.gt_g_value)))
			    max = look;
			}
		      if (max != 0)
			tab[c].gt_entry.gt_bytes +=
			  tab[max].gt_entry.gt_bytes;

		      ++c;
		    }

		  last = int_gptab.gt_entry.gt_bytes;
		}

	      /* Hack: reset the SEC_HAS_CONTENTS flag so that
		 elf_link_input_bfd ignores this section.  */
	      input_section->flags &= ~SEC_HAS_CONTENTS;
	    }

	  /* The table must be sorted by -G value.  */
	  if (c > 2)
	    qsort (tab + 1, c - 1, sizeof (tab[0]), gptab_compare);

	  /* Swap out the table.  */
	  amt = (bfd_size_type) c * sizeof (Elf32_External_gptab);
	  ext_tab = (Elf32_External_gptab *) bfd_alloc (abfd, amt);
	  if (ext_tab == NULL)
	    {
	      free (tab);
	      return false;
	    }

	  for (j = 0; j < c; j++)
	    bfd_mips_elf32_swap_gptab_out (abfd, tab + j, ext_tab + j);
	  free (tab);

	  o->_raw_size = c * sizeof (Elf32_External_gptab);
	  o->contents = (bfd_byte *) ext_tab;

	  /* Skip this section later on (I don't think this currently
	     matters, but someday it might).  */
	  o->link_order_head = (struct bfd_link_order *) NULL;
	}
    }

  /* Invoke the regular ELF backend linker to do all the work.  */
  if (ABI_64_P (abfd))
    {
#ifdef BFD64
      if (!bfd_elf64_bfd_final_link (abfd, info))
	return false;
#else
      abort ();
      return false;
#endif /* BFD64 */
    }
  else if (!bfd_elf32_bfd_final_link (abfd, info))
    return false;

  /* Now write out the computed sections.  */

  if (reginfo_sec != (asection *) NULL)
    {
      Elf32_External_RegInfo ext;

      bfd_mips_elf32_swap_reginfo_out (abfd, &reginfo, &ext);
      if (! bfd_set_section_contents (abfd, reginfo_sec, (PTR) &ext,
				      (file_ptr) 0, (bfd_size_type) sizeof ext))
	return false;
    }

  if (mdebug_sec != (asection *) NULL)
    {
      BFD_ASSERT (abfd->output_has_begun);
      if (! bfd_ecoff_write_accumulated_debug (mdebug_handle, abfd, &debug,
					       swap, info,
					       mdebug_sec->filepos))
	return false;

      bfd_ecoff_debug_free (mdebug_handle, abfd, &debug, swap, info);
    }

  if (gptab_data_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_data_sec,
				      gptab_data_sec->contents,
				      (file_ptr) 0,
				      gptab_data_sec->_raw_size))
	return false;
    }

  if (gptab_bss_sec != (asection *) NULL)
    {
      if (! bfd_set_section_contents (abfd, gptab_bss_sec,
				      gptab_bss_sec->contents,
				      (file_ptr) 0,
				      gptab_bss_sec->_raw_size))
	return false;
    }

  if (SGI_COMPAT (abfd))
    {
      rtproc_sec = bfd_get_section_by_name (abfd, ".rtproc");
      if (rtproc_sec != NULL)
	{
	  if (! bfd_set_section_contents (abfd, rtproc_sec,
					  rtproc_sec->contents,
					  (file_ptr) 0,
					  rtproc_sec->_raw_size))
	    return false;
	}
    }

  return true;
}

/* This function is called via qsort() to sort the dynamic relocation
   entries by increasing r_symndx value.  */

static int
sort_dynamic_relocs (arg1, arg2)
     const PTR arg1;
     const PTR arg2;
{
  const Elf32_External_Rel *ext_reloc1 = (const Elf32_External_Rel *) arg1;
  const Elf32_External_Rel *ext_reloc2 = (const Elf32_External_Rel *) arg2;

  Elf_Internal_Rel int_reloc1;
  Elf_Internal_Rel int_reloc2;

  bfd_elf32_swap_reloc_in (reldyn_sorting_bfd, ext_reloc1, &int_reloc1);
  bfd_elf32_swap_reloc_in (reldyn_sorting_bfd, ext_reloc2, &int_reloc2);

  return (ELF32_R_SYM (int_reloc1.r_info) - ELF32_R_SYM (int_reloc2.r_info));
}

/* Returns the GOT section for ABFD.  */

static asection *
mips_elf_got_section (abfd)
     bfd *abfd;
{
  return bfd_get_section_by_name (abfd, ".got");
}

/* Returns the GOT information associated with the link indicated by
   INFO.  If SGOTP is non-NULL, it is filled in with the GOT
   section.  */

static struct mips_got_info *
mips_elf_got_info (abfd, sgotp)
     bfd *abfd;
     asection **sgotp;
{
  asection *sgot;
  struct mips_got_info *g;

  sgot = mips_elf_got_section (abfd);
  BFD_ASSERT (sgot != NULL);
  BFD_ASSERT (elf_section_data (sgot) != NULL);
  g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
  BFD_ASSERT (g != NULL);

  if (sgotp)
    *sgotp = sgot;
  return g;
}

/* Return whether a relocation is against a local symbol.  */

static boolean
mips_elf_local_relocation_p (input_bfd, relocation, local_sections,
			     check_forced)
     bfd *input_bfd;
     const Elf_Internal_Rela *relocation;
     asection **local_sections;
     boolean check_forced;
{
  unsigned long r_symndx;
  Elf_Internal_Shdr *symtab_hdr;
  struct mips_elf_link_hash_entry *h;
  size_t extsymoff;

  r_symndx = ELF32_R_SYM (relocation->r_info);
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  extsymoff = (elf_bad_symtab (input_bfd)) ? 0 : symtab_hdr->sh_info;

  if (r_symndx < extsymoff)
    return true;
  if (elf_bad_symtab (input_bfd) && local_sections[r_symndx] != NULL)
    return true;

  if (check_forced)
    {
      /* Look up the hash table to check whether the symbol
 	 was forced local.  */
      h = (struct mips_elf_link_hash_entry *)
	elf_sym_hashes (input_bfd) [r_symndx - extsymoff];
      /* Find the real hash-table entry for this symbol.  */
      while (h->root.root.type == bfd_link_hash_indirect
 	     || h->root.root.type == bfd_link_hash_warning)
	h = (struct mips_elf_link_hash_entry *) h->root.root.u.i.link;
      if ((h->root.elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0)
	return true;
    }

  return false;
}

/* Sign-extend VALUE, which has the indicated number of BITS.  */

static bfd_vma
mips_elf_sign_extend (value, bits)
     bfd_vma value;
     int bits;
{
  if (value & ((bfd_vma) 1 << (bits - 1)))
    /* VALUE is negative.  */
    value |= ((bfd_vma) - 1) << bits;

  return value;
}

/* Return non-zero if the indicated VALUE has overflowed the maximum
   range expressable by a signed number with the indicated number of
   BITS.  */

static boolean
mips_elf_overflow_p (value, bits)
     bfd_vma value;
     int bits;
{
  bfd_signed_vma svalue = (bfd_signed_vma) value;

  if (svalue > (1 << (bits - 1)) - 1)
    /* The value is too big.  */
    return true;
  else if (svalue < -(1 << (bits - 1)))
    /* The value is too small.  */
    return true;

  /* All is well.  */
  return false;
}

/* Calculate the %high function.  */

static bfd_vma
mips_elf_high (value)
     bfd_vma value;
{
  return ((value + (bfd_vma) 0x8000) >> 16) & 0xffff;
}

/* Calculate the %higher function.  */

static bfd_vma
mips_elf_higher (value)
     bfd_vma value ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  return ((value + (bfd_vma) 0x80008000) >> 32) & 0xffff;
#else
  abort ();
  return (bfd_vma) -1;
#endif
}

/* Calculate the %highest function.  */

static bfd_vma
mips_elf_highest (value)
     bfd_vma value ATTRIBUTE_UNUSED;
{
#ifdef BFD64
  return ((value + (bfd_vma) 0x800080008000) >> 48) & 0xffff;
#else
  abort ();
  return (bfd_vma) -1;
#endif
}

/* Returns the GOT index for the global symbol indicated by H.  */

static bfd_vma
mips_elf_global_got_index (abfd, h)
     bfd *abfd;
     struct elf_link_hash_entry *h;
{
  bfd_vma index;
  asection *sgot;
  struct mips_got_info *g;

  g = mips_elf_got_info (abfd, &sgot);

  /* Once we determine the global GOT entry with the lowest dynamic
     symbol table index, we must put all dynamic symbols with greater
     indices into the GOT.  That makes it easy to calculate the GOT
     offset.  */
  BFD_ASSERT (h->dynindx >= g->global_gotsym->dynindx);
  index = ((h->dynindx - g->global_gotsym->dynindx + g->local_gotno)
	   * MIPS_ELF_GOT_SIZE (abfd));
  BFD_ASSERT (index < sgot->_raw_size);

  return index;
}

/* Returns the offset for the entry at the INDEXth position
   in the GOT.  */

static bfd_vma
mips_elf_got_offset_from_index (dynobj, output_bfd, index)
     bfd *dynobj;
     bfd *output_bfd;
     bfd_vma index;
{
  asection *sgot;
  bfd_vma gp;

  sgot = mips_elf_got_section (dynobj);
  gp = _bfd_get_gp_value (output_bfd);
  return (sgot->output_section->vma + sgot->output_offset + index -
	  gp);
}

/* If H is a symbol that needs a global GOT entry, but has a dynamic
   symbol table index lower than any we've seen to date, record it for
   posterity.  */

static boolean
mips_elf_record_global_got_symbol (h, info, g)
     struct elf_link_hash_entry *h;
     struct bfd_link_info *info;
     struct mips_got_info *g ATTRIBUTE_UNUSED;
{
  /* A global symbol in the GOT must also be in the dynamic symbol
     table.  */
  if (h->dynindx == -1
      && !bfd_elf32_link_record_dynamic_symbol (info, h))
    return false;

  /* If we've already marked this entry as needing GOT space, we don't
     need to do it again.  */
  if (h->got.offset != (bfd_vma) -1)
    return true;

  /* By setting this to a value other than -1, we are indicating that
     there needs to be a GOT entry for H.  Avoid using zero, as the
     generic ELF copy_indirect_symbol tests for <= 0.  */
  h->got.offset = 1;

  return true;
}

/* This structure is passed to mips_elf_sort_hash_table_f when sorting
   the dynamic symbols.  */

struct mips_elf_hash_sort_data
{
  /* The symbol in the global GOT with the lowest dynamic symbol table
     index.  */
  struct elf_link_hash_entry *low;
  /* The least dynamic symbol table index corresponding to a symbol
     with a GOT entry.  */
  long min_got_dynindx;
  /* The greatest dynamic symbol table index not corresponding to a
     symbol without a GOT entry.  */
  long max_non_got_dynindx;
};

/* If H needs a GOT entry, assign it the highest available dynamic
   index.  Otherwise, assign it the lowest available dynamic
   index.  */

static boolean
mips_elf_sort_hash_table_f (h, data)
     struct mips_elf_link_hash_entry *h;
     PTR data;
{
  struct mips_elf_hash_sort_data *hsd
    = (struct mips_elf_hash_sort_data *) data;

  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct mips_elf_link_hash_entry *) h->root.root.u.i.link;

  /* Symbols without dynamic symbol table entries aren't interesting
     at all.  */
  if (h->root.dynindx == -1)
    return true;

  if (h->root.got.offset != 1)
    h->root.dynindx = hsd->max_non_got_dynindx++;
  else
    {
      h->root.dynindx = --hsd->min_got_dynindx;
      hsd->low = (struct elf_link_hash_entry *) h;
    }

  return true;
}

/* Sort the dynamic symbol table so that symbols that need GOT entries
   appear towards the end.  This reduces the amount of GOT space
   required.  MAX_LOCAL is used to set the number of local symbols
   known to be in the dynamic symbol table.  During
   mips_elf_size_dynamic_sections, this value is 1.  Afterward, the
   section symbols are added and the count is higher.  */

static boolean
mips_elf_sort_hash_table (info, max_local)
     struct bfd_link_info *info;
     unsigned long max_local;
{
  struct mips_elf_hash_sort_data hsd;
  struct mips_got_info *g;
  bfd *dynobj;

  dynobj = elf_hash_table (info)->dynobj;

  hsd.low = NULL;
  hsd.min_got_dynindx = elf_hash_table (info)->dynsymcount;
  hsd.max_non_got_dynindx = max_local;
  mips_elf_link_hash_traverse (((struct mips_elf_link_hash_table *)
				elf_hash_table (info)),
			       mips_elf_sort_hash_table_f,
			       &hsd);

  /* There should have been enough room in the symbol table to
     accomodate both the GOT and non-GOT symbols.  */
  BFD_ASSERT (hsd.max_non_got_dynindx <= hsd.min_got_dynindx);

  /* Now we know which dynamic symbol has the lowest dynamic symbol
     table index in the GOT.  */
  g = mips_elf_got_info (dynobj, NULL);
  g->global_gotsym = hsd.low;

  return true;
}

/* Create a local GOT entry for VALUE.  Return the index of the entry,
   or -1 if it could not be created.  */

static bfd_vma
mips_elf_create_local_got_entry (abfd, g, sgot, value)
     bfd *abfd;
     struct mips_got_info *g;
     asection *sgot;
     bfd_vma value;
{
  if (g->assigned_gotno >= g->local_gotno)
    {
      /* We didn't allocate enough space in the GOT.  */
      (*_bfd_error_handler)
	(_("not enough GOT space for local GOT entries"));
      bfd_set_error (bfd_error_bad_value);
      return (bfd_vma) -1;
    }

  MIPS_ELF_PUT_WORD (abfd, value,
		     (sgot->contents
		      + MIPS_ELF_GOT_SIZE (abfd) * g->assigned_gotno));
  return MIPS_ELF_GOT_SIZE (abfd) * g->assigned_gotno++;
}

/* Returns the GOT offset at which the indicated address can be found.
   If there is not yet a GOT entry for this value, create one.  Returns
   -1 if no satisfactory GOT offset can be found.  */

static bfd_vma
mips_elf_local_got_index (abfd, info, value)
     bfd *abfd;
     struct bfd_link_info *info;
     bfd_vma value;
{
  asection *sgot;
  struct mips_got_info *g;
  bfd_byte *entry;

  g = mips_elf_got_info (elf_hash_table (info)->dynobj, &sgot);

  /* Look to see if we already have an appropriate entry.  */
  for (entry = (sgot->contents
		+ MIPS_ELF_GOT_SIZE (abfd) * MIPS_RESERVED_GOTNO);
       entry != sgot->contents + MIPS_ELF_GOT_SIZE (abfd) * g->assigned_gotno;
       entry += MIPS_ELF_GOT_SIZE (abfd))
    {
      bfd_vma address = MIPS_ELF_GET_WORD (abfd, entry);
      if (address == value)
	return entry - sgot->contents;
    }

  return mips_elf_create_local_got_entry (abfd, g, sgot, value);
}

/* Find a GOT entry that is within 32KB of the VALUE.  These entries
   are supposed to be placed at small offsets in the GOT, i.e.,
   within 32KB of GP.  Return the index into the GOT for this page,
   and store the offset from this entry to the desired address in
   OFFSETP, if it is non-NULL.  */

static bfd_vma
mips_elf_got_page (abfd, info, value, offsetp)
     bfd *abfd;
     struct bfd_link_info *info;
     bfd_vma value;
     bfd_vma *offsetp;
{
  asection *sgot;
  struct mips_got_info *g;
  bfd_byte *entry;
  bfd_byte *last_entry;
  bfd_vma index = 0;
  bfd_vma address;

  g = mips_elf_got_info (elf_hash_table (info)->dynobj, &sgot);

  /* Look to see if we aleady have an appropriate entry.  */
  last_entry = sgot->contents + MIPS_ELF_GOT_SIZE (abfd) * g->assigned_gotno;
  for (entry = (sgot->contents
		+ MIPS_ELF_GOT_SIZE (abfd) * MIPS_RESERVED_GOTNO);
       entry != last_entry;
       entry += MIPS_ELF_GOT_SIZE (abfd))
    {
      address = MIPS_ELF_GET_WORD (abfd, entry);

      if (!mips_elf_overflow_p (value - address, 16))
	{
	  /* This entry will serve as the page pointer.  We can add a
	     16-bit number to it to get the actual address.  */
	  index = entry - sgot->contents;
	  break;
	}
    }

  /* If we didn't have an appropriate entry, we create one now.  */
  if (entry == last_entry)
    index = mips_elf_create_local_got_entry (abfd, g, sgot, value);

  if (offsetp)
    {
      address = MIPS_ELF_GET_WORD (abfd, entry);
      *offsetp = value - address;
    }

  return index;
}

/* Find a GOT entry whose higher-order 16 bits are the same as those
   for value.  Return the index into the GOT for this entry.  */

static bfd_vma
mips_elf_got16_entry (abfd, info, value, external)
     bfd *abfd;
     struct bfd_link_info *info;
     bfd_vma value;
     boolean external;
{
  asection *sgot;
  struct mips_got_info *g;
  bfd_byte *entry;
  bfd_byte *last_entry;
  bfd_vma index = 0;
  bfd_vma address;

  if (! external)
    {
      /* Although the ABI says that it is "the high-order 16 bits" that we
	 want, it is really the %high value.  The complete value is
	 calculated with a `addiu' of a LO16 relocation, just as with a
	 HI16/LO16 pair.  */
      value = mips_elf_high (value) << 16;
    }

  g = mips_elf_got_info (elf_hash_table (info)->dynobj, &sgot);

  /* Look to see if we already have an appropriate entry.  */
  last_entry = sgot->contents + MIPS_ELF_GOT_SIZE (abfd) * g->assigned_gotno;
  for (entry = (sgot->contents
		+ MIPS_ELF_GOT_SIZE (abfd) * MIPS_RESERVED_GOTNO);
       entry != last_entry;
       entry += MIPS_ELF_GOT_SIZE (abfd))
    {
      address = MIPS_ELF_GET_WORD (abfd, entry);
      if (address == value)
	{
	  /* This entry has the right high-order 16 bits, and the low-order
	     16 bits are set to zero.  */
	  index = entry - sgot->contents;
	  break;
	}
    }

  /* If we didn't have an appropriate entry, we create one now.  */
  if (entry == last_entry)
    index = mips_elf_create_local_got_entry (abfd, g, sgot, value);

  return index;
}

/* Returns the first relocation of type r_type found, beginning with
   RELOCATION.  RELEND is one-past-the-end of the relocation table.  */

static const Elf_Internal_Rela *
mips_elf_next_relocation (r_type, relocation, relend)
     unsigned int r_type;
     const Elf_Internal_Rela *relocation;
     const Elf_Internal_Rela *relend;
{
  /* According to the MIPS ELF ABI, the R_MIPS_LO16 relocation must be
     immediately following.  However, for the IRIX6 ABI, the next
     relocation may be a composed relocation consisting of several
     relocations for the same address.  In that case, the R_MIPS_LO16
     relocation may occur as one of these.  We permit a similar
     extension in general, as that is useful for GCC.  */
  while (relocation < relend)
    {
      if (ELF32_R_TYPE (relocation->r_info) == r_type)
	return relocation;

      ++relocation;
    }

  /* We didn't find it.  */
  bfd_set_error (bfd_error_bad_value);
  return NULL;
}

/* Create a rel.dyn relocation for the dynamic linker to resolve.  REL
   is the original relocation, which is now being transformed into a
   dynamic relocation.  The ADDENDP is adjusted if necessary; the
   caller should store the result in place of the original addend.  */

static boolean
mips_elf_create_dynamic_relocation (output_bfd, info, rel, h, sec,
				    symbol, addendp, input_section)
     bfd *output_bfd;
     struct bfd_link_info *info;
     const Elf_Internal_Rela *rel;
     struct mips_elf_link_hash_entry *h;
     asection *sec;
     bfd_vma symbol;
     bfd_vma *addendp;
     asection *input_section;
{
  Elf_Internal_Rel outrel;
  boolean skip;
  asection *sreloc;
  bfd *dynobj;
  int r_type;

  r_type = ELF32_R_TYPE (rel->r_info);
  dynobj = elf_hash_table (info)->dynobj;
  sreloc
    = bfd_get_section_by_name (dynobj,
			       MIPS_ELF_REL_DYN_SECTION_NAME (output_bfd));
  BFD_ASSERT (sreloc != NULL);
  BFD_ASSERT (sreloc->contents != NULL);
  BFD_ASSERT (sreloc->reloc_count * MIPS_ELF_REL_SIZE (output_bfd)
	      < sreloc->_raw_size);

  skip = false;
  outrel.r_offset =
    _bfd_elf_section_offset (output_bfd, info, input_section, rel->r_offset);
  if (outrel.r_offset == (bfd_vma) -1)
    skip = true;
  /* FIXME: For -2 runtime relocation needs to be skipped, but
     properly resolved statically and installed.  */
  BFD_ASSERT (outrel.r_offset != (bfd_vma) -2);

  /* If we've decided to skip this relocation, just output an empty
     record.  Note that R_MIPS_NONE == 0, so that this call to memset
     is a way of setting R_TYPE to R_MIPS_NONE.  */
  if (skip)
    memset (&outrel, 0, sizeof (outrel));
  else
    {
      long indx;
      bfd_vma section_offset;

      /* We must now calculate the dynamic symbol table index to use
	 in the relocation.  */
      if (h != NULL
	  && (! info->symbolic || (h->root.elf_link_hash_flags
				   & ELF_LINK_HASH_DEF_REGULAR) == 0))
	{
	  indx = h->root.dynindx;
	  /* h->root.dynindx may be -1 if this symbol was marked to
	     become local.  */
	  if (indx == -1)
	    indx = 0;
	}
      else
	{
	  if (sec != NULL && bfd_is_abs_section (sec))
	    indx = 0;
	  else if (sec == NULL || sec->owner == NULL)
	    {
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  else
	    {
	      indx = elf_section_data (sec->output_section)->dynindx;
	      if (indx == 0)
		abort ();
	    }

	  /* Figure out how far the target of the relocation is from
	     the beginning of its section.  */
	  section_offset = symbol - sec->output_section->vma;
	  /* The relocation we're building is section-relative.
	     Therefore, the original addend must be adjusted by the
	     section offset.  */
	  *addendp += section_offset;
	  /* Now, the relocation is just against the section.  */
	  symbol = sec->output_section->vma;
	}

      /* If the relocation was previously an absolute relocation and
	 this symbol will not be referred to by the relocation, we must
	 adjust it by the value we give it in the dynamic symbol table.
	 Otherwise leave the job up to the dynamic linker.  */
      if (!indx && r_type != R_MIPS_REL32)
	*addendp += symbol;

      /* The relocation is always an REL32 relocation because we don't
	 know where the shared library will wind up at load-time.  */
      outrel.r_info = ELF32_R_INFO (indx, R_MIPS_REL32);

      /* Adjust the output offset of the relocation to reference the
	 correct location in the output file.  */
      outrel.r_offset += (input_section->output_section->vma
			  + input_section->output_offset);
    }

  /* Put the relocation back out.  We have to use the special
     relocation outputter in the 64-bit case since the 64-bit
     relocation format is non-standard.  */
  if (ABI_64_P (output_bfd))
    {
      (*get_elf_backend_data (output_bfd)->s->swap_reloc_out)
	(output_bfd, &outrel,
	 (sreloc->contents
	  + sreloc->reloc_count * sizeof (Elf64_Mips_External_Rel)));
    }
  else
    bfd_elf32_swap_reloc_out (output_bfd, &outrel,
			      (((Elf32_External_Rel *)
				sreloc->contents)
			       + sreloc->reloc_count));

  /* Record the index of the first relocation referencing H.  This
     information is later emitted in the .msym section.  */
  if (h != NULL
      && (h->min_dyn_reloc_index == 0
	  || sreloc->reloc_count < h->min_dyn_reloc_index))
    h->min_dyn_reloc_index = sreloc->reloc_count;

  /* We've now added another relocation.  */
  ++sreloc->reloc_count;

  /* Make sure the output section is writable.  The dynamic linker
     will be writing to it.  */
  elf_section_data (input_section->output_section)->this_hdr.sh_flags
    |= SHF_WRITE;

  /* On IRIX5, make an entry of compact relocation info.  */
  if (! skip && IRIX_COMPAT (output_bfd) == ict_irix5)
    {
      asection *scpt = bfd_get_section_by_name (dynobj, ".compact_rel");
      bfd_byte *cr;

      if (scpt)
	{
	  Elf32_crinfo cptrel;

	  mips_elf_set_cr_format (cptrel, CRF_MIPS_LONG);
	  cptrel.vaddr = (rel->r_offset
			  + input_section->output_section->vma
			  + input_section->output_offset);
	  if (r_type == R_MIPS_REL32)
	    mips_elf_set_cr_type (cptrel, CRT_MIPS_REL32);
	  else
	    mips_elf_set_cr_type (cptrel, CRT_MIPS_WORD);
	  mips_elf_set_cr_dist2to (cptrel, 0);
	  cptrel.konst = *addendp;

	  cr = (scpt->contents
		+ sizeof (Elf32_External_compact_rel));
	  bfd_elf32_swap_crinfo_out (output_bfd, &cptrel,
				     ((Elf32_External_crinfo *) cr
				      + scpt->reloc_count));
	  ++scpt->reloc_count;
	}
    }

  return true;
}

/* Calculate the value produced by the RELOCATION (which comes from
   the INPUT_BFD).  The ADDEND is the addend to use for this
   RELOCATION; RELOCATION->R_ADDEND is ignored.

   The result of the relocation calculation is stored in VALUEP.
   REQUIRE_JALXP indicates whether or not the opcode used with this
   relocation must be JALX.

   This function returns bfd_reloc_continue if the caller need take no
   further action regarding this relocation, bfd_reloc_notsupported if
   something goes dramatically wrong, bfd_reloc_overflow if an
   overflow occurs, and bfd_reloc_ok to indicate success.  */

static bfd_reloc_status_type
mips_elf_calculate_relocation (abfd,
			       input_bfd,
			       input_section,
			       info,
			       relocation,
			       addend,
			       howto,
			       local_syms,
			       local_sections,
			       valuep,
			       namep,
			       require_jalxp)
     bfd *abfd;
     bfd *input_bfd;
     asection *input_section;
     struct bfd_link_info *info;
     const Elf_Internal_Rela *relocation;
     bfd_vma addend;
     reloc_howto_type *howto;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
     bfd_vma *valuep;
     const char **namep;
     boolean *require_jalxp;
{
  /* The eventual value we will return.  */
  bfd_vma value;
  /* The address of the symbol against which the relocation is
     occurring.  */
  bfd_vma symbol = 0;
  /* The final GP value to be used for the relocatable, executable, or
     shared object file being produced.  */
  bfd_vma gp = (bfd_vma) - 1;
  /* The place (section offset or address) of the storage unit being
     relocated.  */
  bfd_vma p;
  /* The value of GP used to create the relocatable object.  */
  bfd_vma gp0 = (bfd_vma) - 1;
  /* The offset into the global offset table at which the address of
     the relocation entry symbol, adjusted by the addend, resides
     during execution.  */
  bfd_vma g = (bfd_vma) - 1;
  /* The section in which the symbol referenced by the relocation is
     located.  */
  asection *sec = NULL;
  struct mips_elf_link_hash_entry *h = NULL;
  /* True if the symbol referred to by this relocation is a local
     symbol.  */
  boolean local_p;
  /* True if the symbol referred to by this relocation is "_gp_disp".  */
  boolean gp_disp_p = false;
  Elf_Internal_Shdr *symtab_hdr;
  size_t extsymoff;
  unsigned long r_symndx;
  int r_type;
  /* True if overflow occurred during the calculation of the
     relocation value.  */
  boolean overflowed_p;
  /* True if this relocation refers to a MIPS16 function.  */
  boolean target_is_16_bit_code_p = false;

  /* Parse the relocation.  */
  r_symndx = ELF32_R_SYM (relocation->r_info);
  r_type = ELF32_R_TYPE (relocation->r_info);
  p = (input_section->output_section->vma
       + input_section->output_offset
       + relocation->r_offset);

  /* Assume that there will be no overflow.  */
  overflowed_p = false;

  /* Figure out whether or not the symbol is local, and get the offset
     used in the array of hash table entries.  */
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  local_p = mips_elf_local_relocation_p (input_bfd, relocation,
					 local_sections, false);
  if (! elf_bad_symtab (input_bfd))
    extsymoff = symtab_hdr->sh_info;
  else
    {
      /* The symbol table does not follow the rule that local symbols
	 must come before globals.  */
      extsymoff = 0;
    }

  /* Figure out the value of the symbol.  */
  if (local_p)
    {
      Elf_Internal_Sym *sym;

      sym = local_syms + r_symndx;
      sec = local_sections[r_symndx];

      symbol = sec->output_section->vma + sec->output_offset;
      if (ELF_ST_TYPE (sym->st_info) != STT_SECTION)
	symbol += sym->st_value;

      /* MIPS16 text labels should be treated as odd.  */
      if (sym->st_other == STO_MIPS16)
	++symbol;

      /* Record the name of this symbol, for our caller.  */
      *namep = bfd_elf_string_from_elf_section (input_bfd,
						symtab_hdr->sh_link,
						sym->st_name);
      if (*namep == '\0')
	*namep = bfd_section_name (input_bfd, sec);

      target_is_16_bit_code_p = (sym->st_other == STO_MIPS16);
    }
  else
    {
      /* For global symbols we look up the symbol in the hash-table.  */
      h = ((struct mips_elf_link_hash_entry *)
	   elf_sym_hashes (input_bfd) [r_symndx - extsymoff]);
      /* Find the real hash-table entry for this symbol.  */
      while (h->root.root.type == bfd_link_hash_indirect
	     || h->root.root.type == bfd_link_hash_warning)
	h = (struct mips_elf_link_hash_entry *) h->root.root.u.i.link;

      /* Record the name of this symbol, for our caller.  */
      *namep = h->root.root.root.string;

      /* See if this is the special _gp_disp symbol.  Note that such a
	 symbol must always be a global symbol.  */
      if (strcmp (h->root.root.root.string, "_gp_disp") == 0)
	{
	  /* Relocations against _gp_disp are permitted only with
	     R_MIPS_HI16 and R_MIPS_LO16 relocations.  */
	  if (r_type != R_MIPS_HI16 && r_type != R_MIPS_LO16)
	    return bfd_reloc_notsupported;

	  gp_disp_p = true;
	}
      /* If this symbol is defined, calculate its address.  Note that
	 _gp_disp is a magic symbol, always implicitly defined by the
	 linker, so it's inappropriate to check to see whether or not
	 its defined.  */
      else if ((h->root.root.type == bfd_link_hash_defined
		|| h->root.root.type == bfd_link_hash_defweak)
	       && h->root.root.u.def.section)
	{
	  sec = h->root.root.u.def.section;
	  if (sec->output_section)
	    symbol = (h->root.root.u.def.value
		      + sec->output_section->vma
		      + sec->output_offset);
	  else
	    symbol = h->root.root.u.def.value;
	}
      else if (h->root.root.type == bfd_link_hash_undefweak)
	/* We allow relocations against undefined weak symbols, giving
	   it the value zero, so that you can undefined weak functions
	   and check to see if they exist by looking at their
	   addresses.  */
	symbol = 0;
      else if (info->shared
	       && (!info->symbolic || info->allow_shlib_undefined)
	       && !info->no_undefined
	       && ELF_ST_VISIBILITY (h->root.other) == STV_DEFAULT)
	symbol = 0;
      else if (strcmp (h->root.root.root.string, "_DYNAMIC_LINK") == 0 ||
              strcmp (h->root.root.root.string, "_DYNAMIC_LINKING") == 0)
	{
	  /* If this is a dynamic link, we should have created a
	     _DYNAMIC_LINK symbol or _DYNAMIC_LINKING(for normal mips) symbol
	     in in mips_elf_create_dynamic_sections.
	     Otherwise, we should define the symbol with a value of 0.
	     FIXME: It should probably get into the symbol table
	     somehow as well.  */
	  BFD_ASSERT (! info->shared);
	  BFD_ASSERT (bfd_get_section_by_name (abfd, ".dynamic") == NULL);
	  symbol = 0;
	}
      else
	{
	  if (! ((*info->callbacks->undefined_symbol)
		 (info, h->root.root.root.string, input_bfd,
		  input_section, relocation->r_offset,
		  (!info->shared || info->no_undefined
		   || ELF_ST_VISIBILITY (h->root.other)))))
	    return bfd_reloc_undefined;
	  symbol = 0;
	}

      target_is_16_bit_code_p = (h->root.other == STO_MIPS16);
    }

  /* If this is a 32-bit call to a 16-bit function with a stub, we
     need to redirect the call to the stub, unless we're already *in*
     a stub.  */
  if (r_type != R_MIPS16_26 && !info->relocateable
      && ((h != NULL && h->fn_stub != NULL)
	  || (local_p && elf_tdata (input_bfd)->local_stubs != NULL
	      && elf_tdata (input_bfd)->local_stubs[r_symndx] != NULL))
      && !mips_elf_stub_section_p (input_bfd, input_section))
    {
      /* This is a 32-bit call to a 16-bit function.  We should
	 have already noticed that we were going to need the
	 stub.  */
      if (local_p)
	sec = elf_tdata (input_bfd)->local_stubs[r_symndx];
      else
	{
	  BFD_ASSERT (h->need_fn_stub);
	  sec = h->fn_stub;
	}

      symbol = sec->output_section->vma + sec->output_offset;
    }
  /* If this is a 16-bit call to a 32-bit function with a stub, we
     need to redirect the call to the stub.  */
  else if (r_type == R_MIPS16_26 && !info->relocateable
	   && h != NULL
	   && (h->call_stub != NULL || h->call_fp_stub != NULL)
	   && !target_is_16_bit_code_p)
    {
      /* If both call_stub and call_fp_stub are defined, we can figure
	 out which one to use by seeing which one appears in the input
	 file.  */
      if (h->call_stub != NULL && h->call_fp_stub != NULL)
	{
	  asection *o;

	  sec = NULL;
	  for (o = input_bfd->sections; o != NULL; o = o->next)
	    {
	      if (strncmp (bfd_get_section_name (input_bfd, o),
			   CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
		{
		  sec = h->call_fp_stub;
		  break;
		}
	    }
	  if (sec == NULL)
	    sec = h->call_stub;
	}
      else if (h->call_stub != NULL)
	sec = h->call_stub;
      else
	sec = h->call_fp_stub;

      BFD_ASSERT (sec->_raw_size > 0);
      symbol = sec->output_section->vma + sec->output_offset;
    }

  /* Calls from 16-bit code to 32-bit code and vice versa require the
     special jalx instruction.  */
  *require_jalxp = (!info->relocateable
                    && (((r_type == R_MIPS16_26) && !target_is_16_bit_code_p)
                        || ((r_type == R_MIPS_26) && target_is_16_bit_code_p)));

  local_p = mips_elf_local_relocation_p (input_bfd, relocation,
					 local_sections, true);

  /* If we haven't already determined the GOT offset, or the GP value,
     and we're going to need it, get it now.  */
  switch (r_type)
    {
    case R_MIPS_CALL16:
    case R_MIPS_GOT16:
    case R_MIPS_GOT_DISP:
    case R_MIPS_GOT_HI16:
    case R_MIPS_CALL_HI16:
    case R_MIPS_GOT_LO16:
    case R_MIPS_CALL_LO16:
      /* Find the index into the GOT where this value is located.  */
      if (!local_p)
	{
	  BFD_ASSERT (addend == 0);
	  g = mips_elf_global_got_index
	    (elf_hash_table (info)->dynobj,
	     (struct elf_link_hash_entry *) h);
	  if (! elf_hash_table(info)->dynamic_sections_created
	      || (info->shared
		  && (info->symbolic || h->root.dynindx == -1)
		  && (h->root.elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR)))
	    {
	      /* This is a static link or a -Bsymbolic link.  The
		 symbol is defined locally, or was forced to be local.
		 We must initialize this entry in the GOT.  */
	      asection *sgot = mips_elf_got_section(elf_hash_table
						    (info)->dynobj);
	      MIPS_ELF_PUT_WORD (elf_hash_table (info)->dynobj,
				 symbol + addend, sgot->contents + g);
	    }
	}
      else if (r_type == R_MIPS_GOT16 || r_type == R_MIPS_CALL16)
	/* There's no need to create a local GOT entry here; the
	   calculation for a local GOT16 entry does not involve G.  */
	break;
      else
	{
	  g = mips_elf_local_got_index (abfd, info, symbol + addend);
	  if (g == (bfd_vma) -1)
	    return bfd_reloc_outofrange;
	}

      /* Convert GOT indices to actual offsets.  */
      g = mips_elf_got_offset_from_index (elf_hash_table (info)->dynobj,
					  abfd, g);
      break;

    case R_MIPS_HI16:
    case R_MIPS_LO16:
    case R_MIPS16_GPREL:
    case R_MIPS_GPREL16:
    case R_MIPS_GPREL32:
    case R_MIPS_LITERAL:
      gp0 = _bfd_get_gp_value (input_bfd);
      gp = _bfd_get_gp_value (abfd);
      break;

    default:
      break;
    }

  /* Figure out what kind of relocation is being performed.  */
  switch (r_type)
    {
    case R_MIPS_NONE:
      return bfd_reloc_continue;

    case R_MIPS_16:
      value = symbol + mips_elf_sign_extend (addend, 16);
      overflowed_p = mips_elf_overflow_p (value, 16);
      break;

    case R_MIPS_32:
    case R_MIPS_REL32:
    case R_MIPS_64:
      if ((info->shared
	   || (elf_hash_table (info)->dynamic_sections_created
	       && h != NULL
	       && ((h->root.elf_link_hash_flags
		    & ELF_LINK_HASH_DEF_DYNAMIC) != 0)
	       && ((h->root.elf_link_hash_flags
		    & ELF_LINK_HASH_DEF_REGULAR) == 0)))
	  && r_symndx != 0
	  && (input_section->flags & SEC_ALLOC) != 0)
	{
	  /* If we're creating a shared library, or this relocation is
	     against a symbol in a shared library, then we can't know
	     where the symbol will end up.  So, we create a relocation
	     record in the output, and leave the job up to the dynamic
	     linker.  */
	  value = addend;
	  if (!mips_elf_create_dynamic_relocation (abfd,
						   info,
						   relocation,
						   h,
						   sec,
						   symbol,
						   &value,
						   input_section))
	    return bfd_reloc_undefined;
	}
      else
	{
	  if (r_type != R_MIPS_REL32)
	    value = symbol + addend;
	  else
	    value = addend;
	}
      value &= howto->dst_mask;
      break;

    case R_MIPS_PC32:
    case R_MIPS_PC64:
    case R_MIPS_GNU_REL_LO16:
      value = symbol + addend - p;
      value &= howto->dst_mask;
      break;

    case R_MIPS_GNU_REL16_S2:
      value = symbol + mips_elf_sign_extend (addend << 2, 18) - p;
      overflowed_p = mips_elf_overflow_p (value, 18);
      value = (value >> 2) & howto->dst_mask;
      break;

    case R_MIPS_GNU_REL_HI16:
      /* Instead of subtracting 'p' here, we should be subtracting the
	 equivalent value for the LO part of the reloc, since the value
	 here is relative to that address.  Because that's not easy to do,
	 we adjust 'addend' in _bfd_mips_elf_relocate_section().  See also
	 the comment there for more information.  */
      value = mips_elf_high (addend + symbol - p);
      value &= howto->dst_mask;
      break;

    case R_MIPS16_26:
      /* The calculation for R_MIPS16_26 is just the same as for an
	 R_MIPS_26.  It's only the storage of the relocated field into
	 the output file that's different.  That's handled in
	 mips_elf_perform_relocation.  So, we just fall through to the
	 R_MIPS_26 case here.  */
    case R_MIPS_26:
      if (local_p)
	value = (((addend << 2) | ((p + 4) & 0xf0000000)) + symbol) >> 2;
      else
	value = (mips_elf_sign_extend (addend << 2, 28) + symbol) >> 2;
      value &= howto->dst_mask;
      break;

    case R_MIPS_HI16:
      if (!gp_disp_p)
	{
	  value = mips_elf_high (addend + symbol);
	  value &= howto->dst_mask;
	}
      else
	{
	  value = mips_elf_high (addend + gp - p);
	  overflowed_p = mips_elf_overflow_p (value, 16);
	}
      break;

    case R_MIPS_LO16:
      if (!gp_disp_p)
	value = (symbol + addend) & howto->dst_mask;
      else
	{
	  value = addend + gp - p + 4;
	  /* The MIPS ABI requires checking the R_MIPS_LO16 relocation
	     for overflow.  But, on, say, Irix 5, relocations against
	     _gp_disp are normally generated from the .cpload
	     pseudo-op.  It generates code that normally looks like
	     this:

	       lui    $gp,%hi(_gp_disp)
	       addiu  $gp,$gp,%lo(_gp_disp)
	       addu   $gp,$gp,$t9

	     Here $t9 holds the address of the function being called,
	     as required by the MIPS ELF ABI.  The R_MIPS_LO16
	     relocation can easily overflow in this situation, but the
	     R_MIPS_HI16 relocation will handle the overflow.
	     Therefore, we consider this a bug in the MIPS ABI, and do
	     not check for overflow here.  */
	}
      break;

    case R_MIPS_LITERAL:
      /* Because we don't merge literal sections, we can handle this
	 just like R_MIPS_GPREL16.  In the long run, we should merge
	 shared literals, and then we will need to additional work
	 here.  */

      /* Fall through.  */

    case R_MIPS16_GPREL:
      /* The R_MIPS16_GPREL performs the same calculation as
	 R_MIPS_GPREL16, but stores the relocated bits in a different
	 order.  We don't need to do anything special here; the
	 differences are handled in mips_elf_perform_relocation.  */
    case R_MIPS_GPREL16:
      if (local_p)
	value = mips_elf_sign_extend (addend, 16) + symbol + gp0 - gp;
      else
	value = mips_elf_sign_extend (addend, 16) + symbol - gp;
      overflowed_p = mips_elf_overflow_p (value, 16);
      break;

    case R_MIPS_GOT16:
    case R_MIPS_CALL16:
      if (local_p)
	{
	  boolean forced;

	  /* The special case is when the symbol is forced to be local.  We
	     need the full address in the GOT since no R_MIPS_LO16 relocation
	     follows.  */
	  forced = ! mips_elf_local_relocation_p (input_bfd, relocation,
						  local_sections, false);
	  value = mips_elf_got16_entry (abfd, info, symbol + addend, forced);
	  if (value == (bfd_vma) -1)
	    return bfd_reloc_outofrange;
	  value
	    = mips_elf_got_offset_from_index (elf_hash_table (info)->dynobj,
					      abfd,
					      value);
	  overflowed_p = mips_elf_overflow_p (value, 16);
	  break;
	}

      /* Fall through.  */

    case R_MIPS_GOT_DISP:
      value = g;
      overflowed_p = mips_elf_overflow_p (value, 16);
      break;

    case R_MIPS_GPREL32:
      value = (addend + symbol + gp0 - gp) & howto->dst_mask;
      break;

    case R_MIPS_PC16:
      value = mips_elf_sign_extend (addend, 16) + symbol - p;
      overflowed_p = mips_elf_overflow_p (value, 16);
      value = (bfd_vma) ((bfd_signed_vma) value / 4);
      break;

    case R_MIPS_GOT_HI16:
    case R_MIPS_CALL_HI16:
      /* We're allowed to handle these two relocations identically.
	 The dynamic linker is allowed to handle the CALL relocations
	 differently by creating a lazy evaluation stub.  */
      value = g;
      value = mips_elf_high (value);
      value &= howto->dst_mask;
      break;

    case R_MIPS_GOT_LO16:
    case R_MIPS_CALL_LO16:
      value = g & howto->dst_mask;
      break;

    case R_MIPS_GOT_PAGE:
      value = mips_elf_got_page (abfd, info, symbol + addend, NULL);
      if (value == (bfd_vma) -1)
	return bfd_reloc_outofrange;
      value = mips_elf_got_offset_from_index (elf_hash_table (info)->dynobj,
					      abfd,
					      value);
      overflowed_p = mips_elf_overflow_p (value, 16);
      break;

    case R_MIPS_GOT_OFST:
      mips_elf_got_page (abfd, info, symbol + addend, &value);
      overflowed_p = mips_elf_overflow_p (value, 16);
      break;

    case R_MIPS_SUB:
      value = symbol - addend;
      value &= howto->dst_mask;
      break;

    case R_MIPS_HIGHER:
      value = mips_elf_higher (addend + symbol);
      value &= howto->dst_mask;
      break;

    case R_MIPS_HIGHEST:
      value = mips_elf_highest (addend + symbol);
      value &= howto->dst_mask;
      break;

    case R_MIPS_SCN_DISP:
      value = symbol + addend - sec->output_offset;
      value &= howto->dst_mask;
      break;

    case R_MIPS_PJUMP:
    case R_MIPS_JALR:
      /* Both of these may be ignored.  R_MIPS_JALR is an optimization
	 hint; we could improve performance by honoring that hint.  */
      return bfd_reloc_continue;

    case R_MIPS_GNU_VTINHERIT:
    case R_MIPS_GNU_VTENTRY:
      /* We don't do anything with these at present.  */
      return bfd_reloc_continue;

    default:
      /* An unrecognized relocation type.  */
      return bfd_reloc_notsupported;
    }

  /* Store the VALUE for our caller.  */
  *valuep = value;
  return overflowed_p ? bfd_reloc_overflow : bfd_reloc_ok;
}

/* Obtain the field relocated by RELOCATION.  */

static bfd_vma
mips_elf_obtain_contents (howto, relocation, input_bfd, contents)
     reloc_howto_type *howto;
     const Elf_Internal_Rela *relocation;
     bfd *input_bfd;
     bfd_byte *contents;
{
  bfd_vma x;
  bfd_byte *location = contents + relocation->r_offset;

  /* Obtain the bytes.  */
  x = bfd_get (((bfd_vma)(8 * bfd_get_reloc_size (howto))), input_bfd, location);

  if ((ELF32_R_TYPE (relocation->r_info) == R_MIPS16_26
       || ELF32_R_TYPE (relocation->r_info) == R_MIPS16_GPREL)
      && bfd_little_endian (input_bfd))
    /* The two 16-bit words will be reversed on a little-endian
       system.  See mips_elf_perform_relocation for more details.  */
    x = (((x & 0xffff) << 16) | ((x & 0xffff0000) >> 16));

  return x;
}

/* It has been determined that the result of the RELOCATION is the
   VALUE.  Use HOWTO to place VALUE into the output file at the
   appropriate position.  The SECTION is the section to which the
   relocation applies.  If REQUIRE_JALX is true, then the opcode used
   for the relocation must be either JAL or JALX, and it is
   unconditionally converted to JALX.

   Returns false if anything goes wrong.  */

static boolean
mips_elf_perform_relocation (info, howto, relocation, value,
			     input_bfd, input_section,
			     contents, require_jalx)
     struct bfd_link_info *info;
     reloc_howto_type *howto;
     const Elf_Internal_Rela *relocation;
     bfd_vma value;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     boolean require_jalx;
{
  bfd_vma x;
  bfd_byte *location;
  int r_type = ELF32_R_TYPE (relocation->r_info);

  /* Figure out where the relocation is occurring.  */
  location = contents + relocation->r_offset;

  /* Obtain the current value.  */
  x = mips_elf_obtain_contents (howto, relocation, input_bfd, contents);

  /* Clear the field we are setting.  */
  x &= ~howto->dst_mask;

  /* If this is the R_MIPS16_26 relocation, we must store the
     value in a funny way.  */
  if (r_type == R_MIPS16_26)
    {
      /* R_MIPS16_26 is used for the mips16 jal and jalx instructions.
	 Most mips16 instructions are 16 bits, but these instructions
	 are 32 bits.

	 The format of these instructions is:

	 +--------------+--------------------------------+
	 !     JALX     ! X!   Imm 20:16  !   Imm 25:21  !
	 +--------------+--------------------------------+
	 !	  	  Immediate  15:0		    !
	 +-----------------------------------------------+

	 JALX is the 5-bit value 00011.  X is 0 for jal, 1 for jalx.
	 Note that the immediate value in the first word is swapped.

	 When producing a relocateable object file, R_MIPS16_26 is
	 handled mostly like R_MIPS_26.  In particular, the addend is
	 stored as a straight 26-bit value in a 32-bit instruction.
	 (gas makes life simpler for itself by never adjusting a
	 R_MIPS16_26 reloc to be against a section, so the addend is
	 always zero).  However, the 32 bit instruction is stored as 2
	 16-bit values, rather than a single 32-bit value.  In a
	 big-endian file, the result is the same; in a little-endian
	 file, the two 16-bit halves of the 32 bit value are swapped.
	 This is so that a disassembler can recognize the jal
	 instruction.

	 When doing a final link, R_MIPS16_26 is treated as a 32 bit
	 instruction stored as two 16-bit values.  The addend A is the
	 contents of the targ26 field.  The calculation is the same as
	 R_MIPS_26.  When storing the calculated value, reorder the
	 immediate value as shown above, and don't forget to store the
	 value as two 16-bit values.

	 To put it in MIPS ABI terms, the relocation field is T-targ26-16,
	 defined as

	 big-endian:
	 +--------+----------------------+
	 |        |                      |
	 |        |    targ26-16         |
	 |31    26|25                   0|
	 +--------+----------------------+

	 little-endian:
	 +----------+------+-------------+
	 |          |      |             |
	 |  sub1    |      |     sub2    |
	 |0        9|10  15|16         31|
	 +----------+--------------------+
	 where targ26-16 is sub1 followed by sub2 (i.e., the addend field A is
	 ((sub1 << 16) | sub2)).

	 When producing a relocateable object file, the calculation is
	 (((A < 2) | ((P + 4) & 0xf0000000) + S) >> 2)
	 When producing a fully linked file, the calculation is
	 let R = (((A < 2) | ((P + 4) & 0xf0000000) + S) >> 2)
	 ((R & 0x1f0000) << 5) | ((R & 0x3e00000) >> 5) | (R & 0xffff)  */

      if (!info->relocateable)
	/* Shuffle the bits according to the formula above.  */
	value = (((value & 0x1f0000) << 5)
		 | ((value & 0x3e00000) >> 5)
		 | (value & 0xffff));
    }
  else if (r_type == R_MIPS16_GPREL)
    {
      /* R_MIPS16_GPREL is used for GP-relative addressing in mips16
	 mode.  A typical instruction will have a format like this:

	 +--------------+--------------------------------+
	 !    EXTEND    !     Imm 10:5    !   Imm 15:11  !
	 +--------------+--------------------------------+
	 !    Major     !   rx   !   ry   !   Imm  4:0   !
	 +--------------+--------------------------------+

	 EXTEND is the five bit value 11110.  Major is the instruction
	 opcode.

	 This is handled exactly like R_MIPS_GPREL16, except that the
	 addend is retrieved and stored as shown in this diagram; that
	 is, the Imm fields above replace the V-rel16 field.

         All we need to do here is shuffle the bits appropriately.  As
	 above, the two 16-bit halves must be swapped on a
	 little-endian system.  */
      value = (((value & 0x7e0) << 16)
	       | ((value & 0xf800) << 5)
	       | (value & 0x1f));
    }

  /* Set the field.  */
  x |= (value & howto->dst_mask);

  /* If required, turn JAL into JALX.  */
  if (require_jalx)
    {
      boolean ok;
      bfd_vma opcode = x >> 26;
      bfd_vma jalx_opcode;

      /* Check to see if the opcode is already JAL or JALX.  */
      if (r_type == R_MIPS16_26)
	{
	  ok = ((opcode == 0x6) || (opcode == 0x7));
	  jalx_opcode = 0x7;
	}
      else
	{
	  ok = ((opcode == 0x3) || (opcode == 0x1d));
	  jalx_opcode = 0x1d;
	}

      /* If the opcode is not JAL or JALX, there's a problem.  */
      if (!ok)
	{
	  (*_bfd_error_handler)
	    (_("%s: %s+0x%lx: jump to stub routine which is not jal"),
	     bfd_archive_filename (input_bfd),
	     input_section->name,
	     (unsigned long) relocation->r_offset);
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}

      /* Make this the JALX opcode.  */
      x = (x & ~(0x3f << 26)) | (jalx_opcode << 26);
    }

  /* Swap the high- and low-order 16 bits on little-endian systems
     when doing a MIPS16 relocation.  */
  if ((r_type == R_MIPS16_GPREL || r_type == R_MIPS16_26)
      && bfd_little_endian (input_bfd))
    x = (((x & 0xffff) << 16) | ((x & 0xffff0000) >> 16));

  /* Put the value into the output.  */
  bfd_put (8 * bfd_get_reloc_size (howto), input_bfd, x, location);
  return true;
}

/* Returns true if SECTION is a MIPS16 stub section.  */

static boolean
mips_elf_stub_section_p (abfd, section)
     bfd *abfd ATTRIBUTE_UNUSED;
     asection *section;
{
  const char *name = bfd_get_section_name (abfd, section);

  return (strncmp (name, FN_STUB, sizeof FN_STUB - 1) == 0
	  || strncmp (name, CALL_STUB, sizeof CALL_STUB - 1) == 0
	  || strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0);
}

/* Relocate a MIPS ELF section.  */

boolean
_bfd_mips_elf_relocate_section (output_bfd, info, input_bfd, input_section,
				contents, relocs, local_syms, local_sections)
     bfd *output_bfd;
     struct bfd_link_info *info;
     bfd *input_bfd;
     asection *input_section;
     bfd_byte *contents;
     Elf_Internal_Rela *relocs;
     Elf_Internal_Sym *local_syms;
     asection **local_sections;
{
  Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *relend;
  bfd_vma addend = 0;
  boolean use_saved_addend_p = false;
  struct elf_backend_data *bed;

  bed = get_elf_backend_data (output_bfd);
  relend = relocs + input_section->reloc_count * bed->s->int_rels_per_ext_rel;
  for (rel = relocs; rel < relend; ++rel)
    {
      const char *name;
      bfd_vma value;
      reloc_howto_type *howto;
      boolean require_jalx;
      /* True if the relocation is a RELA relocation, rather than a
         REL relocation.  */
      boolean rela_relocation_p = true;
      unsigned int r_type = ELF32_R_TYPE (rel->r_info);
      const char * msg = (const char *) NULL;

      /* Find the relocation howto for this relocation.  */
      if (r_type == R_MIPS_64 && !ABI_64_P (output_bfd))
	{
	  /* Some 32-bit code uses R_MIPS_64.  In particular, people use
	     64-bit code, but make sure all their addresses are in the
	     lowermost or uppermost 32-bit section of the 64-bit address
	     space.  Thus, when they use an R_MIPS_64 they mean what is
	     usually meant by R_MIPS_32, with the exception that the
	     stored value is sign-extended to 64 bits.  */
	  howto = elf_mips_howto_table_rel + R_MIPS_32;

	  /* On big-endian systems, we need to lie about the position
	     of the reloc.  */
	  if (bfd_big_endian (input_bfd))
	    rel->r_offset += 4;
	}
      else
	howto = mips_rtype_to_howto (r_type);

      if (!use_saved_addend_p)
	{
	  Elf_Internal_Shdr *rel_hdr;

	  /* If these relocations were originally of the REL variety,
	     we must pull the addend out of the field that will be
	     relocated.  Otherwise, we simply use the contents of the
	     RELA relocation.  To determine which flavor or relocation
	     this is, we depend on the fact that the INPUT_SECTION's
	     REL_HDR is read before its REL_HDR2.  */
	  rel_hdr = &elf_section_data (input_section)->rel_hdr;
	  if ((size_t) (rel - relocs)
	      >= (NUM_SHDR_ENTRIES (rel_hdr) * bed->s->int_rels_per_ext_rel))
	    rel_hdr = elf_section_data (input_section)->rel_hdr2;
	  if (rel_hdr->sh_entsize == MIPS_ELF_REL_SIZE (input_bfd))
	    {
	      /* Note that this is a REL relocation.  */
	      rela_relocation_p = false;

	      /* Get the addend, which is stored in the input file.  */
	      addend = mips_elf_obtain_contents (howto,
						 rel,
						 input_bfd,
						 contents);
	      addend &= howto->src_mask;

	      /* For some kinds of relocations, the ADDEND is a
		 combination of the addend stored in two different
		 relocations.   */
	      if (r_type == R_MIPS_HI16
		  || r_type == R_MIPS_GNU_REL_HI16
		  || (r_type == R_MIPS_GOT16
		      && mips_elf_local_relocation_p (input_bfd, rel,
						      local_sections, false)))
		{
		  bfd_vma l;
		  const Elf_Internal_Rela *lo16_relocation;
		  reloc_howto_type *lo16_howto;
		  unsigned int lo;

		  /* The combined value is the sum of the HI16 addend,
		     left-shifted by sixteen bits, and the LO16
		     addend, sign extended.  (Usually, the code does
		     a `lui' of the HI16 value, and then an `addiu' of
		     the LO16 value.)

		     Scan ahead to find a matching LO16 relocation.  */
		  if (r_type == R_MIPS_GNU_REL_HI16)
		    lo = R_MIPS_GNU_REL_LO16;
		  else
		    lo = R_MIPS_LO16;
		  lo16_relocation
		    = mips_elf_next_relocation (lo, rel, relend);
		  if (lo16_relocation == NULL)
		    return false;

		  /* Obtain the addend kept there.  */
		  lo16_howto = mips_rtype_to_howto (lo);
		  l = mips_elf_obtain_contents (lo16_howto,
						lo16_relocation,
						input_bfd, contents);
		  l &= lo16_howto->src_mask;
		  l = mips_elf_sign_extend (l, 16);

		  addend <<= 16;

		  /* Compute the combined addend.  */
		  addend += l;

		  /* If PC-relative, subtract the difference between the
		     address of the LO part of the reloc and the address of
		     the HI part.  The relocation is relative to the LO
		     part, but mips_elf_calculate_relocation() doesn't know
		     it address or the difference from the HI part, so
		     we subtract that difference here.  See also the
		     comment in mips_elf_calculate_relocation().  */
		  if (r_type == R_MIPS_GNU_REL_HI16)
		    addend -= (lo16_relocation->r_offset - rel->r_offset);
		}
	      else if (r_type == R_MIPS16_GPREL)
		{
		  /* The addend is scrambled in the object file.  See
		     mips_elf_perform_relocation for details on the
		     format.  */
		  addend = (((addend & 0x1f0000) >> 5)
			    | ((addend & 0x7e00000) >> 16)
			    | (addend & 0x1f));
		}
	    }
	  else
	    addend = rel->r_addend;
	}

      if (info->relocateable)
	{
	  Elf_Internal_Sym *sym;
	  unsigned long r_symndx;

	  if (r_type == R_MIPS_64 && !ABI_64_P (output_bfd)
	      && bfd_big_endian (input_bfd))
	    rel->r_offset -= 4;

	  /* Since we're just relocating, all we need to do is copy
	     the relocations back out to the object file, unless
	     they're against a section symbol, in which case we need
	     to adjust by the section offset, or unless they're GP
	     relative in which case we need to adjust by the amount
	     that we're adjusting GP in this relocateable object.  */

	  if (!mips_elf_local_relocation_p (input_bfd, rel, local_sections,
					    false))
	    /* There's nothing to do for non-local relocations.  */
	    continue;

	  if (r_type == R_MIPS16_GPREL
	      || r_type == R_MIPS_GPREL16
	      || r_type == R_MIPS_GPREL32
	      || r_type == R_MIPS_LITERAL)
	    addend -= (_bfd_get_gp_value (output_bfd)
		       - _bfd_get_gp_value (input_bfd));
	  else if (r_type == R_MIPS_26 || r_type == R_MIPS16_26
		   || r_type == R_MIPS_GNU_REL16_S2)
	    /* The addend is stored without its two least
	       significant bits (which are always zero.)  In a
	       non-relocateable link, calculate_relocation will do
	       this shift; here, we must do it ourselves.  */
	    addend <<= 2;

	  r_symndx = ELF32_R_SYM (rel->r_info);
	  sym = local_syms + r_symndx;
	  if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
	    /* Adjust the addend appropriately.  */
	    addend += local_sections[r_symndx]->output_offset;

	  /* If the relocation is for a R_MIPS_HI16 or R_MIPS_GOT16,
	     then we only want to write out the high-order 16 bits.
	     The subsequent R_MIPS_LO16 will handle the low-order bits.  */
	  if (r_type == R_MIPS_HI16 || r_type == R_MIPS_GOT16
	      || r_type == R_MIPS_GNU_REL_HI16)
	    addend = mips_elf_high (addend);
	  /* If the relocation is for an R_MIPS_26 relocation, then
	     the two low-order bits are not stored in the object file;
	     they are implicitly zero.  */
	  else if (r_type == R_MIPS_26 || r_type == R_MIPS16_26
		   || r_type == R_MIPS_GNU_REL16_S2)
	    addend >>= 2;

	  if (rela_relocation_p)
	    /* If this is a RELA relocation, just update the addend.
	       We have to cast away constness for REL.  */
	    rel->r_addend = addend;
	  else
	    {
	      /* Otherwise, we have to write the value back out.  Note
		 that we use the source mask, rather than the
		 destination mask because the place to which we are
		 writing will be source of the addend in the final
		 link.  */
	      addend &= howto->src_mask;

	      if (r_type == R_MIPS_64 && !ABI_64_P (output_bfd))
		/* See the comment above about using R_MIPS_64 in the 32-bit
		   ABI.  Here, we need to update the addend.  It would be
		   possible to get away with just using the R_MIPS_32 reloc
		   but for endianness.  */
		{
		  bfd_vma sign_bits;
		  bfd_vma low_bits;
		  bfd_vma high_bits;

		  if (addend & ((bfd_vma) 1 << 31))
#ifdef BFD64
		    sign_bits = ((bfd_vma) 1 << 32) - 1;
#else
		    sign_bits = -1;
#endif
		  else
		    sign_bits = 0;

		  /* If we don't know that we have a 64-bit type,
		     do two separate stores.  */
		  if (bfd_big_endian (input_bfd))
		    {
		      /* Store the sign-bits (which are most significant)
			 first.  */
		      low_bits = sign_bits;
		      high_bits = addend;
		    }
		  else
		    {
		      low_bits = addend;
		      high_bits = sign_bits;
		    }
		  bfd_put_32 (input_bfd, low_bits,
			      contents + rel->r_offset);
		  bfd_put_32 (input_bfd, high_bits,
			      contents + rel->r_offset + 4);
		  continue;
		}

	      if (!mips_elf_perform_relocation (info, howto, rel, addend,
						input_bfd, input_section,
						contents, false))
		return false;
	    }

	  /* Go on to the next relocation.  */
	  continue;
	}

      /* In the N32 and 64-bit ABIs there may be multiple consecutive
	 relocations for the same offset.  In that case we are
	 supposed to treat the output of each relocation as the addend
	 for the next.  */
      if (rel + 1 < relend
	  && rel->r_offset == rel[1].r_offset
	  && ELF32_R_TYPE (rel[1].r_info) != R_MIPS_NONE)
	use_saved_addend_p = true;
      else
	use_saved_addend_p = false;

      /* Figure out what value we are supposed to relocate.  */
      switch (mips_elf_calculate_relocation (output_bfd,
					     input_bfd,
					     input_section,
					     info,
					     rel,
					     addend,
					     howto,
					     local_syms,
					     local_sections,
					     &value,
					     &name,
					     &require_jalx))
	{
	case bfd_reloc_continue:
	  /* There's nothing to do.  */
	  continue;

	case bfd_reloc_undefined:
	  /* mips_elf_calculate_relocation already called the
	     undefined_symbol callback.  There's no real point in
	     trying to perform the relocation at this point, so we
	     just skip ahead to the next relocation.  */
	  continue;

	case bfd_reloc_notsupported:
	  msg = _("internal error: unsupported relocation error");
	  info->callbacks->warning
	    (info, msg, name, input_bfd, input_section, rel->r_offset);
	  return false;

	case bfd_reloc_overflow:
	  if (use_saved_addend_p)
	    /* Ignore overflow until we reach the last relocation for
	       a given location.  */
	    ;
	  else
	    {
	      BFD_ASSERT (name != NULL);
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, name, howto->name, (bfd_vma) 0,
		      input_bfd, input_section, rel->r_offset)))
		return false;
	    }
	  break;

	case bfd_reloc_ok:
	  break;

	default:
	  abort ();
	  break;
	}

      /* If we've got another relocation for the address, keep going
	 until we reach the last one.  */
      if (use_saved_addend_p)
	{
	  addend = value;
	  continue;
	}

      if (r_type == R_MIPS_64 && !ABI_64_P (output_bfd))
	/* See the comment above about using R_MIPS_64 in the 32-bit
	   ABI.  Until now, we've been using the HOWTO for R_MIPS_32;
	   that calculated the right value.  Now, however, we
	   sign-extend the 32-bit result to 64-bits, and store it as a
	   64-bit value.  We are especially generous here in that we
	   go to extreme lengths to support this usage on systems with
	   only a 32-bit VMA.  */
	{
	  bfd_vma sign_bits;
	  bfd_vma low_bits;
	  bfd_vma high_bits;

	  if (value & ((bfd_vma) 1 << 31))
#ifdef BFD64
	    sign_bits = ((bfd_vma) 1 << 32) - 1;
#else
	    sign_bits = -1;
#endif
	  else
	    sign_bits = 0;

	  /* If we don't know that we have a 64-bit type,
	     do two separate stores.  */
	  if (bfd_big_endian (input_bfd))
	    {
	      /* Undo what we did above.  */
	      rel->r_offset -= 4;
	      /* Store the sign-bits (which are most significant)
		 first.  */
	      low_bits = sign_bits;
	      high_bits = value;
	    }
	  else
	    {
	      low_bits = value;
	      high_bits = sign_bits;
	    }
	  bfd_put_32 (input_bfd, low_bits,
		      contents + rel->r_offset);
	  bfd_put_32 (input_bfd, high_bits,
		      contents + rel->r_offset + 4);
	  continue;
	}

      /* Actually perform the relocation.  */
      if (!mips_elf_perform_relocation (info, howto, rel, value, input_bfd,
					input_section, contents,
					require_jalx))
	return false;
    }

  return true;
}

/* This hook function is called before the linker writes out a global
   symbol.  We mark symbols as small common if appropriate.  This is
   also where we undo the increment of the value for a mips16 symbol.  */

boolean
_bfd_mips_elf_link_output_symbol_hook (abfd, info, name, sym, input_sec)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     const char *name ATTRIBUTE_UNUSED;
     Elf_Internal_Sym *sym;
     asection *input_sec;
{
  /* If we see a common symbol, which implies a relocatable link, then
     if a symbol was small common in an input file, mark it as small
     common in the output file.  */
  if (sym->st_shndx == SHN_COMMON
      && strcmp (input_sec->name, ".scommon") == 0)
    sym->st_shndx = SHN_MIPS_SCOMMON;

  if (sym->st_other == STO_MIPS16
      && (sym->st_value & 1) != 0)
    --sym->st_value;

  return true;
}

/* Functions for the dynamic linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER(abfd) 		\
   (ABI_N32_P (abfd) ? "/usr/lib32/libc.so.1" 	\
    : ABI_64_P (abfd) ? "/usr/lib64/libc.so.1" 	\
    : "/usr/lib/libc.so.1")

/* Create dynamic sections when linking against a dynamic object.  */

boolean
_bfd_mips_elf_create_dynamic_sections (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  struct elf_link_hash_entry *h;
  flagword flags;
  register asection *s;
  const char * const *namep;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED | SEC_READONLY);

  /* Mips ABI requests the .dynamic section to be read only.  */
  s = bfd_get_section_by_name (abfd, ".dynamic");
  if (s != NULL)
    {
      if (! bfd_set_section_flags (abfd, s, flags))
	return false;
    }

  /* We need to create .got section.  */
  if (! mips_elf_create_got_section (abfd, info))
    return false;

  /* Create the .msym section on IRIX6.  It is used by the dynamic
     linker to speed up dynamic relocations, and to avoid computing
     the ELF hash for symbols.  */
  if (IRIX_COMPAT (abfd) == ict_irix6
      && !mips_elf_create_msym_section (abfd))
    return false;

  /* Create .stub section.  */
  if (bfd_get_section_by_name (abfd,
			       MIPS_ELF_STUB_SECTION_NAME (abfd)) == NULL)
    {
      s = bfd_make_section (abfd, MIPS_ELF_STUB_SECTION_NAME (abfd));
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags | SEC_CODE)
	  || ! bfd_set_section_alignment (abfd, s,
					  MIPS_ELF_LOG_FILE_ALIGN (abfd)))
	return false;
    }

  if ((IRIX_COMPAT (abfd) == ict_irix5 || IRIX_COMPAT (abfd) == ict_none)
      && !info->shared
      && bfd_get_section_by_name (abfd, ".rld_map") == NULL)
    {
      s = bfd_make_section (abfd, ".rld_map");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags &~ (flagword) SEC_READONLY)
	  || ! bfd_set_section_alignment (abfd, s,
					  MIPS_ELF_LOG_FILE_ALIGN (abfd)))
	return false;
    }

  /* On IRIX5, we adjust add some additional symbols and change the
     alignments of several sections.  There is no ABI documentation
     indicating that this is necessary on IRIX6, nor any evidence that
     the linker takes such action.  */
  if (IRIX_COMPAT (abfd) == ict_irix5)
    {
      for (namep = mips_elf_dynsym_rtproc_names; *namep != NULL; namep++)
	{
	  h = NULL;
	  if (! (_bfd_generic_link_add_one_symbol
		 (info, abfd, *namep, BSF_GLOBAL, bfd_und_section_ptr,
		  (bfd_vma) 0, (const char *) NULL, false,
		  get_elf_backend_data (abfd)->collect,
		  (struct bfd_link_hash_entry **) &h)))
	    return false;
	  h->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  h->type = STT_SECTION;

	  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	    return false;
	}

      /* We need to create a .compact_rel section.  */
      if (SGI_COMPAT (abfd))
	{
	  if (!mips_elf_create_compact_rel_section (abfd, info))
	    return false;
	}

      /* Change aligments of some sections.  */
      s = bfd_get_section_by_name (abfd, ".hash");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".dynsym");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".dynstr");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".reginfo");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
      s = bfd_get_section_by_name (abfd, ".dynamic");
      if (s != NULL)
	bfd_set_section_alignment (abfd, s, 4);
    }

  if (!info->shared)
    {
      h = NULL;
      if (SGI_COMPAT (abfd))
	{
	  if (!(_bfd_generic_link_add_one_symbol
		(info, abfd, "_DYNAMIC_LINK", BSF_GLOBAL, bfd_abs_section_ptr,
		 (bfd_vma) 0, (const char *) NULL, false,
		 get_elf_backend_data (abfd)->collect,
		 (struct bfd_link_hash_entry **) &h)))
	    return false;
	}
      else
	{
	  /* For normal mips it is _DYNAMIC_LINKING.  */
	  if (!(_bfd_generic_link_add_one_symbol
		(info, abfd, "_DYNAMIC_LINKING", BSF_GLOBAL,
		 bfd_abs_section_ptr, (bfd_vma) 0, (const char *) NULL, false,
		 get_elf_backend_data (abfd)->collect,
		 (struct bfd_link_hash_entry **) &h)))
	    return false;
	}
      h->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;
      h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
      h->type = STT_SECTION;

      if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	return false;

      if (! mips_elf_hash_table (info)->use_rld_obj_head)
	{
	  /* __rld_map is a four byte word located in the .data section
	     and is filled in by the rtld to contain a pointer to
	     the _r_debug structure. Its symbol value will be set in
	     mips_elf_finish_dynamic_symbol.  */
	  s = bfd_get_section_by_name (abfd, ".rld_map");
	  BFD_ASSERT (s != NULL);

	  h = NULL;
	  if (SGI_COMPAT (abfd))
	    {
	      if (!(_bfd_generic_link_add_one_symbol
		    (info, abfd, "__rld_map", BSF_GLOBAL, s,
		     (bfd_vma) 0, (const char *) NULL, false,
		     get_elf_backend_data (abfd)->collect,
		     (struct bfd_link_hash_entry **) &h)))
		return false;
	    }
	  else
	    {
	      /* For normal mips the symbol is __RLD_MAP.  */
	      if (!(_bfd_generic_link_add_one_symbol
		    (info, abfd, "__RLD_MAP", BSF_GLOBAL, s,
		     (bfd_vma) 0, (const char *) NULL, false,
		     get_elf_backend_data (abfd)->collect,
		     (struct bfd_link_hash_entry **) &h)))
		return false;
	    }
	  h->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;
	  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
	  h->type = STT_OBJECT;

	  if (! bfd_elf32_link_record_dynamic_symbol (info, h))
	    return false;
	}
    }

  return true;
}

/* Create the .compact_rel section.  */

static boolean
mips_elf_create_compact_rel_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
{
  flagword flags;
  register asection *s;

  if (bfd_get_section_by_name (abfd, ".compact_rel") == NULL)
    {
      flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY | SEC_LINKER_CREATED
	       | SEC_READONLY);

      s = bfd_make_section (abfd, ".compact_rel");
      if (s == NULL
	  || ! bfd_set_section_flags (abfd, s, flags)
	  || ! bfd_set_section_alignment (abfd, s,
					  MIPS_ELF_LOG_FILE_ALIGN (abfd)))
	return false;

      s->_raw_size = sizeof (Elf32_External_compact_rel);
    }

  return true;
}

/* Create the .got section to hold the global offset table.  */

static boolean
mips_elf_create_got_section (abfd, info)
     bfd *abfd;
     struct bfd_link_info *info;
{
  flagword flags;
  register asection *s;
  struct elf_link_hash_entry *h;
  struct mips_got_info *g;
  bfd_size_type amt;

  /* This function may be called more than once.  */
  if (mips_elf_got_section (abfd))
    return true;

  flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	   | SEC_LINKER_CREATED);

  s = bfd_make_section (abfd, ".got");
  if (s == NULL
      || ! bfd_set_section_flags (abfd, s, flags)
      || ! bfd_set_section_alignment (abfd, s, 4))
    return false;

  /* Define the symbol _GLOBAL_OFFSET_TABLE_.  We don't do this in the
     linker script because we don't want to define the symbol if we
     are not creating a global offset table.  */
  h = NULL;
  if (! (_bfd_generic_link_add_one_symbol
	 (info, abfd, "_GLOBAL_OFFSET_TABLE_", BSF_GLOBAL, s,
	  (bfd_vma) 0, (const char *) NULL, false,
	  get_elf_backend_data (abfd)->collect,
	  (struct bfd_link_hash_entry **) &h)))
    return false;
  h->elf_link_hash_flags &= ~ELF_LINK_NON_ELF;
  h->elf_link_hash_flags |= ELF_LINK_HASH_DEF_REGULAR;
  h->type = STT_OBJECT;

  if (info->shared
      && ! bfd_elf32_link_record_dynamic_symbol (info, h))
    return false;

  /* The first several global offset table entries are reserved.  */
  s->_raw_size = MIPS_RESERVED_GOTNO * MIPS_ELF_GOT_SIZE (abfd);

  amt = sizeof (struct mips_got_info);
  g = (struct mips_got_info *) bfd_alloc (abfd, amt);
  if (g == NULL)
    return false;
  g->global_gotsym = NULL;
  g->local_gotno = MIPS_RESERVED_GOTNO;
  g->assigned_gotno = MIPS_RESERVED_GOTNO;
  if (elf_section_data (s) == NULL)
    {
      amt = sizeof (struct bfd_elf_section_data);
      s->used_by_bfd = (PTR) bfd_zalloc (abfd, amt);
      if (elf_section_data (s) == NULL)
	return false;
    }
  elf_section_data (s)->tdata = (PTR) g;
  elf_section_data (s)->this_hdr.sh_flags
    |= SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL;

  return true;
}

/* Returns the .msym section for ABFD, creating it if it does not
   already exist.  Returns NULL to indicate error.  */

static asection *
mips_elf_create_msym_section (abfd)
     bfd *abfd;
{
  asection *s;

  s = bfd_get_section_by_name (abfd, MIPS_ELF_MSYM_SECTION_NAME (abfd));
  if (!s)
    {
      s = bfd_make_section (abfd, MIPS_ELF_MSYM_SECTION_NAME (abfd));
      if (!s
	  || !bfd_set_section_flags (abfd, s,
				     SEC_ALLOC
				     | SEC_LOAD
				     | SEC_HAS_CONTENTS
				     | SEC_LINKER_CREATED
				     | SEC_READONLY)
	  || !bfd_set_section_alignment (abfd, s,
					 MIPS_ELF_LOG_FILE_ALIGN (abfd)))
	return NULL;
    }

  return s;
}

/* Add room for N relocations to the .rel.dyn section in ABFD.  */

static void
mips_elf_allocate_dynamic_relocations (abfd, n)
     bfd *abfd;
     unsigned int n;
{
  asection *s;

  s = bfd_get_section_by_name (abfd, MIPS_ELF_REL_DYN_SECTION_NAME (abfd));
  BFD_ASSERT (s != NULL);

  if (s->_raw_size == 0)
    {
      /* Make room for a null element.  */
      s->_raw_size += MIPS_ELF_REL_SIZE (abfd);
      ++s->reloc_count;
    }
  s->_raw_size += n * MIPS_ELF_REL_SIZE (abfd);
}

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table.  */

boolean
_bfd_mips_elf_check_relocs (abfd, info, sec, relocs)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *sec;
     const Elf_Internal_Rela *relocs;
{
  const char *name;
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  struct mips_got_info *g;
  size_t extsymoff;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sgot;
  asection *sreloc;
  struct elf_backend_data *bed;

  if (info->relocateable)
    return true;

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  extsymoff = (elf_bad_symtab (abfd)) ? 0 : symtab_hdr->sh_info;

  /* Check for the mips16 stub sections.  */

  name = bfd_get_section_name (abfd, sec);
  if (strncmp (name, FN_STUB, sizeof FN_STUB - 1) == 0)
    {
      unsigned long r_symndx;

      /* Look at the relocation information to figure out which symbol
         this is for.  */

      r_symndx = ELF32_R_SYM (relocs->r_info);

      if (r_symndx < extsymoff
	  || sym_hashes[r_symndx - extsymoff] == NULL)
	{
	  asection *o;

	  /* This stub is for a local symbol.  This stub will only be
             needed if there is some relocation in this BFD, other
             than a 16 bit function call, which refers to this symbol.  */
	  for (o = abfd->sections; o != NULL; o = o->next)
	    {
	      Elf_Internal_Rela *sec_relocs;
	      const Elf_Internal_Rela *r, *rend;

	      /* We can ignore stub sections when looking for relocs.  */
	      if ((o->flags & SEC_RELOC) == 0
		  || o->reloc_count == 0
		  || strncmp (bfd_get_section_name (abfd, o), FN_STUB,
			      sizeof FN_STUB - 1) == 0
		  || strncmp (bfd_get_section_name (abfd, o), CALL_STUB,
			      sizeof CALL_STUB - 1) == 0
		  || strncmp (bfd_get_section_name (abfd, o), CALL_FP_STUB,
			      sizeof CALL_FP_STUB - 1) == 0)
		continue;

	      sec_relocs = (_bfd_elf32_link_read_relocs
			    (abfd, o, (PTR) NULL,
			     (Elf_Internal_Rela *) NULL,
			     info->keep_memory));
	      if (sec_relocs == NULL)
		return false;

	      rend = sec_relocs + o->reloc_count;
	      for (r = sec_relocs; r < rend; r++)
		if (ELF32_R_SYM (r->r_info) == r_symndx
		    && ELF32_R_TYPE (r->r_info) != R_MIPS16_26)
		  break;

	      if (! info->keep_memory)
		free (sec_relocs);

	      if (r < rend)
		break;
	    }

	  if (o == NULL)
	    {
	      /* There is no non-call reloc for this stub, so we do
                 not need it.  Since this function is called before
                 the linker maps input sections to output sections, we
                 can easily discard it by setting the SEC_EXCLUDE
                 flag.  */
	      sec->flags |= SEC_EXCLUDE;
	      return true;
	    }

	  /* Record this stub in an array of local symbol stubs for
             this BFD.  */
	  if (elf_tdata (abfd)->local_stubs == NULL)
	    {
	      unsigned long symcount;
	      asection **n;
	      bfd_size_type amt;

	      if (elf_bad_symtab (abfd))
		symcount = NUM_SHDR_ENTRIES (symtab_hdr);
	      else
		symcount = symtab_hdr->sh_info;
	      amt = symcount * sizeof (asection *);
	      n = (asection **) bfd_zalloc (abfd, amt);
	      if (n == NULL)
		return false;
	      elf_tdata (abfd)->local_stubs = n;
	    }

	  elf_tdata (abfd)->local_stubs[r_symndx] = sec;

	  /* We don't need to set mips16_stubs_seen in this case.
             That flag is used to see whether we need to look through
             the global symbol table for stubs.  We don't need to set
             it here, because we just have a local stub.  */
	}
      else
	{
	  struct mips_elf_link_hash_entry *h;

	  h = ((struct mips_elf_link_hash_entry *)
	       sym_hashes[r_symndx - extsymoff]);

	  /* H is the symbol this stub is for.  */

	  h->fn_stub = sec;
	  mips_elf_hash_table (info)->mips16_stubs_seen = true;
	}
    }
  else if (strncmp (name, CALL_STUB, sizeof CALL_STUB - 1) == 0
	   || strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
    {
      unsigned long r_symndx;
      struct mips_elf_link_hash_entry *h;
      asection **loc;

      /* Look at the relocation information to figure out which symbol
         this is for.  */

      r_symndx = ELF32_R_SYM (relocs->r_info);

      if (r_symndx < extsymoff
	  || sym_hashes[r_symndx - extsymoff] == NULL)
	{
	  /* This stub was actually built for a static symbol defined
	     in the same file.  We assume that all static symbols in
	     mips16 code are themselves mips16, so we can simply
	     discard this stub.  Since this function is called before
	     the linker maps input sections to output sections, we can
	     easily discard it by setting the SEC_EXCLUDE flag.  */
	  sec->flags |= SEC_EXCLUDE;
	  return true;
	}

      h = ((struct mips_elf_link_hash_entry *)
	   sym_hashes[r_symndx - extsymoff]);

      /* H is the symbol this stub is for.  */

      if (strncmp (name, CALL_FP_STUB, sizeof CALL_FP_STUB - 1) == 0)
	loc = &h->call_fp_stub;
      else
	loc = &h->call_stub;

      /* If we already have an appropriate stub for this function, we
	 don't need another one, so we can discard this one.  Since
	 this function is called before the linker maps input sections
	 to output sections, we can easily discard it by setting the
	 SEC_EXCLUDE flag.  We can also discard this section if we
	 happen to already know that this is a mips16 function; it is
	 not necessary to check this here, as it is checked later, but
	 it is slightly faster to check now.  */
      if (*loc != NULL || h->root.other == STO_MIPS16)
	{
	  sec->flags |= SEC_EXCLUDE;
	  return true;
	}

      *loc = sec;
      mips_elf_hash_table (info)->mips16_stubs_seen = true;
    }

  if (dynobj == NULL)
    {
      sgot = NULL;
      g = NULL;
    }
  else
    {
      sgot = mips_elf_got_section (dynobj);
      if (sgot == NULL)
	g = NULL;
      else
	{
	  BFD_ASSERT (elf_section_data (sgot) != NULL);
	  g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
	  BFD_ASSERT (g != NULL);
	}
    }

  sreloc = NULL;
  bed = get_elf_backend_data (abfd);
  rel_end = relocs + sec->reloc_count * bed->s->int_rels_per_ext_rel;
  for (rel = relocs; rel < rel_end; ++rel)
    {
      unsigned long r_symndx;
      unsigned int r_type;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);

      if (r_symndx < extsymoff)
	h = NULL;
      else if (r_symndx >= extsymoff + NUM_SHDR_ENTRIES (symtab_hdr))
	{
	  (*_bfd_error_handler)
	    (_("%s: Malformed reloc detected for section %s"),
	     bfd_archive_filename (abfd), name);
	  bfd_set_error (bfd_error_bad_value);
	  return false;
	}
      else
	{
	  h = sym_hashes[r_symndx - extsymoff];

	  /* This may be an indirect symbol created because of a version.  */
	  if (h != NULL)
	    {
	      while (h->root.type == bfd_link_hash_indirect)
		h = (struct elf_link_hash_entry *) h->root.u.i.link;
	    }
	}

      /* Some relocs require a global offset table.  */
      if (dynobj == NULL || sgot == NULL)
	{
	  switch (r_type)
	    {
	    case R_MIPS_GOT16:
	    case R_MIPS_CALL16:
	    case R_MIPS_CALL_HI16:
	    case R_MIPS_CALL_LO16:
	    case R_MIPS_GOT_HI16:
	    case R_MIPS_GOT_LO16:
	    case R_MIPS_GOT_PAGE:
	    case R_MIPS_GOT_OFST:
	    case R_MIPS_GOT_DISP:
	      if (dynobj == NULL)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      if (! mips_elf_create_got_section (dynobj, info))
		return false;
	      g = mips_elf_got_info (dynobj, &sgot);
	      break;

	    case R_MIPS_32:
	    case R_MIPS_REL32:
	    case R_MIPS_64:
	      if (dynobj == NULL
		  && (info->shared || h != NULL)
		  && (sec->flags & SEC_ALLOC) != 0)
		elf_hash_table (info)->dynobj = dynobj = abfd;
	      break;

	    default:
	      break;
	    }
	}

      if (!h && (r_type == R_MIPS_CALL_LO16
		 || r_type == R_MIPS_GOT_LO16
		 || r_type == R_MIPS_GOT_DISP))
	{
	  /* We may need a local GOT entry for this relocation.  We
	     don't count R_MIPS_GOT_PAGE because we can estimate the
	     maximum number of pages needed by looking at the size of
	     the segment.  Similar comments apply to R_MIPS_GOT16 and
	     R_MIPS_CALL16.  We don't count R_MIPS_GOT_HI16, or
	     R_MIPS_CALL_HI16 because these are always followed by an
	     R_MIPS_GOT_LO16 or R_MIPS_CALL_LO16.

	     This estimation is very conservative since we can merge
	     duplicate entries in the GOT.  In order to be less
	     conservative, we could actually build the GOT here,
	     rather than in relocate_section.  */
	  g->local_gotno++;
	  sgot->_raw_size += MIPS_ELF_GOT_SIZE (dynobj);
	}

      switch (r_type)
	{
	case R_MIPS_CALL16:
	  if (h == NULL)
	    {
	      (*_bfd_error_handler)
		(_("%s: CALL16 reloc at 0x%lx not against global symbol"),
		 bfd_archive_filename (abfd), (unsigned long) rel->r_offset);
	      bfd_set_error (bfd_error_bad_value);
	      return false;
	    }
	  /* Fall through.  */

	case R_MIPS_CALL_HI16:
	case R_MIPS_CALL_LO16:
	  if (h != NULL)
	    {
	      /* This symbol requires a global offset table entry.  */
	      if (!mips_elf_record_global_got_symbol (h, info, g))
		return false;

	      /* We need a stub, not a plt entry for the undefined
		 function.  But we record it as if it needs plt.  See
		 elf_adjust_dynamic_symbol in elflink.h.  */
	      h->elf_link_hash_flags |= ELF_LINK_HASH_NEEDS_PLT;
	      h->type = STT_FUNC;
	    }
	  break;

	case R_MIPS_GOT16:
	case R_MIPS_GOT_HI16:
	case R_MIPS_GOT_LO16:
	case R_MIPS_GOT_DISP:
	  /* This symbol requires a global offset table entry.  */
	  if (h && !mips_elf_record_global_got_symbol (h, info, g))
	    return false;
	  break;

	case R_MIPS_32:
	case R_MIPS_REL32:
	case R_MIPS_64:
	  if ((info->shared || h != NULL)
	      && (sec->flags & SEC_ALLOC) != 0)
	    {
	      if (sreloc == NULL)
		{
		  const char *dname = MIPS_ELF_REL_DYN_SECTION_NAME (dynobj);

		  sreloc = bfd_get_section_by_name (dynobj, dname);
		  if (sreloc == NULL)
		    {
		      sreloc = bfd_make_section (dynobj, dname);
		      if (sreloc == NULL
			  || ! bfd_set_section_flags (dynobj, sreloc,
						      (SEC_ALLOC
						       | SEC_LOAD
						       | SEC_HAS_CONTENTS
						       | SEC_IN_MEMORY
						       | SEC_LINKER_CREATED
						       | SEC_READONLY))
			  || ! bfd_set_section_alignment (dynobj, sreloc,
							  4))
			return false;
		    }
		}
#define MIPS_READONLY_SECTION (SEC_ALLOC | SEC_LOAD | SEC_READONLY)
	      if (info->shared)
		{
		  /* When creating a shared object, we must copy these
		     reloc types into the output file as R_MIPS_REL32
		     relocs.  We make room for this reloc in the
		     .rel.dyn reloc section.  */
		  mips_elf_allocate_dynamic_relocations (dynobj, 1);
		  if ((sec->flags & MIPS_READONLY_SECTION)
		      == MIPS_READONLY_SECTION)
		    /* We tell the dynamic linker that there are
		       relocations against the text segment.  */
		    info->flags |= DF_TEXTREL;
		}
	      else
		{
		  struct mips_elf_link_hash_entry *hmips;

		  /* We only need to copy this reloc if the symbol is
                     defined in a dynamic object.  */
		  hmips = (struct mips_elf_link_hash_entry *) h;
		  ++hmips->possibly_dynamic_relocs;
		  if ((sec->flags & MIPS_READONLY_SECTION)
		      == MIPS_READONLY_SECTION)
		    /* We need it to tell the dynamic linker if there
		       are relocations against the text segment.  */
		    hmips->readonly_reloc = true;
		}

	      /* Even though we don't directly need a GOT entry for
		 this symbol, a symbol must have a dynamic symbol
		 table index greater that DT_MIPS_GOTSYM if there are
		 dynamic relocations against it.  */
	      if (h != NULL
		  && !mips_elf_record_global_got_symbol (h, info, g))
		return false;
	    }

	  if (SGI_COMPAT (abfd))
	    mips_elf_hash_table (info)->compact_rel_size +=
	      sizeof (Elf32_External_crinfo);
	  break;

	case R_MIPS_26:
	case R_MIPS_GPREL16:
	case R_MIPS_LITERAL:
	case R_MIPS_GPREL32:
	  if (SGI_COMPAT (abfd))
	    mips_elf_hash_table (info)->compact_rel_size +=
	      sizeof (Elf32_External_crinfo);
	  break;

	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	case R_MIPS_GNU_VTINHERIT:
	  if (!_bfd_elf32_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	case R_MIPS_GNU_VTENTRY:
	  if (!_bfd_elf32_gc_record_vtentry (abfd, sec, h, rel->r_offset))
	    return false;
	  break;

	default:
	  break;
	}

      /* We must not create a stub for a symbol that has relocations
         related to taking the function's address.  */
      switch (r_type)
	{
	default:
	  if (h != NULL)
	    {
	      struct mips_elf_link_hash_entry *mh;

	      mh = (struct mips_elf_link_hash_entry *) h;
	      mh->no_fn_stub = true;
	    }
	  break;
	case R_MIPS_CALL16:
	case R_MIPS_CALL_HI16:
	case R_MIPS_CALL_LO16:
	  break;
	}

      /* If this reloc is not a 16 bit call, and it has a global
         symbol, then we will need the fn_stub if there is one.
         References from a stub section do not count.  */
      if (h != NULL
	  && r_type != R_MIPS16_26
	  && strncmp (bfd_get_section_name (abfd, sec), FN_STUB,
		      sizeof FN_STUB - 1) != 0
	  && strncmp (bfd_get_section_name (abfd, sec), CALL_STUB,
		      sizeof CALL_STUB - 1) != 0
	  && strncmp (bfd_get_section_name (abfd, sec), CALL_FP_STUB,
		      sizeof CALL_FP_STUB - 1) != 0)
	{
	  struct mips_elf_link_hash_entry *mh;

	  mh = (struct mips_elf_link_hash_entry *) h;
	  mh->need_fn_stub = true;
	}
    }

  return true;
}

/* Return the section that should be marked against GC for a given
   relocation.  */

asection *
_bfd_mips_elf_gc_mark_hook (abfd, info, rel, h, sym)
     bfd *abfd;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     Elf_Internal_Rela *rel;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  /* ??? Do mips16 stub sections need to be handled special?  */

  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_MIPS_GNU_VTINHERIT:
	case R_MIPS_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    {
      return bfd_section_from_elf_index (abfd, sym->st_shndx);
    }

  return NULL;
}

/* Update the got entry reference counts for the section being removed.  */

boolean
_bfd_mips_elf_gc_sweep_hook (abfd, info, sec, relocs)
     bfd *abfd ATTRIBUTE_UNUSED;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sec ATTRIBUTE_UNUSED;
     const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED;
{
#if 0
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;
  unsigned long r_symndx;
  struct elf_link_hash_entry *h;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_MIPS_GOT16:
      case R_MIPS_CALL16:
      case R_MIPS_CALL_HI16:
      case R_MIPS_CALL_LO16:
      case R_MIPS_GOT_HI16:
      case R_MIPS_GOT_LO16:
	/* ??? It would seem that the existing MIPS code does no sort
	   of reference counting or whatnot on its GOT and PLT entries,
	   so it is not possible to garbage collect them at this time.  */
	break;

      default:
	break;
      }
#endif

  return true;
}

/* Copy data from a MIPS ELF indirect symbol to its direct symbol,
   hiding the old indirect symbol.  Process additional relocation
   information.  Also called for weakdefs, in which case we just let
   _bfd_elf_link_hash_copy_indirect copy the flags for us.  */

static void
_bfd_mips_elf_copy_indirect_symbol (dir, ind)
     struct elf_link_hash_entry *dir, *ind;
{
  struct mips_elf_link_hash_entry *dirmips, *indmips;

  _bfd_elf_link_hash_copy_indirect (dir, ind);

  if (ind->root.type != bfd_link_hash_indirect)
    return;

  dirmips = (struct mips_elf_link_hash_entry *) dir;
  indmips = (struct mips_elf_link_hash_entry *) ind;
  dirmips->possibly_dynamic_relocs += indmips->possibly_dynamic_relocs;
  if (indmips->readonly_reloc)
    dirmips->readonly_reloc = true;
  if (dirmips->min_dyn_reloc_index == 0
      || (indmips->min_dyn_reloc_index != 0
	  && indmips->min_dyn_reloc_index < dirmips->min_dyn_reloc_index))
    dirmips->min_dyn_reloc_index = indmips->min_dyn_reloc_index;
  if (indmips->no_fn_stub)
    dirmips->no_fn_stub = true;
}

/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

boolean
_bfd_mips_elf_adjust_dynamic_symbol (info, h)
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
{
  bfd *dynobj;
  struct mips_elf_link_hash_entry *hmips;
  asection *s;

  dynobj = elf_hash_table (info)->dynobj;

  /* Make sure we know what is going on here.  */
  BFD_ASSERT (dynobj != NULL
	      && ((h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT)
		  || h->weakdef != NULL
		  || ((h->elf_link_hash_flags
		       & ELF_LINK_HASH_DEF_DYNAMIC) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_REF_REGULAR) != 0
		      && (h->elf_link_hash_flags
			  & ELF_LINK_HASH_DEF_REGULAR) == 0)));

  /* If this symbol is defined in a dynamic object, we need to copy
     any R_MIPS_32 or R_MIPS_REL32 relocs against it into the output
     file.  */
  hmips = (struct mips_elf_link_hash_entry *) h;
  if (! info->relocateable
      && hmips->possibly_dynamic_relocs != 0
      && (h->root.type == bfd_link_hash_defweak
	  || (h->elf_link_hash_flags
	      & ELF_LINK_HASH_DEF_REGULAR) == 0))
    {
      mips_elf_allocate_dynamic_relocations (dynobj,
					     hmips->possibly_dynamic_relocs);
      if (hmips->readonly_reloc)
	/* We tell the dynamic linker that there are relocations
	   against the text segment.  */
	info->flags |= DF_TEXTREL;
    }

  /* For a function, create a stub, if allowed.  */
  if (! hmips->no_fn_stub
      && (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) != 0)
    {
      if (! elf_hash_table (info)->dynamic_sections_created)
	return true;

      /* If this symbol is not defined in a regular file, then set
	 the symbol to the stub location.  This is required to make
	 function pointers compare as equal between the normal
	 executable and the shared library.  */
      if ((h->elf_link_hash_flags & ELF_LINK_HASH_DEF_REGULAR) == 0)
	{
	  /* We need .stub section.  */
	  s = bfd_get_section_by_name (dynobj,
				       MIPS_ELF_STUB_SECTION_NAME (dynobj));
	  BFD_ASSERT (s != NULL);

	  h->root.u.def.section = s;
	  h->root.u.def.value = s->_raw_size;

	  /* XXX Write this stub address somewhere.  */
	  h->plt.offset = s->_raw_size;

	  /* Make room for this stub code.  */
	  s->_raw_size += MIPS_FUNCTION_STUB_SIZE;

	  /* The last half word of the stub will be filled with the index
	     of this symbol in .dynsym section.  */
	  return true;
	}
    }
  else if ((h->type == STT_FUNC)
	   && (h->elf_link_hash_flags & ELF_LINK_HASH_NEEDS_PLT) == 0)
    {
      /* This will set the entry for this symbol in the GOT to 0, and
         the dynamic linker will take care of this.  */
      h->root.u.def.value = 0;
      return true;
    }

  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->weakdef != NULL)
    {
      BFD_ASSERT (h->weakdef->root.type == bfd_link_hash_defined
		  || h->weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->weakdef->root.u.def.section;
      h->root.u.def.value = h->weakdef->root.u.def.value;
      return true;
    }

  /* This is a reference to a symbol defined by a dynamic object which
     is not a function.  */

  return true;
}

/* This function is called after all the input files have been read,
   and the input sections have been assigned to output sections.  We
   check for any mips16 stub sections that we can discard.  */

static boolean mips_elf_check_mips16_stubs
  PARAMS ((struct mips_elf_link_hash_entry *, PTR));

boolean
_bfd_mips_elf_always_size_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  asection *ri;

  /* The .reginfo section has a fixed size.  */
  ri = bfd_get_section_by_name (output_bfd, ".reginfo");
  if (ri != NULL)
    bfd_set_section_size (output_bfd, ri,
			  (bfd_size_type) sizeof (Elf32_External_RegInfo));

  if (info->relocateable
      || ! mips_elf_hash_table (info)->mips16_stubs_seen)
    return true;

  mips_elf_link_hash_traverse (mips_elf_hash_table (info),
			       mips_elf_check_mips16_stubs,
			       (PTR) NULL);

  return true;
}

/* Check the mips16 stubs for a particular symbol, and see if we can
   discard them.  */

static boolean
mips_elf_check_mips16_stubs (h, data)
     struct mips_elf_link_hash_entry *h;
     PTR data ATTRIBUTE_UNUSED;
{
  if (h->root.root.type == bfd_link_hash_warning)
    h = (struct mips_elf_link_hash_entry *) h->root.root.u.i.link;

  if (h->fn_stub != NULL
      && ! h->need_fn_stub)
    {
      /* We don't need the fn_stub; the only references to this symbol
         are 16 bit calls.  Clobber the size to 0 to prevent it from
         being included in the link.  */
      h->fn_stub->_raw_size = 0;
      h->fn_stub->_cooked_size = 0;
      h->fn_stub->flags &= ~SEC_RELOC;
      h->fn_stub->reloc_count = 0;
      h->fn_stub->flags |= SEC_EXCLUDE;
    }

  if (h->call_stub != NULL
      && h->root.other == STO_MIPS16)
    {
      /* We don't need the call_stub; this is a 16 bit function, so
         calls from other 16 bit functions are OK.  Clobber the size
         to 0 to prevent it from being included in the link.  */
      h->call_stub->_raw_size = 0;
      h->call_stub->_cooked_size = 0;
      h->call_stub->flags &= ~SEC_RELOC;
      h->call_stub->reloc_count = 0;
      h->call_stub->flags |= SEC_EXCLUDE;
    }

  if (h->call_fp_stub != NULL
      && h->root.other == STO_MIPS16)
    {
      /* We don't need the call_stub; this is a 16 bit function, so
         calls from other 16 bit functions are OK.  Clobber the size
         to 0 to prevent it from being included in the link.  */
      h->call_fp_stub->_raw_size = 0;
      h->call_fp_stub->_cooked_size = 0;
      h->call_fp_stub->flags &= ~SEC_RELOC;
      h->call_fp_stub->reloc_count = 0;
      h->call_fp_stub->flags |= SEC_EXCLUDE;
    }

  return true;
}

/* Set the sizes of the dynamic sections.  */

boolean
_bfd_mips_elf_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean reltext;
  struct mips_got_info *g = NULL;

  dynobj = elf_hash_table (info)->dynobj;
  BFD_ASSERT (dynobj != NULL);

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size
	    = strlen (ELF_DYNAMIC_INTERPRETER (output_bfd)) + 1;
	  s->contents
	    = (bfd_byte *) ELF_DYNAMIC_INTERPRETER (output_bfd);
	}
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  reltext = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      strip = false;

      if (strncmp (name, ".rel", 4) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* We only strip the section if the output section name
                 has the same name.  Otherwise, there might be several
                 input sections for this output section.  FIXME: This
                 code is probably not needed these days anyhow, since
                 the linker now does not create empty output sections.  */
	      if (s->output_section != NULL
		  && strcmp (name,
			     bfd_get_section_name (s->output_section->owner,
						   s->output_section)) == 0)
		strip = true;
	    }
	  else
	    {
	      const char *outname;
	      asection *target;

	      /* If this relocation section applies to a read only
                 section, then we probably need a DT_TEXTREL entry.
                 If the relocation section is .rel.dyn, we always
                 assert a DT_TEXTREL entry rather than testing whether
                 there exists a relocation to a read only section or
                 not.  */
	      outname = bfd_get_section_name (output_bfd,
					      s->output_section);
	      target = bfd_get_section_by_name (output_bfd, outname + 4);
	      if ((target != NULL
		   && (target->flags & SEC_READONLY) != 0
		   && (target->flags & SEC_ALLOC) != 0)
		  || strcmp (outname,
			     MIPS_ELF_REL_DYN_SECTION_NAME (output_bfd)) == 0)
		reltext = true;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      if (strcmp (name,
			  MIPS_ELF_REL_DYN_SECTION_NAME (output_bfd)) != 0)
		s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) == 0)
	{
	  int i;
	  bfd_size_type loadable_size = 0;
	  bfd_size_type local_gotno;
	  bfd *sub;

	  BFD_ASSERT (elf_section_data (s) != NULL);
	  g = (struct mips_got_info *) elf_section_data (s)->tdata;
	  BFD_ASSERT (g != NULL);

	  /* Calculate the total loadable size of the output.  That
	     will give us the maximum number of GOT_PAGE entries
	     required.  */
	  for (sub = info->input_bfds; sub; sub = sub->link_next)
	    {
	      asection *subsection;

	      for (subsection = sub->sections;
		   subsection;
		   subsection = subsection->next)
		{
		  if ((subsection->flags & SEC_ALLOC) == 0)
		    continue;
		  loadable_size += ((subsection->_raw_size + 0xf)
				    &~ (bfd_size_type) 0xf);
		}
	    }
	  loadable_size += MIPS_FUNCTION_STUB_SIZE;

	  /* Assume there are two loadable segments consisting of
	     contiguous sections.  Is 5 enough?  */
	  local_gotno = (loadable_size >> 16) + 5;
	  if (IRIX_COMPAT (output_bfd) == ict_irix6)
	    /* It's possible we will need GOT_PAGE entries as well as
	       GOT16 entries.  Often, these will be able to share GOT
	       entries, but not always.  */
	    local_gotno *= 2;

	  g->local_gotno += local_gotno;
	  s->_raw_size += local_gotno * MIPS_ELF_GOT_SIZE (dynobj);

	  /* There has to be a global GOT entry for every symbol with
	     a dynamic symbol table index of DT_MIPS_GOTSYM or
	     higher.  Therefore, it make sense to put those symbols
	     that need GOT entries at the end of the symbol table.  We
	     do that here.  */
 	  if (!mips_elf_sort_hash_table (info, 1))
 	    return false;

	  if (g->global_gotsym != NULL)
	    i = elf_hash_table (info)->dynsymcount - g->global_gotsym->dynindx;
	  else
	    /* If there are no global symbols, or none requiring
	       relocations, then GLOBAL_GOTSYM will be NULL.  */
	    i = 0;
	  g->global_gotno = i;
	  s->_raw_size += i * MIPS_ELF_GOT_SIZE (dynobj);
	}
      else if (strcmp (name, MIPS_ELF_STUB_SECTION_NAME (output_bfd)) == 0)
	{
	  /* Irix rld assumes that the function stub isn't at the end
	     of .text section. So put a dummy. XXX  */
	  s->_raw_size += MIPS_FUNCTION_STUB_SIZE;
	}
      else if (! info->shared
	       && ! mips_elf_hash_table (info)->use_rld_obj_head
	       && strncmp (name, ".rld_map", 8) == 0)
	{
	  /* We add a room for __rld_map. It will be filled in by the
	     rtld to contain a pointer to the _r_debug structure.  */
	  s->_raw_size += 4;
	}
      else if (SGI_COMPAT (output_bfd)
	       && strncmp (name, ".compact_rel", 12) == 0)
	s->_raw_size += mips_elf_hash_table (info)->compact_rel_size;
      else if (strcmp (name, MIPS_ELF_MSYM_SECTION_NAME (output_bfd))
	       == 0)
	s->_raw_size = (sizeof (Elf32_External_Msym)
			* (elf_hash_table (info)->dynsymcount
			   + bfd_count_sections (output_bfd)));
      else if (strncmp (name, ".init", 5) != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip)
	{
	  _bfd_strip_section_from_output (info, s);
	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return false;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_mips_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  /* SGI object has the equivalence of DT_DEBUG in the
	     DT_MIPS_RLD_MAP entry.  */
	  if (!MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_RLD_MAP, 0))
	    return false;
	  if (!SGI_COMPAT (output_bfd))
	    {
	      if (!MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_DEBUG, 0))
		return false;
	    }
	}
      else
	{
	  /* Shared libraries on traditional mips have DT_DEBUG.  */
	  if (!SGI_COMPAT (output_bfd))
	    {
	      if (!MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_DEBUG, 0))
		return false;
	    }
	}

      if (reltext && SGI_COMPAT (output_bfd))
	info->flags |= DF_TEXTREL;

      if ((info->flags & DF_TEXTREL) != 0)
	{
	  if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_TEXTREL, 0))
	    return false;
	}

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_PLTGOT, 0))
	return false;

      if (bfd_get_section_by_name (dynobj,
				   MIPS_ELF_REL_DYN_SECTION_NAME (dynobj)))
	{
	  if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_REL, 0))
	    return false;

	  if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_RELSZ, 0))
	    return false;

	  if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_RELENT, 0))
	    return false;
	}

      if (SGI_COMPAT (output_bfd))
	{
	  if (!MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_CONFLICTNO, 0))
	    return false;
	}

      if (SGI_COMPAT (output_bfd))
	{
	  if (!MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_LIBLISTNO, 0))
	    return false;
	}

      if (bfd_get_section_by_name (dynobj, ".conflict") != NULL)
	{
	  if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_CONFLICT, 0))
	    return false;

	  s = bfd_get_section_by_name (dynobj, ".liblist");
	  BFD_ASSERT (s != NULL);

	  if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_LIBLIST, 0))
	    return false;
	}

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_RLD_VERSION, 0))
	return false;

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_FLAGS, 0))
	return false;

#if 0
      /* Time stamps in executable files are a bad idea.  */
      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_TIME_STAMP, 0))
	return false;
#endif

#if 0 /* FIXME  */
      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_ICHECKSUM, 0))
	return false;
#endif

#if 0 /* FIXME  */
      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_IVERSION, 0))
	return false;
#endif

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_BASE_ADDRESS, 0))
	return false;

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_LOCAL_GOTNO, 0))
	return false;

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_SYMTABNO, 0))
	return false;

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_UNREFEXTNO, 0))
	return false;

      if (! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_GOTSYM, 0))
	return false;

      if (IRIX_COMPAT (dynobj) == ict_irix5
	  && ! MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_HIPAGENO, 0))
	return false;

      if (IRIX_COMPAT (dynobj) == ict_irix6
	  && (bfd_get_section_by_name
	      (dynobj, MIPS_ELF_OPTIONS_SECTION_NAME (dynobj)))
	  && !MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_OPTIONS, 0))
	return false;

      if (bfd_get_section_by_name (dynobj,
				   MIPS_ELF_MSYM_SECTION_NAME (dynobj))
	  && !MIPS_ELF_ADD_DYNAMIC_ENTRY (info, DT_MIPS_MSYM, 0))
	return false;
    }

  return true;
}

/* If NAME is one of the special IRIX6 symbols defined by the linker,
   adjust it appropriately now.  */

static void
mips_elf_irix6_finish_dynamic_symbol (abfd, name, sym)
     bfd *abfd ATTRIBUTE_UNUSED;
     const char *name;
     Elf_Internal_Sym *sym;
{
  /* The linker script takes care of providing names and values for
     these, but we must place them into the right sections.  */
  static const char* const text_section_symbols[] = {
    "_ftext",
    "_etext",
    "__dso_displacement",
    "__elf_header",
    "__program_header_table",
    NULL
  };

  static const char* const data_section_symbols[] = {
    "_fdata",
    "_edata",
    "_end",
    "_fbss",
    NULL
  };

  const char* const *p;
  int i;

  for (i = 0; i < 2; ++i)
    for (p = (i == 0) ? text_section_symbols : data_section_symbols;
	 *p;
	 ++p)
      if (strcmp (*p, name) == 0)
	{
	  /* All of these symbols are given type STT_SECTION by the
	     IRIX6 linker.  */
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);

	  /* The IRIX linker puts these symbols in special sections.  */
	  if (i == 0)
	    sym->st_shndx = SHN_MIPS_TEXT;
	  else
	    sym->st_shndx = SHN_MIPS_DATA;

	  break;
	}
}

/* Finish up dynamic symbol handling.  We set the contents of various
   dynamic sections here.  */

boolean
_bfd_mips_elf_finish_dynamic_symbol (output_bfd, info, h, sym)
     bfd *output_bfd;
     struct bfd_link_info *info;
     struct elf_link_hash_entry *h;
     Elf_Internal_Sym *sym;
{
  bfd *dynobj;
  bfd_vma gval;
  asection *sgot;
  asection *smsym;
  struct mips_got_info *g;
  const char *name;
  struct mips_elf_link_hash_entry *mh;

  dynobj = elf_hash_table (info)->dynobj;
  gval = sym->st_value;
  mh = (struct mips_elf_link_hash_entry *) h;

  if (h->plt.offset != (bfd_vma) -1)
    {
      asection *s;
      bfd_byte *p;
      bfd_byte stub[MIPS_FUNCTION_STUB_SIZE];

      /* This symbol has a stub.  Set it up.  */

      BFD_ASSERT (h->dynindx != -1);

      s = bfd_get_section_by_name (dynobj,
				   MIPS_ELF_STUB_SECTION_NAME (dynobj));
      BFD_ASSERT (s != NULL);

      /* Fill the stub.  */
      p = stub;
      bfd_put_32 (output_bfd, (bfd_vma) STUB_LW (output_bfd), p);
      p += 4;
      bfd_put_32 (output_bfd, (bfd_vma) STUB_MOVE (output_bfd), p);
      p += 4;

      /* FIXME: Can h->dynindex be more than 64K?  */
      if (h->dynindx & 0xffff0000)
	return false;

      bfd_put_32 (output_bfd, (bfd_vma) STUB_JALR, p);
      p += 4;
      bfd_put_32 (output_bfd, (bfd_vma) STUB_LI16 (output_bfd) + h->dynindx, p);

      BFD_ASSERT (h->plt.offset <= s->_raw_size);
      memcpy (s->contents + h->plt.offset, stub, MIPS_FUNCTION_STUB_SIZE);

      /* Mark the symbol as undefined.  plt.offset != -1 occurs
	 only for the referenced symbol.  */
      sym->st_shndx = SHN_UNDEF;

      /* The run-time linker uses the st_value field of the symbol
	 to reset the global offset table entry for this external
	 to its stub address when unlinking a shared object.  */
      gval = s->output_section->vma + s->output_offset + h->plt.offset;
      sym->st_value = gval;
    }

  BFD_ASSERT (h->dynindx != -1
	      || (h->elf_link_hash_flags & ELF_LINK_FORCED_LOCAL) != 0);

  sgot = mips_elf_got_section (dynobj);
  BFD_ASSERT (sgot != NULL);
  BFD_ASSERT (elf_section_data (sgot) != NULL);
  g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
  BFD_ASSERT (g != NULL);

  /* Run through the global symbol table, creating GOT entries for all
     the symbols that need them.  */
  if (g->global_gotsym != NULL
      && h->dynindx >= g->global_gotsym->dynindx)
    {
      bfd_vma offset;
      bfd_vma value;

      if (sym->st_value)
	value = sym->st_value;
      else
	{
	  /* For an entity defined in a shared object, this will be
	     NULL.  (For functions in shared objects for
	     which we have created stubs, ST_VALUE will be non-NULL.
	     That's because such the functions are now no longer defined
	     in a shared object.)  */

	  if (info->shared && h->root.type == bfd_link_hash_undefined)
	    value = 0;
	  else
	    value = h->root.u.def.value;
	}
      offset = mips_elf_global_got_index (dynobj, h);
      MIPS_ELF_PUT_WORD (output_bfd, value, sgot->contents + offset);
    }

  /* Create a .msym entry, if appropriate.  */
  smsym = bfd_get_section_by_name (dynobj,
				   MIPS_ELF_MSYM_SECTION_NAME (dynobj));
  if (smsym)
    {
      Elf32_Internal_Msym msym;

      msym.ms_hash_value = bfd_elf_hash (h->root.root.string);
      /* It is undocumented what the `1' indicates, but IRIX6 uses
	 this value.  */
      msym.ms_info = ELF32_MS_INFO (mh->min_dyn_reloc_index, 1);
      bfd_mips_elf_swap_msym_out
	(dynobj, &msym,
	 ((Elf32_External_Msym *) smsym->contents) + h->dynindx);
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  name = h->root.root.string;
  if (strcmp (name, "_DYNAMIC") == 0
      || strcmp (name, "_GLOBAL_OFFSET_TABLE_") == 0)
    sym->st_shndx = SHN_ABS;
  else if (strcmp (name, "_DYNAMIC_LINK") == 0
	   || strcmp (name, "_DYNAMIC_LINKING") == 0)
    {
      sym->st_shndx = SHN_ABS;
      sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
      sym->st_value = 1;
    }
  else if (strcmp (name, "_gp_disp") == 0)
    {
      sym->st_shndx = SHN_ABS;
      sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
      sym->st_value = elf_gp (output_bfd);
    }
  else if (SGI_COMPAT (output_bfd))
    {
      if (strcmp (name, mips_elf_dynsym_rtproc_names[0]) == 0
	  || strcmp (name, mips_elf_dynsym_rtproc_names[1]) == 0)
	{
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
	  sym->st_other = STO_PROTECTED;
	  sym->st_value = 0;
	  sym->st_shndx = SHN_MIPS_DATA;
	}
      else if (strcmp (name, mips_elf_dynsym_rtproc_names[2]) == 0)
	{
	  sym->st_info = ELF_ST_INFO (STB_GLOBAL, STT_SECTION);
	  sym->st_other = STO_PROTECTED;
	  sym->st_value = mips_elf_hash_table (info)->procedure_count;
	  sym->st_shndx = SHN_ABS;
	}
      else if (sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS)
	{
	  if (h->type == STT_FUNC)
	    sym->st_shndx = SHN_MIPS_TEXT;
	  else if (h->type == STT_OBJECT)
	    sym->st_shndx = SHN_MIPS_DATA;
	}
    }

  /* Handle the IRIX6-specific symbols.  */
  if (IRIX_COMPAT (output_bfd) == ict_irix6)
    mips_elf_irix6_finish_dynamic_symbol (output_bfd, name, sym);

  if (! info->shared)
    {
      if (! mips_elf_hash_table (info)->use_rld_obj_head
	  && (strcmp (name, "__rld_map") == 0
	      || strcmp (name, "__RLD_MAP") == 0))
	{
	  asection *s = bfd_get_section_by_name (dynobj, ".rld_map");
	  BFD_ASSERT (s != NULL);
	  sym->st_value = s->output_section->vma + s->output_offset;
	  bfd_put_32 (output_bfd, (bfd_vma) 0, s->contents);
	  if (mips_elf_hash_table (info)->rld_value == 0)
	    mips_elf_hash_table (info)->rld_value = sym->st_value;
	}
      else if (mips_elf_hash_table (info)->use_rld_obj_head
	       && strcmp (name, "__rld_obj_head") == 0)
	{
	  /* IRIX6 does not use a .rld_map section.  */
	  if (IRIX_COMPAT (output_bfd) == ict_irix5
              || IRIX_COMPAT (output_bfd) == ict_none)
	    BFD_ASSERT (bfd_get_section_by_name (dynobj, ".rld_map")
			!= NULL);
	  mips_elf_hash_table (info)->rld_value = sym->st_value;
	}
    }

  /* If this is a mips16 symbol, force the value to be even.  */
  if (sym->st_other == STO_MIPS16
      && (sym->st_value & 1) != 0)
    --sym->st_value;

  return true;
}

/* Finish up the dynamic sections.  */

boolean
_bfd_mips_elf_finish_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *sdyn;
  asection *sgot;
  struct mips_got_info *g;

  dynobj = elf_hash_table (info)->dynobj;

  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");

  sgot = mips_elf_got_section (dynobj);
  if (sgot == NULL)
    g = NULL;
  else
    {
      BFD_ASSERT (elf_section_data (sgot) != NULL);
      g = (struct mips_got_info *) elf_section_data (sgot)->tdata;
      BFD_ASSERT (g != NULL);
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      bfd_byte *b;

      BFD_ASSERT (sdyn != NULL);
      BFD_ASSERT (g != NULL);

      for (b = sdyn->contents;
	   b < sdyn->contents + sdyn->_raw_size;
	   b += MIPS_ELF_DYN_SIZE (dynobj))
	{
	  Elf_Internal_Dyn dyn;
	  const char *name;
	  size_t elemsize;
	  asection *s;
	  boolean swap_out_p;

	  /* Read in the current dynamic entry.  */
	  (*get_elf_backend_data (dynobj)->s->swap_dyn_in) (dynobj, b, &dyn);

	  /* Assume that we're going to modify it and write it out.  */
	  swap_out_p = true;

	  switch (dyn.d_tag)
	    {
	    case DT_RELENT:
	      s = (bfd_get_section_by_name
		   (dynobj,
		    MIPS_ELF_REL_DYN_SECTION_NAME (dynobj)));
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_val = MIPS_ELF_REL_SIZE (dynobj);
	      break;

	    case DT_STRSZ:
	      /* Rewrite DT_STRSZ.  */
	      dyn.d_un.d_val =
		_bfd_elf_strtab_size (elf_hash_table (info)->dynstr);
	      break;

	    case DT_PLTGOT:
	      name = ".got";
	      goto get_vma;
	    case DT_MIPS_CONFLICT:
	      name = ".conflict";
	      goto get_vma;
	    case DT_MIPS_LIBLIST:
	      name = ".liblist";
	    get_vma:
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma;
	      break;

	    case DT_MIPS_RLD_VERSION:
	      dyn.d_un.d_val = 1; /* XXX */
	      break;

	    case DT_MIPS_FLAGS:
	      dyn.d_un.d_val = RHF_NOTPOT; /* XXX */
	      break;

	    case DT_MIPS_CONFLICTNO:
	      name = ".conflict";
	      elemsize = sizeof (Elf32_Conflict);
	      goto set_elemno;

	    case DT_MIPS_LIBLISTNO:
	      name = ".liblist";
	      elemsize = sizeof (Elf32_Lib);
	    set_elemno:
	      s = bfd_get_section_by_name (output_bfd, name);
	      if (s != NULL)
		{
		  if (s->_cooked_size != 0)
		    dyn.d_un.d_val = s->_cooked_size / elemsize;
		  else
		    dyn.d_un.d_val = s->_raw_size / elemsize;
		}
	      else
		dyn.d_un.d_val = 0;
	      break;

	    case DT_MIPS_TIME_STAMP:
	      time ((time_t *) &dyn.d_un.d_val);
	      break;

	    case DT_MIPS_ICHECKSUM:
	      /* XXX FIXME: */
	      swap_out_p = false;
	      break;

	    case DT_MIPS_IVERSION:
	      /* XXX FIXME: */
	      swap_out_p = false;
	      break;

	    case DT_MIPS_BASE_ADDRESS:
	      s = output_bfd->sections;
	      BFD_ASSERT (s != NULL);
	      dyn.d_un.d_ptr = s->vma & ~(bfd_vma) 0xffff;
	      break;

	    case DT_MIPS_LOCAL_GOTNO:
	      dyn.d_un.d_val = g->local_gotno;
	      break;

	    case DT_MIPS_UNREFEXTNO:
	      /* The index into the dynamic symbol table which is the
		 entry of the first external symbol that is not
		 referenced within the same object.  */
	      dyn.d_un.d_val = bfd_count_sections (output_bfd) + 1;
	      break;

	    case DT_MIPS_GOTSYM:
	      if (g->global_gotsym)
		{
		  dyn.d_un.d_val = g->global_gotsym->dynindx;
		  break;
		}
	      /* In case if we don't have global got symbols we default
		 to setting DT_MIPS_GOTSYM to the same value as
		 DT_MIPS_SYMTABNO, so we just fall through.  */

	    case DT_MIPS_SYMTABNO:
	      name = ".dynsym";
	      elemsize = MIPS_ELF_SYM_SIZE (output_bfd);
	      s = bfd_get_section_by_name (output_bfd, name);
	      BFD_ASSERT (s != NULL);

	      if (s->_cooked_size != 0)
		dyn.d_un.d_val = s->_cooked_size / elemsize;
	      else
		dyn.d_un.d_val = s->_raw_size / elemsize;
	      break;

	    case DT_MIPS_HIPAGENO:
	      dyn.d_un.d_val = g->local_gotno - MIPS_RESERVED_GOTNO;
	      break;

	    case DT_MIPS_RLD_MAP:
	      dyn.d_un.d_ptr = mips_elf_hash_table (info)->rld_value;
	      break;

	    case DT_MIPS_OPTIONS:
	      s = (bfd_get_section_by_name
		   (output_bfd, MIPS_ELF_OPTIONS_SECTION_NAME (output_bfd)));
	      dyn.d_un.d_ptr = s->vma;
	      break;

	    case DT_MIPS_MSYM:
	      s = (bfd_get_section_by_name
		   (output_bfd, MIPS_ELF_MSYM_SECTION_NAME (output_bfd)));
	      dyn.d_un.d_ptr = s->vma;
	      break;

	    default:
	      swap_out_p = false;
	      break;
	    }

	  if (swap_out_p)
	    (*get_elf_backend_data (dynobj)->s->swap_dyn_out)
	      (dynobj, &dyn, b);
	}
    }

  /* The first entry of the global offset table will be filled at
     runtime. The second entry will be used by some runtime loaders.
     This isn't the case of Irix rld.  */
  if (sgot != NULL && sgot->_raw_size > 0)
    {
      MIPS_ELF_PUT_WORD (output_bfd, (bfd_vma) 0, sgot->contents);
      MIPS_ELF_PUT_WORD (output_bfd, (bfd_vma) 0x80000000,
			 sgot->contents + MIPS_ELF_GOT_SIZE (output_bfd));
    }

  if (sgot != NULL)
    elf_section_data (sgot->output_section)->this_hdr.sh_entsize
      = MIPS_ELF_GOT_SIZE (output_bfd);

  {
    asection *smsym;
    asection *s;
    Elf32_compact_rel cpt;

    /* ??? The section symbols for the output sections were set up in
       _bfd_elf_final_link.  SGI sets the STT_NOTYPE attribute for these
       symbols.  Should we do so?  */

    smsym = bfd_get_section_by_name (dynobj,
				     MIPS_ELF_MSYM_SECTION_NAME (dynobj));
    if (smsym != NULL)
      {
	Elf32_Internal_Msym msym;

	msym.ms_hash_value = 0;
	msym.ms_info = ELF32_MS_INFO (0, 1);

	for (s = output_bfd->sections; s != NULL; s = s->next)
	  {
	    long dynindx = elf_section_data (s)->dynindx;

	    bfd_mips_elf_swap_msym_out
	      (output_bfd, &msym,
	       (((Elf32_External_Msym *) smsym->contents)
		+ dynindx));
	  }
      }

    if (SGI_COMPAT (output_bfd))
      {
	/* Write .compact_rel section out.  */
	s = bfd_get_section_by_name (dynobj, ".compact_rel");
	if (s != NULL)
	  {
	    cpt.id1 = 1;
	    cpt.num = s->reloc_count;
	    cpt.id2 = 2;
	    cpt.offset = (s->output_section->filepos
			  + sizeof (Elf32_External_compact_rel));
	    cpt.reserved0 = 0;
	    cpt.reserved1 = 0;
	    bfd_elf32_swap_compact_rel_out (output_bfd, &cpt,
					    ((Elf32_External_compact_rel *)
					     s->contents));

	    /* Clean up a dummy stub function entry in .text.  */
	    s = bfd_get_section_by_name (dynobj,
					 MIPS_ELF_STUB_SECTION_NAME (dynobj));
	    if (s != NULL)
	      {
		file_ptr dummy_offset;

		BFD_ASSERT (s->_raw_size >= MIPS_FUNCTION_STUB_SIZE);
		dummy_offset = s->_raw_size - MIPS_FUNCTION_STUB_SIZE;
		memset (s->contents + dummy_offset, 0,
			MIPS_FUNCTION_STUB_SIZE);
	      }
	  }
      }

    /* We need to sort the entries of the dynamic relocation section.  */

    if (!ABI_64_P (output_bfd))
      {
	asection *reldyn;

	reldyn = bfd_get_section_by_name (dynobj,
					  MIPS_ELF_REL_DYN_SECTION_NAME (dynobj));
	if (reldyn != NULL && reldyn->reloc_count > 2)
	  {
	    reldyn_sorting_bfd = output_bfd;
	    qsort ((Elf32_External_Rel *) reldyn->contents + 1,
		   (size_t) reldyn->reloc_count - 1,
		   sizeof (Elf32_External_Rel), sort_dynamic_relocs);
	  }
      }

    /* Clean up a first relocation in .rel.dyn.  */
    s = bfd_get_section_by_name (dynobj,
				 MIPS_ELF_REL_DYN_SECTION_NAME (dynobj));
    if (s != NULL && s->_raw_size > 0)
      memset (s->contents, 0, MIPS_ELF_REL_SIZE (dynobj));
  }

  return true;
}

/* Support for core dump NOTE sections */
static boolean
_bfd_elf32_mips_grok_prstatus (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  int offset;
  unsigned int raw_size;

  switch (note->descsz)
    {
      default:
	return false;

      case 256:		/* Linux/MIPS */
	/* pr_cursig */
	elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

	/* pr_pid */
	elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

	/* pr_reg */
	offset = 72;
	raw_size = 180;

	break;
    }

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  raw_size, note->descpos + offset);
}

static boolean
_bfd_elf32_mips_grok_psinfo (abfd, note)
     bfd *abfd;
     Elf_Internal_Note *note;
{
  switch (note->descsz)
    {
      default:
	return false;

      case 128:		/* Linux/MIPS elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 32, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 48, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return true;
}

#define PDR_SIZE 32

static boolean
_bfd_elf32_mips_discard_info (abfd, cookie, info)
     bfd *abfd;
     struct elf_reloc_cookie *cookie;
     struct bfd_link_info *info;
{
  asection *o;
  struct elf_backend_data *bed = get_elf_backend_data (abfd);
  boolean ret = false;
  unsigned char *tdata;
  size_t i, skip;

  o = bfd_get_section_by_name (abfd, ".pdr");
  if (! o)
    return false;
  if (o->_raw_size == 0)
    return false;
  if (o->_raw_size % PDR_SIZE != 0)
    return false;
  if (o->output_section != NULL
      && bfd_is_abs_section (o->output_section))
    return false;

  tdata = bfd_zmalloc (o->_raw_size / PDR_SIZE);
  if (! tdata)
    return false;

  cookie->rels = _bfd_elf32_link_read_relocs (abfd, o, (PTR) NULL,
					     (Elf_Internal_Rela *) NULL,
					      info->keep_memory);
  if (!cookie->rels)
    {
      free (tdata);
      return false;
    }

  cookie->rel = cookie->rels;
  cookie->relend =
    cookie->rels + o->reloc_count * bed->s->int_rels_per_ext_rel;

  for (i = 0, skip = 0; i < o->_raw_size; i ++)
    {
      if (_bfd_elf32_reloc_symbol_deleted_p (i * PDR_SIZE, cookie))
	{
	  tdata[i] = 1;
	  skip ++;
	}
    }

  if (skip != 0)
    {
      elf_section_data (o)->tdata = tdata;
      o->_cooked_size = o->_raw_size - skip * PDR_SIZE;
      ret = true;
    }
  else
    free (tdata);

  if (! info->keep_memory)
    free (cookie->rels);

  return ret;
}

static boolean
_bfd_elf32_mips_ignore_discarded_relocs (sec)
     asection *sec;
{
  if (strcmp (sec->name, ".pdr") == 0)
    return true;
  return false;
}

static boolean
_bfd_elf32_mips_write_section (output_bfd, sec, contents)
     bfd *output_bfd;
     asection *sec;
     bfd_byte *contents;
{
  bfd_byte *to, *from, *end;
  int i;

  if (strcmp (sec->name, ".pdr") != 0)
    return false;

  if (elf_section_data (sec)->tdata == NULL)
    return false;

  to = contents;
  end = contents + sec->_raw_size;
  for (from = contents, i = 0;
       from < end;
       from += PDR_SIZE, i++)
    {
      if (((unsigned char *)elf_section_data (sec)->tdata)[i] == 1)
	continue;
      if (to != from)
	memcpy (to, from, PDR_SIZE);
      to += PDR_SIZE;
    }
  bfd_set_section_contents (output_bfd, sec->output_section, contents,
			    (file_ptr) sec->output_offset,
			    sec->_cooked_size);
  return true;
}

/* Given a data section and an in-memory embedded reloc section, store
   relocation information into the embedded reloc section which can be
   used at runtime to relocate the data section.  This is called by the
   linker when the --embedded-relocs switch is used.  This is called
   after the add_symbols entry point has been called for all the
   objects, and before the final_link entry point is called.  */

boolean
bfd_mips_elf32_create_embedded_relocs (abfd, info, datasec, relsec, errmsg)
     bfd *abfd;
     struct bfd_link_info *info;
     asection *datasec;
     asection *relsec;
     char **errmsg;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Shdr *shndx_hdr;
  Elf32_External_Sym *extsyms;
  Elf32_External_Sym *free_extsyms = NULL;
  Elf_External_Sym_Shndx *shndx_buf = NULL;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *free_relocs = NULL;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *p;
  bfd_size_type amt;

  BFD_ASSERT (! info->relocateable);

  *errmsg = NULL;

  if (datasec->reloc_count == 0)
    return true;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  /* Read this BFD's symbols if we haven't done so already, or get the cached
     copy if it exists.  */
  if (symtab_hdr->contents != NULL)
    extsyms = (Elf32_External_Sym *) symtab_hdr->contents;
  else
    {
      /* Go get them off disk.  */
      if (info->keep_memory)
	extsyms = ((Elf32_External_Sym *)
		   bfd_alloc (abfd, symtab_hdr->sh_size));
      else
	extsyms = ((Elf32_External_Sym *)
		   bfd_malloc (symtab_hdr->sh_size));
      if (extsyms == NULL)
	goto error_return;
      if (! info->keep_memory)
	free_extsyms = extsyms;
      if (bfd_seek (abfd, symtab_hdr->sh_offset, SEEK_SET) != 0
	  || (bfd_bread (extsyms, symtab_hdr->sh_size, abfd)
	      != symtab_hdr->sh_size))
	goto error_return;
      if (info->keep_memory)
	symtab_hdr->contents = (unsigned char *) extsyms;
    }

  shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;
  if (shndx_hdr->sh_size != 0)
    {
      amt = symtab_hdr->sh_info * sizeof (Elf_External_Sym_Shndx);
      shndx_buf = (Elf_External_Sym_Shndx *) bfd_malloc (amt);
      if (shndx_buf == NULL)
	goto error_return;
      if (bfd_seek (abfd, shndx_hdr->sh_offset, SEEK_SET) != 0
	  || bfd_bread ((PTR) shndx_buf, amt, abfd) != amt)
	goto error_return;
    }

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf32_link_read_relocs
		     (abfd, datasec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;
  if (! info->keep_memory)
    free_relocs = internal_relocs;

  relsec->contents = (bfd_byte *) bfd_alloc (abfd, datasec->reloc_count * 12);
  if (relsec->contents == NULL)
    goto error_return;

  p = relsec->contents;

  irelend = internal_relocs + datasec->reloc_count;

  for (irel = internal_relocs; irel < irelend; irel++, p += 12)
    {
      asection *targetsec;

      /* We are going to write a four byte longword into the runtime
       reloc section.  The longword will be the address in the data
       section which must be relocated.  It is followed by the name
       of the target section NUL-padded or truncated to 8
       characters.  */

      /* We can only relocate absolute longword relocs at run time.  */
      if ((ELF32_R_TYPE (irel->r_info) != (int) R_MIPS_32) &&
	  (ELF32_R_TYPE (irel->r_info) != (int) R_MIPS_64))
	{
	  *errmsg = _("unsupported reloc type");
	  bfd_set_error (bfd_error_bad_value);
	  goto error_return;
	}
      /* Get the target section referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
          Elf32_External_Sym *esym;
          Elf_External_Sym_Shndx *shndx;
          Elf_Internal_Sym isym;

          /* A local symbol.  */
          esym = extsyms + ELF32_R_SYM (irel->r_info);
          shndx = shndx_buf + (shndx_buf ? ELF32_R_SYM (irel->r_info) : 0);
	  bfd_elf32_swap_symbol_in (abfd, esym, shndx, &isym);

	  targetsec = bfd_section_from_elf_index (abfd, isym.st_shndx);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info);
	  h = elf_sym_hashes (abfd)[indx];
	  targetsec = NULL;
	  /*
	   * For some reason, in certain programs, the symbol will
	   * not be in the hash table.  It seems to happen when you
	   * declare a static table of pointers to const external structures.
	   * In this case, the relocs are relative to data, not
	   * text, so just treating it like an undefined link
	   * should be sufficient.
	   */
	  BFD_ASSERT(h != NULL);
	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    targetsec = h->root.u.def.section;
	}


      /*
       * Set the low bit of the relocation offset if it's a MIPS64 reloc.
       * Relocations will always be on (at least) 32-bit boundaries.
       */

      bfd_put_32 (abfd, ((irel->r_offset + datasec->output_offset) +
		  ((ELF32_R_TYPE (irel->r_info) == (int) R_MIPS_64) ? 1 : 0)),
		  p);
      memset (p + 4, 0, 8);
      if (targetsec != NULL)
	strncpy (p + 4, targetsec->output_section->name, 8);
    }

  if (shndx_buf != NULL)
    free (shndx_buf);
  if (free_extsyms != NULL)
    free (free_extsyms);
  if (free_relocs != NULL)
    free (free_relocs);
  return true;

 error_return:
  if (shndx_buf != NULL)
    free (shndx_buf);
  if (free_extsyms != NULL)
    free (free_extsyms);
  if (free_relocs != NULL)
    free (free_relocs);
  return false;
}

/* This is almost identical to bfd_generic_get_... except that some
   MIPS relocations need to be handled specially.  Sigh.  */

static bfd_byte *
elf32_mips_get_relocated_section_contents (abfd, link_info, link_order, data,
					   relocateable, symbols)
     bfd *abfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     boolean relocateable;
     asymbol **symbols;
{
  /* Get enough memory to hold the stuff */
  bfd *input_bfd = link_order->u.indirect.section->owner;
  asection *input_section = link_order->u.indirect.section;

  long reloc_size = bfd_get_reloc_upper_bound (input_bfd, input_section);
  arelent **reloc_vector = NULL;
  long reloc_count;

  if (reloc_size < 0)
    goto error_return;

  reloc_vector = (arelent **) bfd_malloc ((bfd_size_type) reloc_size);
  if (reloc_vector == NULL && reloc_size != 0)
    goto error_return;

  /* read in the section */
  if (!bfd_get_section_contents (input_bfd,
				 input_section,
				 (PTR) data,
				 (file_ptr) 0,
				 input_section->_raw_size))
    goto error_return;

  /* We're not relaxing the section, so just copy the size info */
  input_section->_cooked_size = input_section->_raw_size;
  input_section->reloc_done = true;

  reloc_count = bfd_canonicalize_reloc (input_bfd,
					input_section,
					reloc_vector,
					symbols);
  if (reloc_count < 0)
    goto error_return;

  if (reloc_count > 0)
    {
      arelent **parent;
      /* for mips */
      int gp_found;
      bfd_vma gp = 0x12345678;	/* initialize just to shut gcc up */

      {
	struct bfd_hash_entry *h;
	struct bfd_link_hash_entry *lh;
	/* Skip all this stuff if we aren't mixing formats.  */
	if (abfd && input_bfd
	    && abfd->xvec == input_bfd->xvec)
	  lh = 0;
	else
	  {
	    h = bfd_hash_lookup (&link_info->hash->table, "_gp", false, false);
	    lh = (struct bfd_link_hash_entry *) h;
	  }
      lookup:
	if (lh)
	  {
	    switch (lh->type)
	      {
	      case bfd_link_hash_undefined:
	      case bfd_link_hash_undefweak:
	      case bfd_link_hash_common:
		gp_found = 0;
		break;
	      case bfd_link_hash_defined:
	      case bfd_link_hash_defweak:
		gp_found = 1;
		gp = lh->u.def.value;
		break;
	      case bfd_link_hash_indirect:
	      case bfd_link_hash_warning:
		lh = lh->u.i.link;
		/* @@FIXME  ignoring warning for now */
		goto lookup;
	      case bfd_link_hash_new:
	      default:
		abort ();
	      }
	  }
	else
	  gp_found = 0;
      }
      /* end mips */
      for (parent = reloc_vector; *parent != (arelent *) NULL;
	   parent++)
	{
	  char *error_message = (char *) NULL;
	  bfd_reloc_status_type r;

	  /* Specific to MIPS: Deal with relocation types that require
	     knowing the gp of the output bfd.  */
	  asymbol *sym = *(*parent)->sym_ptr_ptr;
	  if (bfd_is_abs_section (sym->section) && abfd)
	    {
	      /* The special_function wouldn't get called anyways.  */
	    }
	  else if (!gp_found)
	    {
	      /* The gp isn't there; let the special function code
		 fall over on its own.  */
	    }
	  else if ((*parent)->howto->special_function
		   == _bfd_mips_elf_gprel16_reloc)
	    {
	      /* bypass special_function call */
	      r = gprel16_with_gp (input_bfd, sym, *parent, input_section,
				   relocateable, (PTR) data, gp);
	      goto skip_bfd_perform_relocation;
	    }
	  /* end mips specific stuff */

	  r = bfd_perform_relocation (input_bfd,
				      *parent,
				      (PTR) data,
				      input_section,
				      relocateable ? abfd : (bfd *) NULL,
				      &error_message);
	skip_bfd_perform_relocation:

	  if (relocateable)
	    {
	      asection *os = input_section->output_section;

	      /* A partial link, so keep the relocs */
	      os->orelocation[os->reloc_count] = *parent;
	      os->reloc_count++;
	    }

	  if (r != bfd_reloc_ok)
	    {
	      switch (r)
		{
		case bfd_reloc_undefined:
		  if (!((*link_info->callbacks->undefined_symbol)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 input_bfd, input_section, (*parent)->address,
			 true)))
		    goto error_return;
		  break;
		case bfd_reloc_dangerous:
		  BFD_ASSERT (error_message != (char *) NULL);
		  if (!((*link_info->callbacks->reloc_dangerous)
			(link_info, error_message, input_bfd, input_section,
			 (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_overflow:
		  if (!((*link_info->callbacks->reloc_overflow)
			(link_info, bfd_asymbol_name (*(*parent)->sym_ptr_ptr),
			 (*parent)->howto->name, (*parent)->addend,
			 input_bfd, input_section, (*parent)->address)))
		    goto error_return;
		  break;
		case bfd_reloc_outofrange:
		default:
		  abort ();
		  break;
		}

	    }
	}
    }
  if (reloc_vector != NULL)
    free (reloc_vector);
  return data;

error_return:
  if (reloc_vector != NULL)
    free (reloc_vector);
  return NULL;
}

#define bfd_elf32_bfd_get_relocated_section_contents \
  elf32_mips_get_relocated_section_contents

/* ECOFF swapping routines.  These are used when dealing with the
   .mdebug section, which is in the ECOFF debugging format.  */
static const struct ecoff_debug_swap mips_elf32_ecoff_debug_swap = {
  /* Symbol table magic number.  */
  magicSym,
  /* Alignment of debugging information.  E.g., 4.  */
  4,
  /* Sizes of external symbolic information.  */
  sizeof (struct hdr_ext),
  sizeof (struct dnr_ext),
  sizeof (struct pdr_ext),
  sizeof (struct sym_ext),
  sizeof (struct opt_ext),
  sizeof (struct fdr_ext),
  sizeof (struct rfd_ext),
  sizeof (struct ext_ext),
  /* Functions to swap in external symbolic data.  */
  ecoff_swap_hdr_in,
  ecoff_swap_dnr_in,
  ecoff_swap_pdr_in,
  ecoff_swap_sym_in,
  ecoff_swap_opt_in,
  ecoff_swap_fdr_in,
  ecoff_swap_rfd_in,
  ecoff_swap_ext_in,
  _bfd_ecoff_swap_tir_in,
  _bfd_ecoff_swap_rndx_in,
  /* Functions to swap out external symbolic data.  */
  ecoff_swap_hdr_out,
  ecoff_swap_dnr_out,
  ecoff_swap_pdr_out,
  ecoff_swap_sym_out,
  ecoff_swap_opt_out,
  ecoff_swap_fdr_out,
  ecoff_swap_rfd_out,
  ecoff_swap_ext_out,
  _bfd_ecoff_swap_tir_out,
  _bfd_ecoff_swap_rndx_out,
  /* Function to read in symbolic data.  */
  _bfd_mips_elf_read_ecoff_info
};

#define ELF_ARCH			bfd_arch_mips
#define ELF_MACHINE_CODE		EM_MIPS

/* The SVR4 MIPS ABI says that this should be 0x10000, but Irix 5 uses
   a value of 0x1000, and we are compatible.  */
#define ELF_MAXPAGESIZE			0x1000

#define elf_backend_collect		true
#define elf_backend_type_change_ok	true
#define elf_backend_can_gc_sections	true
#define elf_info_to_howto		mips_info_to_howto_rela
#define elf_info_to_howto_rel		mips_info_to_howto_rel
#define elf_backend_sym_is_global	mips_elf_sym_is_global
#define elf_backend_object_p		_bfd_mips_elf_object_p
#define elf_backend_symbol_processing	_bfd_mips_elf_symbol_processing
#define elf_backend_section_processing	_bfd_mips_elf_section_processing
#define elf_backend_section_from_shdr	_bfd_mips_elf_section_from_shdr
#define elf_backend_fake_sections	_bfd_mips_elf_fake_sections
#define elf_backend_section_from_bfd_section \
					_bfd_mips_elf_section_from_bfd_section
#define elf_backend_add_symbol_hook	_bfd_mips_elf_add_symbol_hook
#define elf_backend_link_output_symbol_hook \
					_bfd_mips_elf_link_output_symbol_hook
#define elf_backend_create_dynamic_sections \
					_bfd_mips_elf_create_dynamic_sections
#define elf_backend_check_relocs	_bfd_mips_elf_check_relocs
#define elf_backend_adjust_dynamic_symbol \
					_bfd_mips_elf_adjust_dynamic_symbol
#define elf_backend_always_size_sections \
					_bfd_mips_elf_always_size_sections
#define elf_backend_size_dynamic_sections \
					_bfd_mips_elf_size_dynamic_sections
#define elf_backend_relocate_section	_bfd_mips_elf_relocate_section
#define elf_backend_finish_dynamic_symbol \
					_bfd_mips_elf_finish_dynamic_symbol
#define elf_backend_finish_dynamic_sections \
					_bfd_mips_elf_finish_dynamic_sections
#define elf_backend_final_write_processing \
					_bfd_mips_elf_final_write_processing
#define elf_backend_additional_program_headers \
					_bfd_mips_elf_additional_program_headers
#define elf_backend_modify_segment_map	_bfd_mips_elf_modify_segment_map
#define elf_backend_gc_mark_hook	_bfd_mips_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook	_bfd_mips_elf_gc_sweep_hook
#define elf_backend_copy_indirect_symbol \
					_bfd_mips_elf_copy_indirect_symbol
#define elf_backend_hide_symbol		_bfd_mips_elf_hide_symbol
#define elf_backend_grok_prstatus	_bfd_elf32_mips_grok_prstatus
#define elf_backend_grok_psinfo		_bfd_elf32_mips_grok_psinfo
#define elf_backend_ecoff_debug_swap	&mips_elf32_ecoff_debug_swap

#define elf_backend_got_header_size	(4 * MIPS_RESERVED_GOTNO)
#define elf_backend_plt_header_size	0
#define elf_backend_may_use_rel_p	1
#define elf_backend_may_use_rela_p	0
#define elf_backend_default_use_rela_p	0
#define elf_backend_sign_extend_vma	true

#define elf_backend_discard_info	_bfd_elf32_mips_discard_info
#define elf_backend_ignore_discarded_relocs \
					_bfd_elf32_mips_ignore_discarded_relocs
#define elf_backend_write_section	_bfd_elf32_mips_write_section

#define bfd_elf32_bfd_is_local_label_name \
					mips_elf_is_local_label_name
#define bfd_elf32_find_nearest_line	_bfd_mips_elf_find_nearest_line
#define bfd_elf32_set_section_contents	_bfd_mips_elf_set_section_contents
#define bfd_elf32_bfd_link_hash_table_create \
					_bfd_mips_elf_link_hash_table_create
#define bfd_elf32_bfd_final_link	_bfd_mips_elf_final_link
#define bfd_elf32_bfd_merge_private_bfd_data \
					_bfd_mips_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags	_bfd_mips_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data \
					_bfd_mips_elf_print_private_bfd_data

/* Support for SGI-ish mips targets.  */
#define TARGET_LITTLE_SYM		bfd_elf32_littlemips_vec
#define TARGET_LITTLE_NAME		"elf32-littlemips"
#define TARGET_BIG_SYM			bfd_elf32_bigmips_vec
#define TARGET_BIG_NAME			"elf32-bigmips"

#include "elf32-target.h"

/* Support for traditional mips targets.  */
#define INCLUDED_TARGET_FILE            /* More a type of flag.  */

#undef TARGET_LITTLE_SYM
#undef TARGET_LITTLE_NAME
#undef TARGET_BIG_SYM
#undef TARGET_BIG_NAME

#define TARGET_LITTLE_SYM               bfd_elf32_tradlittlemips_vec
#define TARGET_LITTLE_NAME              "elf32-tradlittlemips"
#define TARGET_BIG_SYM                  bfd_elf32_tradbigmips_vec
#define TARGET_BIG_NAME                 "elf32-tradbigmips"

/* Include the target file again for this target */
#include "elf32-target.h"
