/* ELF executable support for BFD.
   Copyright 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001 Free Software Foundation, Inc.

   Written by Fred Fish @ Cygnus Support, from information published
   in "UNIX System V Release 4, Programmers Guide: ANSI C and
   Programming Support Tools".  Sufficient support for gdb.

   Rewritten by Mark Eichin @ Cygnus Support, from information
   published in "System V Application Binary Interface", chapters 4
   and 5, as well as the various "Processor Supplement" documents
   derived from it. Added support for assembler and other object file
   utilities.  Further work done by Ken Raeburn (Cygnus Support), Michael
   Meissner (Open Software Foundation), and Peter Hoogenboom (University
   of Utah) to finish and extend this.

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

/* Problems and other issues to resolve.

   (1)	BFD expects there to be some fixed number of "sections" in
        the object file.  I.E. there is a "section_count" variable in the
	bfd structure which contains the number of sections.  However, ELF
	supports multiple "views" of a file.  In particular, with current
	implementations, executable files typically have two tables, a
	program header table and a section header table, both of which
	partition the executable.

	In ELF-speak, the "linking view" of the file uses the section header
	table to access "sections" within the file, and the "execution view"
	uses the program header table to access "segments" within the file.
	"Segments" typically may contain all the data from one or more
	"sections".

	Note that the section header table is optional in ELF executables,
	but it is this information that is most useful to gdb.  If the
	section header table is missing, then gdb should probably try
	to make do with the program header table.  (FIXME)

   (2)  The code in this file is compiled twice, once in 32-bit mode and
	once in 64-bit mode.  More of it should be made size-independent
	and moved into elf.c.

   (3)	ELF section symbols are handled rather sloppily now.  This should
	be cleaned up, and ELF section symbols reconciled with BFD section
	symbols.

   (4)  We need a published spec for 64-bit ELF.  We've got some stuff here
	that we're using for SPARC V9 64-bit chips, but don't assume that
	it's cast in stone.
 */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"

/* Renaming structures, typedefs, macros and functions to be size-specific.  */
#define Elf_External_Ehdr	NAME(Elf,External_Ehdr)
#define Elf_External_Sym	NAME(Elf,External_Sym)
#define Elf_External_Shdr	NAME(Elf,External_Shdr)
#define Elf_External_Phdr	NAME(Elf,External_Phdr)
#define Elf_External_Rel	NAME(Elf,External_Rel)
#define Elf_External_Rela	NAME(Elf,External_Rela)
#define Elf_External_Dyn	NAME(Elf,External_Dyn)

#define elf_core_file_failing_command	NAME(bfd_elf,core_file_failing_command)
#define elf_core_file_failing_signal	NAME(bfd_elf,core_file_failing_signal)
#define elf_core_file_matches_executable_p \
  NAME(bfd_elf,core_file_matches_executable_p)
#define elf_object_p			NAME(bfd_elf,object_p)
#define elf_core_file_p			NAME(bfd_elf,core_file_p)
#define elf_get_symtab_upper_bound	NAME(bfd_elf,get_symtab_upper_bound)
#define elf_get_dynamic_symtab_upper_bound \
  NAME(bfd_elf,get_dynamic_symtab_upper_bound)
#define elf_swap_reloc_in		NAME(bfd_elf,swap_reloc_in)
#define elf_swap_reloca_in		NAME(bfd_elf,swap_reloca_in)
#define elf_swap_reloc_out		NAME(bfd_elf,swap_reloc_out)
#define elf_swap_reloca_out		NAME(bfd_elf,swap_reloca_out)
#define elf_swap_symbol_in		NAME(bfd_elf,swap_symbol_in)
#define elf_swap_symbol_out		NAME(bfd_elf,swap_symbol_out)
#define elf_swap_phdr_in		NAME(bfd_elf,swap_phdr_in)
#define elf_swap_phdr_out		NAME(bfd_elf,swap_phdr_out)
#define elf_swap_dyn_in			NAME(bfd_elf,swap_dyn_in)
#define elf_swap_dyn_out		NAME(bfd_elf,swap_dyn_out)
#define elf_get_reloc_upper_bound	NAME(bfd_elf,get_reloc_upper_bound)
#define elf_canonicalize_reloc		NAME(bfd_elf,canonicalize_reloc)
#define elf_slurp_symbol_table		NAME(bfd_elf,slurp_symbol_table)
#define elf_get_symtab			NAME(bfd_elf,get_symtab)
#define elf_canonicalize_dynamic_symtab \
  NAME(bfd_elf,canonicalize_dynamic_symtab)
#define elf_make_empty_symbol		NAME(bfd_elf,make_empty_symbol)
#define elf_get_symbol_info		NAME(bfd_elf,get_symbol_info)
#define elf_get_lineno			NAME(bfd_elf,get_lineno)
#define elf_set_arch_mach		NAME(bfd_elf,set_arch_mach)
#define elf_find_nearest_line		NAME(bfd_elf,find_nearest_line)
#define elf_sizeof_headers		NAME(bfd_elf,sizeof_headers)
#define elf_set_section_contents	NAME(bfd_elf,set_section_contents)
#define elf_no_info_to_howto		NAME(bfd_elf,no_info_to_howto)
#define elf_no_info_to_howto_rel	NAME(bfd_elf,no_info_to_howto_rel)
#define elf_find_section		NAME(bfd_elf,find_section)
#define elf_bfd_link_add_symbols	NAME(bfd_elf,bfd_link_add_symbols)
#define elf_add_dynamic_entry		NAME(bfd_elf,add_dynamic_entry)
#define elf_write_shdrs_and_ehdr	NAME(bfd_elf,write_shdrs_and_ehdr)
#define elf_write_out_phdrs		NAME(bfd_elf,write_out_phdrs)
#define elf_write_relocs		NAME(bfd_elf,write_relocs)
#define elf_slurp_reloc_table		NAME(bfd_elf,slurp_reloc_table)
#define elf_link_create_dynamic_sections \
  NAME(bfd_elf,link_create_dynamic_sections)
#define elf_link_record_dynamic_symbol  _bfd_elf_link_record_dynamic_symbol
#define elf_bfd_final_link		NAME(bfd_elf,bfd_final_link)
#define elf_create_pointer_linker_section NAME(bfd_elf,create_pointer_linker_section)
#define elf_finish_pointer_linker_section NAME(bfd_elf,finish_pointer_linker_section)
#define elf_gc_sections			NAME(_bfd_elf,gc_sections)
#define elf_gc_common_finalize_got_offsets \
  NAME(_bfd_elf,gc_common_finalize_got_offsets)
#define elf_gc_common_final_link	NAME(_bfd_elf,gc_common_final_link)
#define elf_gc_record_vtinherit		NAME(_bfd_elf,gc_record_vtinherit)
#define elf_gc_record_vtentry		NAME(_bfd_elf,gc_record_vtentry)
#define elf_link_record_local_dynamic_symbol \
  NAME(_bfd_elf,link_record_local_dynamic_symbol)

#if ARCH_SIZE == 64
#define ELF_R_INFO(X,Y)	ELF64_R_INFO(X,Y)
#define ELF_R_SYM(X)	ELF64_R_SYM(X)
#define ELF_R_TYPE(X)	ELF64_R_TYPE(X)
#define ELFCLASS	ELFCLASS64
#define FILE_ALIGN	8
#define LOG_FILE_ALIGN	3
#endif
#if ARCH_SIZE == 32
#define ELF_R_INFO(X,Y)	ELF32_R_INFO(X,Y)
#define ELF_R_SYM(X)	ELF32_R_SYM(X)
#define ELF_R_TYPE(X)	ELF32_R_TYPE(X)
#define ELFCLASS	ELFCLASS32
#define FILE_ALIGN	4
#define LOG_FILE_ALIGN	2
#endif

/* Static functions */

static void elf_swap_ehdr_in
  PARAMS ((bfd *, const Elf_External_Ehdr *, Elf_Internal_Ehdr *));
static void elf_swap_ehdr_out
  PARAMS ((bfd *, const Elf_Internal_Ehdr *, Elf_External_Ehdr *));
static void elf_swap_shdr_in
  PARAMS ((bfd *, const Elf_External_Shdr *, Elf_Internal_Shdr *));
static void elf_swap_shdr_out
  PARAMS ((bfd *, const Elf_Internal_Shdr *, Elf_External_Shdr *));

#define elf_stringtab_init _bfd_elf_stringtab_init

#define section_from_elf_index bfd_section_from_elf_index

static boolean elf_slurp_reloc_table_from_section
  PARAMS ((bfd *, asection *, Elf_Internal_Shdr *, bfd_size_type,
	   arelent *, asymbol **, boolean));

static boolean elf_file_p PARAMS ((Elf_External_Ehdr *));

#ifdef DEBUG
static void elf_debug_section PARAMS ((int, Elf_Internal_Shdr *));
static void elf_debug_file PARAMS ((Elf_Internal_Ehdr *));
static char *elf_symbol_flags PARAMS ((flagword));
#endif

/* Structure swapping routines */

/* Should perhaps use put_offset, put_word, etc.  For now, the two versions
   can be handled by explicitly specifying 32 bits or "the long type".  */
#if ARCH_SIZE == 64
#define put_word	bfd_h_put_64
#define put_signed_word	bfd_h_put_signed_64
#define get_word	bfd_h_get_64
#define get_signed_word	bfd_h_get_signed_64
#endif
#if ARCH_SIZE == 32
#define put_word	bfd_h_put_32
#define put_signed_word	bfd_h_put_signed_32
#define get_word	bfd_h_get_32
#define get_signed_word	bfd_h_get_signed_32
#endif

/* Translate an ELF symbol in external format into an ELF symbol in internal
   format.  */

void
elf_swap_symbol_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Sym *src;
     Elf_Internal_Sym *dst;
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;

  dst->st_name = bfd_h_get_32 (abfd, (bfd_byte *) src->st_name);
  if (signed_vma)
    dst->st_value = get_signed_word (abfd, (bfd_byte *) src->st_value);
  else
    dst->st_value = get_word (abfd, (bfd_byte *) src->st_value);
  dst->st_size = get_word (abfd, (bfd_byte *) src->st_size);
  dst->st_info = bfd_h_get_8 (abfd, (bfd_byte *) src->st_info);
  dst->st_other = bfd_h_get_8 (abfd, (bfd_byte *) src->st_other);
  dst->st_shndx = bfd_h_get_16 (abfd, (bfd_byte *) src->st_shndx);
}

/* Translate an ELF symbol in internal format into an ELF symbol in external
   format.  */

void
elf_swap_symbol_out (abfd, src, cdst)
     bfd *abfd;
     const Elf_Internal_Sym *src;
     PTR cdst;
{
  Elf_External_Sym *dst = (Elf_External_Sym *) cdst;
  bfd_h_put_32 (abfd, src->st_name, dst->st_name);
  put_word (abfd, src->st_value, dst->st_value);
  put_word (abfd, src->st_size, dst->st_size);
  bfd_h_put_8 (abfd, src->st_info, dst->st_info);
  bfd_h_put_8 (abfd, src->st_other, dst->st_other);
  bfd_h_put_16 (abfd, src->st_shndx, dst->st_shndx);
}

/* Translate an ELF file header in external format into an ELF file header in
   internal format.  */

static void
elf_swap_ehdr_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Ehdr *src;
     Elf_Internal_Ehdr *dst;
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;
  memcpy (dst->e_ident, src->e_ident, EI_NIDENT);
  dst->e_type = bfd_h_get_16 (abfd, (bfd_byte *) src->e_type);
  dst->e_machine = bfd_h_get_16 (abfd, (bfd_byte *) src->e_machine);
  dst->e_version = bfd_h_get_32 (abfd, (bfd_byte *) src->e_version);
  if (signed_vma)
    dst->e_entry = get_signed_word (abfd, (bfd_byte *) src->e_entry);
  else
    dst->e_entry = get_word (abfd, (bfd_byte *) src->e_entry);
  dst->e_phoff = get_word (abfd, (bfd_byte *) src->e_phoff);
  dst->e_shoff = get_word (abfd, (bfd_byte *) src->e_shoff);
  dst->e_flags = bfd_h_get_32 (abfd, (bfd_byte *) src->e_flags);
  dst->e_ehsize = bfd_h_get_16 (abfd, (bfd_byte *) src->e_ehsize);
  dst->e_phentsize = bfd_h_get_16 (abfd, (bfd_byte *) src->e_phentsize);
  dst->e_phnum = bfd_h_get_16 (abfd, (bfd_byte *) src->e_phnum);
  dst->e_shentsize = bfd_h_get_16 (abfd, (bfd_byte *) src->e_shentsize);
  dst->e_shnum = bfd_h_get_16 (abfd, (bfd_byte *) src->e_shnum);
  dst->e_shstrndx = bfd_h_get_16 (abfd, (bfd_byte *) src->e_shstrndx);
}

/* Translate an ELF file header in internal format into an ELF file header in
   external format.  */

static void
elf_swap_ehdr_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Ehdr *src;
     Elf_External_Ehdr *dst;
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;
  memcpy (dst->e_ident, src->e_ident, EI_NIDENT);
  /* note that all elements of dst are *arrays of unsigned char* already...  */
  bfd_h_put_16 (abfd, src->e_type, dst->e_type);
  bfd_h_put_16 (abfd, src->e_machine, dst->e_machine);
  bfd_h_put_32 (abfd, src->e_version, dst->e_version);
  if (signed_vma)
    put_signed_word (abfd, src->e_entry, dst->e_entry);
  else
    put_word (abfd, src->e_entry, dst->e_entry);
  put_word (abfd, src->e_phoff, dst->e_phoff);
  put_word (abfd, src->e_shoff, dst->e_shoff);
  bfd_h_put_32 (abfd, src->e_flags, dst->e_flags);
  bfd_h_put_16 (abfd, src->e_ehsize, dst->e_ehsize);
  bfd_h_put_16 (abfd, src->e_phentsize, dst->e_phentsize);
  bfd_h_put_16 (abfd, src->e_phnum, dst->e_phnum);
  bfd_h_put_16 (abfd, src->e_shentsize, dst->e_shentsize);
  bfd_h_put_16 (abfd, src->e_shnum, dst->e_shnum);
  bfd_h_put_16 (abfd, src->e_shstrndx, dst->e_shstrndx);
}

/* Translate an ELF section header table entry in external format into an
   ELF section header table entry in internal format.  */

static void
elf_swap_shdr_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Shdr *src;
     Elf_Internal_Shdr *dst;
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;

  dst->sh_name = bfd_h_get_32 (abfd, (bfd_byte *) src->sh_name);
  dst->sh_type = bfd_h_get_32 (abfd, (bfd_byte *) src->sh_type);
  dst->sh_flags = get_word (abfd, (bfd_byte *) src->sh_flags);
  if (signed_vma)
    dst->sh_addr = get_signed_word (abfd, (bfd_byte *) src->sh_addr);
  else
    dst->sh_addr = get_word (abfd, (bfd_byte *) src->sh_addr);
  dst->sh_offset = get_word (abfd, (bfd_byte *) src->sh_offset);
  dst->sh_size = get_word (abfd, (bfd_byte *) src->sh_size);
  dst->sh_link = bfd_h_get_32 (abfd, (bfd_byte *) src->sh_link);
  dst->sh_info = bfd_h_get_32 (abfd, (bfd_byte *) src->sh_info);
  dst->sh_addralign = get_word (abfd, (bfd_byte *) src->sh_addralign);
  dst->sh_entsize = get_word (abfd, (bfd_byte *) src->sh_entsize);
  dst->bfd_section = NULL;
  dst->contents = NULL;
}

/* Translate an ELF section header table entry in internal format into an
   ELF section header table entry in external format.  */

static void
elf_swap_shdr_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Shdr *src;
     Elf_External_Shdr *dst;
{
  /* note that all elements of dst are *arrays of unsigned char* already...  */
  bfd_h_put_32 (abfd, src->sh_name, dst->sh_name);
  bfd_h_put_32 (abfd, src->sh_type, dst->sh_type);
  put_word (abfd, src->sh_flags, dst->sh_flags);
  put_word (abfd, src->sh_addr, dst->sh_addr);
  put_word (abfd, src->sh_offset, dst->sh_offset);
  put_word (abfd, src->sh_size, dst->sh_size);
  bfd_h_put_32 (abfd, src->sh_link, dst->sh_link);
  bfd_h_put_32 (abfd, src->sh_info, dst->sh_info);
  put_word (abfd, src->sh_addralign, dst->sh_addralign);
  put_word (abfd, src->sh_entsize, dst->sh_entsize);
}

/* Translate an ELF program header table entry in external format into an
   ELF program header table entry in internal format.  */

void
elf_swap_phdr_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Phdr *src;
     Elf_Internal_Phdr *dst;
{
  int signed_vma = get_elf_backend_data (abfd)->sign_extend_vma;

  dst->p_type = bfd_h_get_32 (abfd, (bfd_byte *) src->p_type);
  dst->p_flags = bfd_h_get_32 (abfd, (bfd_byte *) src->p_flags);
  dst->p_offset = get_word (abfd, (bfd_byte *) src->p_offset);
  if (signed_vma)
    {
      dst->p_vaddr = get_signed_word (abfd, (bfd_byte *) src->p_vaddr);
      dst->p_paddr = get_signed_word (abfd, (bfd_byte *) src->p_paddr);
    }
  else
    {
      dst->p_vaddr = get_word (abfd, (bfd_byte *) src->p_vaddr);
      dst->p_paddr = get_word (abfd, (bfd_byte *) src->p_paddr);
    }
  dst->p_filesz = get_word (abfd, (bfd_byte *) src->p_filesz);
  dst->p_memsz = get_word (abfd, (bfd_byte *) src->p_memsz);
  dst->p_align = get_word (abfd, (bfd_byte *) src->p_align);
}

void
elf_swap_phdr_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Phdr *src;
     Elf_External_Phdr *dst;
{
  /* note that all elements of dst are *arrays of unsigned char* already...  */
  bfd_h_put_32 (abfd, src->p_type, dst->p_type);
  put_word (abfd, src->p_offset, dst->p_offset);
  put_word (abfd, src->p_vaddr, dst->p_vaddr);
  put_word (abfd, src->p_paddr, dst->p_paddr);
  put_word (abfd, src->p_filesz, dst->p_filesz);
  put_word (abfd, src->p_memsz, dst->p_memsz);
  bfd_h_put_32 (abfd, src->p_flags, dst->p_flags);
  put_word (abfd, src->p_align, dst->p_align);
}

/* Translate an ELF reloc from external format to internal format.  */
INLINE void
elf_swap_reloc_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Rel *src;
     Elf_Internal_Rel *dst;
{
  dst->r_offset = get_word (abfd, (bfd_byte *) src->r_offset);
  dst->r_info = get_word (abfd, (bfd_byte *) src->r_info);
}

INLINE void
elf_swap_reloca_in (abfd, src, dst)
     bfd *abfd;
     const Elf_External_Rela *src;
     Elf_Internal_Rela *dst;
{
  dst->r_offset = get_word (abfd, (bfd_byte *) src->r_offset);
  dst->r_info = get_word (abfd, (bfd_byte *) src->r_info);
  dst->r_addend = get_signed_word (abfd, (bfd_byte *) src->r_addend);
}

/* Translate an ELF reloc from internal format to external format.  */
INLINE void
elf_swap_reloc_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rel *src;
     Elf_External_Rel *dst;
{
  put_word (abfd, src->r_offset, dst->r_offset);
  put_word (abfd, src->r_info, dst->r_info);
}

INLINE void
elf_swap_reloca_out (abfd, src, dst)
     bfd *abfd;
     const Elf_Internal_Rela *src;
     Elf_External_Rela *dst;
{
  put_word (abfd, src->r_offset, dst->r_offset);
  put_word (abfd, src->r_info, dst->r_info);
  put_signed_word (abfd, src->r_addend, dst->r_addend);
}

INLINE void
elf_swap_dyn_in (abfd, p, dst)
     bfd *abfd;
     const PTR p;
     Elf_Internal_Dyn *dst;
{
  const Elf_External_Dyn *src = (const Elf_External_Dyn *) p;

  dst->d_tag = get_word (abfd, src->d_tag);
  dst->d_un.d_val = get_word (abfd, src->d_un.d_val);
}

INLINE void
elf_swap_dyn_out (abfd, src, p)
     bfd *abfd;
     const Elf_Internal_Dyn *src;
     PTR p;
{
  Elf_External_Dyn *dst = (Elf_External_Dyn *) p;

  put_word (abfd, src->d_tag, dst->d_tag);
  put_word (abfd, src->d_un.d_val, dst->d_un.d_val);
}

/* ELF .o/exec file reading */

/* Begin processing a given object.

   First we validate the file by reading in the ELF header and checking
   the magic number.  */

static INLINE boolean
elf_file_p (x_ehdrp)
     Elf_External_Ehdr *x_ehdrp;
{
  return ((x_ehdrp->e_ident[EI_MAG0] == ELFMAG0)
	  && (x_ehdrp->e_ident[EI_MAG1] == ELFMAG1)
	  && (x_ehdrp->e_ident[EI_MAG2] == ELFMAG2)
	  && (x_ehdrp->e_ident[EI_MAG3] == ELFMAG3));
}

/* Check to see if the file associated with ABFD matches the target vector
   that ABFD points to.

   Note that we may be called several times with the same ABFD, but different
   target vectors, most of which will not match.  We have to avoid leaving
   any side effects in ABFD, or any data it points to (like tdata), if the
   file does not match the target vector.  */

const bfd_target *
elf_object_p (abfd)
     bfd *abfd;
{
  Elf_External_Ehdr x_ehdr;	/* Elf file header, external form */
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_External_Shdr x_shdr;	/* Section header table entry, external form */
  Elf_Internal_Shdr *i_shdrp = NULL; /* Section header table, internal form */
  unsigned int shindex;
  char *shstrtab;		/* Internal copy of section header stringtab */
  struct elf_backend_data *ebd;
  struct elf_obj_tdata *preserved_tdata = elf_tdata (abfd);
  struct sec *preserved_sections = abfd->sections;
  unsigned int preserved_section_count = abfd->section_count;
  enum bfd_architecture previous_arch = bfd_get_arch (abfd);
  unsigned long previous_mach = bfd_get_mach (abfd);
  struct elf_obj_tdata *new_tdata = NULL;
  asection *s;

  /* Clear section information, since there might be a recognized bfd that
     we now check if we can replace, and we don't want to append to it.  */
  abfd->sections = NULL;
  abfd->section_count = 0;

  /* Read in the ELF header in external format.  */

  if (bfd_read ((PTR) & x_ehdr, sizeof (x_ehdr), 1, abfd) != sizeof (x_ehdr))
    {
      if (bfd_get_error () != bfd_error_system_call)
	goto got_wrong_format_error;
      else
	goto got_no_match;
    }

  /* Now check to see if we have a valid ELF file, and one that BFD can
     make use of.  The magic number must match, the address size ('class')
     and byte-swapping must match our XVEC entry, and it must have a
     section header table (FIXME: See comments re sections at top of this
     file).  */

  if ((elf_file_p (&x_ehdr) == false) ||
      (x_ehdr.e_ident[EI_VERSION] != EV_CURRENT) ||
      (x_ehdr.e_ident[EI_CLASS] != ELFCLASS))
    goto got_wrong_format_error;

  /* Check that file's byte order matches xvec's */
  switch (x_ehdr.e_ident[EI_DATA])
    {
    case ELFDATA2MSB:		/* Big-endian */
      if (! bfd_header_big_endian (abfd))
	goto got_wrong_format_error;
      break;
    case ELFDATA2LSB:		/* Little-endian */
      if (! bfd_header_little_endian (abfd))
	goto got_wrong_format_error;
      break;
    case ELFDATANONE:		/* No data encoding specified */
    default:			/* Unknown data encoding specified */
      goto got_wrong_format_error;
    }

  /* Allocate an instance of the elf_obj_tdata structure and hook it up to
     the tdata pointer in the bfd.  */

  new_tdata = ((struct elf_obj_tdata *)
	       bfd_zalloc (abfd, sizeof (struct elf_obj_tdata)));
  if (new_tdata == NULL)
    goto got_no_match;
  elf_tdata (abfd) = new_tdata;

  /* Now that we know the byte order, swap in the rest of the header */
  i_ehdrp = elf_elfheader (abfd);
  elf_swap_ehdr_in (abfd, &x_ehdr, i_ehdrp);
#if DEBUG & 1
  elf_debug_file (i_ehdrp);
#endif

  /* Reject ET_CORE (header indicates core file, not object file) */
  if (i_ehdrp->e_type == ET_CORE)
    goto got_wrong_format_error;

  /* If there is no section header table, we're hosed.  */
  if (i_ehdrp->e_shoff == 0)
    goto got_wrong_format_error;

  /* As a simple sanity check, verify that the what BFD thinks is the
     size of each section header table entry actually matches the size
     recorded in the file.  */
  if (i_ehdrp->e_shentsize != sizeof (x_shdr))
    goto got_wrong_format_error;

  ebd = get_elf_backend_data (abfd);

  /* Check that the ELF e_machine field matches what this particular
     BFD format expects.  */
  if (ebd->elf_machine_code != i_ehdrp->e_machine
      && (ebd->elf_machine_alt1 == 0 || i_ehdrp->e_machine != ebd->elf_machine_alt1)
      && (ebd->elf_machine_alt2 == 0 || i_ehdrp->e_machine != ebd->elf_machine_alt2))
    {
      const bfd_target * const *target_ptr;

      if (ebd->elf_machine_code != EM_NONE)
	goto got_wrong_format_error;

      /* This is the generic ELF target.  Let it match any ELF target
	 for which we do not have a specific backend.  */
      for (target_ptr = bfd_target_vector; *target_ptr != NULL; target_ptr++)
	{
	  struct elf_backend_data *back;

	  if ((*target_ptr)->flavour != bfd_target_elf_flavour)
	    continue;
	  back = (struct elf_backend_data *) (*target_ptr)->backend_data;
	  if (back->elf_machine_code == i_ehdrp->e_machine
	      || (back->elf_machine_alt1 != 0
		  && back->elf_machine_alt1 == i_ehdrp->e_machine)
	      || (back->elf_machine_alt2 != 0
		  && back->elf_machine_alt2 == i_ehdrp->e_machine))
	    {
	      /* target_ptr is an ELF backend which matches this
		 object file, so reject the generic ELF target.  */
	      goto got_wrong_format_error;
	    }
	}
    }

  if (i_ehdrp->e_type == ET_EXEC)
    abfd->flags |= EXEC_P;
  else if (i_ehdrp->e_type == ET_DYN)
    abfd->flags |= DYNAMIC;

  if (i_ehdrp->e_phnum > 0)
    abfd->flags |= D_PAGED;

  if (! bfd_default_set_arch_mach (abfd, ebd->arch, 0))
    {
      /* It's OK if this fails for the generic target.  */
      if (ebd->elf_machine_code != EM_NONE)
	goto got_no_match;
    }

  /* Remember the entry point specified in the ELF file header.  */
  bfd_set_start_address (abfd, i_ehdrp->e_entry);

  /* Allocate space for a copy of the section header table in
     internal form, seek to the section header table in the file,
     read it in, and convert it to internal form.  */
  i_shdrp = ((Elf_Internal_Shdr *)
	     bfd_alloc (abfd, sizeof (*i_shdrp) * i_ehdrp->e_shnum));
  elf_elfsections (abfd) = ((Elf_Internal_Shdr **)
			    bfd_alloc (abfd,
				       sizeof (i_shdrp) * i_ehdrp->e_shnum));
  if (!i_shdrp || !elf_elfsections (abfd))
    goto got_no_match;
  if (bfd_seek (abfd, i_ehdrp->e_shoff, SEEK_SET) != 0)
    goto got_no_match;
  for (shindex = 0; shindex < i_ehdrp->e_shnum; shindex++)
    {
      if (bfd_read ((PTR) & x_shdr, sizeof x_shdr, 1, abfd) != sizeof (x_shdr))
	goto got_no_match;
      elf_swap_shdr_in (abfd, &x_shdr, i_shdrp + shindex);
      elf_elfsections (abfd)[shindex] = i_shdrp + shindex;

      /* If the section is loaded, but not page aligned, clear
         D_PAGED.  */
      if ((i_shdrp[shindex].sh_flags & SHF_ALLOC) != 0
	  && i_shdrp[shindex].sh_type != SHT_NOBITS
	  && (((i_shdrp[shindex].sh_addr - i_shdrp[shindex].sh_offset)
	       % ebd->maxpagesize)
	      != 0))
	abfd->flags &= ~D_PAGED;
    }
  if (i_ehdrp->e_shstrndx)
    {
      if (! bfd_section_from_shdr (abfd, i_ehdrp->e_shstrndx))
	goto got_no_match;
    }

  /* Read in the program headers.  */
  if (i_ehdrp->e_phnum == 0)
    elf_tdata (abfd)->phdr = NULL;
  else
    {
      Elf_Internal_Phdr *i_phdr;
      unsigned int i;

      elf_tdata (abfd)->phdr = ((Elf_Internal_Phdr *)
				bfd_alloc (abfd,
					   (i_ehdrp->e_phnum
					    * sizeof (Elf_Internal_Phdr))));
      if (elf_tdata (abfd)->phdr == NULL)
	goto got_no_match;
      if (bfd_seek (abfd, i_ehdrp->e_phoff, SEEK_SET) != 0)
	goto got_no_match;
      i_phdr = elf_tdata (abfd)->phdr;
      for (i = 0; i < i_ehdrp->e_phnum; i++, i_phdr++)
	{
	  Elf_External_Phdr x_phdr;

	  if (bfd_read ((PTR) &x_phdr, sizeof x_phdr, 1, abfd)
	      != sizeof x_phdr)
	    goto got_no_match;
	  elf_swap_phdr_in (abfd, &x_phdr, i_phdr);
	}
    }

  /* Read in the string table containing the names of the sections.  We
     will need the base pointer to this table later.  */
  /* We read this inline now, so that we don't have to go through
     bfd_section_from_shdr with it (since this particular strtab is
     used to find all of the ELF section names.) */

  shstrtab = bfd_elf_get_str_section (abfd, i_ehdrp->e_shstrndx);
  if (!shstrtab)
    goto got_no_match;

  /* Once all of the section headers have been read and converted, we
     can start processing them.  Note that the first section header is
     a dummy placeholder entry, so we ignore it.  */

  for (shindex = 1; shindex < i_ehdrp->e_shnum; shindex++)
    {
      if (! bfd_section_from_shdr (abfd, shindex))
	goto got_no_match;
    }

  /* Let the backend double check the format and override global
     information.  */
  if (ebd->elf_backend_object_p)
    {
      if ((*ebd->elf_backend_object_p) (abfd) == false)
	goto got_wrong_format_error;
    }

  /* If we have created any reloc sections that are associated with
     debugging sections, mark the reloc sections as debugging as well.  */
  for (s = abfd->sections; s != NULL; s = s->next)
    {
      if ((elf_section_data (s)->this_hdr.sh_type == SHT_REL
	   || elf_section_data (s)->this_hdr.sh_type == SHT_RELA)
	  && elf_section_data (s)->this_hdr.sh_info > 0)
	{
	  unsigned long targ_index;
	  asection *targ_sec;

	  targ_index = elf_section_data (s)->this_hdr.sh_info;
	  targ_sec = bfd_section_from_elf_index (abfd, targ_index);
	  if (targ_sec != NULL
	      && (targ_sec->flags & SEC_DEBUGGING) != 0)
	    s->flags |= SEC_DEBUGGING;
	}
    }

  return (abfd->xvec);

 got_wrong_format_error:
  /* There is way too much undoing of half-known state here.  The caller,
     bfd_check_format_matches, really shouldn't iterate on live bfd's to
     check match/no-match like it does.  We have to rely on that a call to
     bfd_default_set_arch_mach with the previously known mach, undoes what
     was done by the first bfd_default_set_arch_mach (with mach 0) here.
     For this to work, only elf-data and the mach may be changed by the
     target-specific elf_backend_object_p function.  Note that saving the
     whole bfd here and restoring it would be even worse; the first thing
     you notice is that the cached bfd file position gets out of sync.  */
  bfd_default_set_arch_mach (abfd, previous_arch, previous_mach);
  bfd_set_error (bfd_error_wrong_format);
 got_no_match:
  if (new_tdata != NULL
      && new_tdata->elf_sect_ptr != NULL)
    bfd_release (abfd, new_tdata->elf_sect_ptr);
  if (i_shdrp != NULL)
    bfd_release (abfd, i_shdrp);
  if (new_tdata != NULL)
    bfd_release (abfd, new_tdata);
  elf_tdata (abfd) = preserved_tdata;
  abfd->sections = preserved_sections;
  abfd->section_count = preserved_section_count;
  return (NULL);
}

/* ELF .o/exec file writing */

/* Write out the relocs.  */

void
elf_write_relocs (abfd, sec, data)
     bfd *abfd;
     asection *sec;
     PTR data;
{
  boolean *failedp = (boolean *) data;
  Elf_Internal_Shdr *rela_hdr;
  Elf_External_Rela *outbound_relocas;
  Elf_External_Rel *outbound_relocs;
  unsigned int idx;
  int use_rela_p;
  asymbol *last_sym = 0;
  int last_sym_idx = 0;

  /* If we have already failed, don't do anything.  */
  if (*failedp)
    return;

  if ((sec->flags & SEC_RELOC) == 0)
    return;

  /* The linker backend writes the relocs out itself, and sets the
     reloc_count field to zero to inhibit writing them here.  Also,
     sometimes the SEC_RELOC flag gets set even when there aren't any
     relocs.  */
  if (sec->reloc_count == 0)
    return;

  rela_hdr = &elf_section_data (sec)->rel_hdr;

  rela_hdr->sh_size = rela_hdr->sh_entsize * sec->reloc_count;
  rela_hdr->contents = (PTR) bfd_alloc (abfd, rela_hdr->sh_size);
  if (rela_hdr->contents == NULL)
    {
      *failedp = true;
      return;
    }

  /* Figure out whether the relocations are RELA or REL relocations.  */
  if (rela_hdr->sh_type == SHT_RELA)
    use_rela_p = true;
  else if (rela_hdr->sh_type == SHT_REL)
    use_rela_p = false;
  else
    /* Every relocation section should be either an SHT_RELA or an
       SHT_REL section.  */
    abort ();

  /* orelocation has the data, reloc_count has the count...  */
  if (use_rela_p)
    {
      outbound_relocas = (Elf_External_Rela *) rela_hdr->contents;

      for (idx = 0; idx < sec->reloc_count; idx++)
	{
	  Elf_Internal_Rela dst_rela;
	  Elf_External_Rela *src_rela;
	  arelent *ptr;
	  asymbol *sym;
	  int n;

	  ptr = sec->orelocation[idx];
	  src_rela = outbound_relocas + idx;

	  /* The address of an ELF reloc is section relative for an object
	     file, and absolute for an executable file or shared library.
	     The address of a BFD reloc is always section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	    dst_rela.r_offset = ptr->address;
	  else
	    dst_rela.r_offset = ptr->address + sec->vma;

	  sym = *ptr->sym_ptr_ptr;
	  if (sym == last_sym)
	    n = last_sym_idx;
	  else if (bfd_is_abs_section (sym->section) && sym->value == 0)
	    n = STN_UNDEF;
	  else
	    {
	      last_sym = sym;
	      n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	      if (n < 0)
		{
		  *failedp = true;
		  return;
		}
	      last_sym_idx = n;
	    }

	  if ((*ptr->sym_ptr_ptr)->the_bfd != NULL
	      && (*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	      && ! _bfd_elf_validate_reloc (abfd, ptr))
	    {
	      *failedp = true;
	      return;
	    }

	  dst_rela.r_info = ELF_R_INFO (n, ptr->howto->type);

	  dst_rela.r_addend = ptr->addend;
	  elf_swap_reloca_out (abfd, &dst_rela, src_rela);
	}
    }
  else
    /* REL relocations */
    {
      outbound_relocs = (Elf_External_Rel *) rela_hdr->contents;

      for (idx = 0; idx < sec->reloc_count; idx++)
	{
	  Elf_Internal_Rel dst_rel;
	  Elf_External_Rel *src_rel;
	  arelent *ptr;
	  int n;
	  asymbol *sym;

	  ptr = sec->orelocation[idx];
	  sym = *ptr->sym_ptr_ptr;
	  src_rel = outbound_relocs + idx;

	  /* The address of an ELF reloc is section relative for an object
	     file, and absolute for an executable file or shared library.
	     The address of a BFD reloc is always section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0)
	    dst_rel.r_offset = ptr->address;
	  else
	    dst_rel.r_offset = ptr->address + sec->vma;

	  if (sym == last_sym)
	    n = last_sym_idx;
	  else if (bfd_is_abs_section (sym->section) && sym->value == 0)
	    n = STN_UNDEF;
	  else
	    {
	      last_sym = sym;
	      n = _bfd_elf_symbol_from_bfd_symbol (abfd, &sym);
	      if (n < 0)
		{
		  *failedp = true;
		  return;
		}
	      last_sym_idx = n;
	    }

	  if ((*ptr->sym_ptr_ptr)->the_bfd != NULL
	      && (*ptr->sym_ptr_ptr)->the_bfd->xvec != abfd->xvec
	      && ! _bfd_elf_validate_reloc (abfd, ptr))
	    {
	      *failedp = true;
	      return;
	    }

	  dst_rel.r_info = ELF_R_INFO (n, ptr->howto->type);

	  elf_swap_reloc_out (abfd, &dst_rel, src_rel);
	}
    }
}

/* Write out the program headers.  */

int
elf_write_out_phdrs (abfd, phdr, count)
     bfd *abfd;
     const Elf_Internal_Phdr *phdr;
     int count;
{
  while (count--)
    {
      Elf_External_Phdr extphdr;
      elf_swap_phdr_out (abfd, phdr, &extphdr);
      if (bfd_write (&extphdr, sizeof (Elf_External_Phdr), 1, abfd)
	  != sizeof (Elf_External_Phdr))
	return -1;
      phdr++;
    }
  return 0;
}

/* Write out the section headers and the ELF file header.  */

boolean
elf_write_shdrs_and_ehdr (abfd)
     bfd *abfd;
{
  Elf_External_Ehdr x_ehdr;	/* Elf file header, external form */
  Elf_Internal_Ehdr *i_ehdrp;	/* Elf file header, internal form */
  Elf_External_Shdr *x_shdrp;	/* Section header table, external form */
  Elf_Internal_Shdr **i_shdrp;	/* Section header table, internal form */
  unsigned int count;

  i_ehdrp = elf_elfheader (abfd);
  i_shdrp = elf_elfsections (abfd);

  /* swap the header before spitting it out...  */

#if DEBUG & 1
  elf_debug_file (i_ehdrp);
#endif
  elf_swap_ehdr_out (abfd, i_ehdrp, &x_ehdr);
  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || (bfd_write ((PTR) & x_ehdr, sizeof (x_ehdr), 1, abfd)
	  != sizeof (x_ehdr)))
    return false;

  /* at this point we've concocted all the ELF sections...  */
  x_shdrp = (Elf_External_Shdr *)
    bfd_alloc (abfd, sizeof (*x_shdrp) * (i_ehdrp->e_shnum));
  if (!x_shdrp)
    return false;

  for (count = 0; count < i_ehdrp->e_shnum; count++)
    {
#if DEBUG & 2
      elf_debug_section (count, i_shdrp[count]);
#endif
      elf_swap_shdr_out (abfd, i_shdrp[count], x_shdrp + count);
    }
  if (bfd_seek (abfd, (file_ptr) i_ehdrp->e_shoff, SEEK_SET) != 0
      || (bfd_write ((PTR) x_shdrp, sizeof (*x_shdrp), i_ehdrp->e_shnum, abfd)
	  != sizeof (*x_shdrp) * i_ehdrp->e_shnum))
    return false;

  /* need to dump the string table too...  */

  return true;
}

long
elf_slurp_symbol_table (abfd, symptrs, dynamic)
     bfd *abfd;
     asymbol **symptrs;		/* Buffer for generated bfd symbols */
     boolean dynamic;
{
  Elf_Internal_Shdr *hdr;
  Elf_Internal_Shdr *verhdr;
  unsigned long symcount;	/* Number of external ELF symbols */
  elf_symbol_type *sym;		/* Pointer to current bfd symbol */
  elf_symbol_type *symbase;	/* Buffer for generated bfd symbols */
  Elf_Internal_Sym i_sym;
  Elf_External_Sym *x_symp = NULL;
  Elf_External_Versym *x_versymp = NULL;

  /* Read each raw ELF symbol, converting from external ELF form to
     internal ELF form, and then using the information to create a
     canonical bfd symbol table entry.

     Note that we allocate the initial bfd canonical symbol buffer
     based on a one-to-one mapping of the ELF symbols to canonical
     symbols.  We actually use all the ELF symbols, so there will be no
     space left over at the end.  When we have all the symbols, we
     build the caller's pointer vector.  */

  if (! dynamic)
    {
      hdr = &elf_tdata (abfd)->symtab_hdr;
      verhdr = NULL;
    }
  else
    {
      hdr = &elf_tdata (abfd)->dynsymtab_hdr;
      if (elf_dynversym (abfd) == 0)
	verhdr = NULL;
      else
	verhdr = &elf_tdata (abfd)->dynversym_hdr;
      if ((elf_tdata (abfd)->dynverdef_section != 0
	   && elf_tdata (abfd)->verdef == NULL)
	  || (elf_tdata (abfd)->dynverref_section != 0
	      && elf_tdata (abfd)->verref == NULL))
	{
	  if (! _bfd_elf_slurp_version_tables (abfd))
	    return -1;
	}
    }

  if (bfd_seek (abfd, hdr->sh_offset, SEEK_SET) == -1)
    return -1;

  symcount = hdr->sh_size / sizeof (Elf_External_Sym);

  if (symcount == 0)
    sym = symbase = NULL;
  else
    {
      unsigned long i;

      if (bfd_seek (abfd, hdr->sh_offset, SEEK_SET) == -1)
	return -1;

      symbase = ((elf_symbol_type *)
		 bfd_zalloc (abfd, symcount * sizeof (elf_symbol_type)));
      if (symbase == (elf_symbol_type *) NULL)
	return -1;
      sym = symbase;

      /* Temporarily allocate room for the raw ELF symbols.  */
      x_symp = ((Elf_External_Sym *)
		bfd_malloc (symcount * sizeof (Elf_External_Sym)));
      if (x_symp == NULL && symcount != 0)
	goto error_return;

      if (bfd_read ((PTR) x_symp, sizeof (Elf_External_Sym), symcount, abfd)
	  != symcount * sizeof (Elf_External_Sym))
	goto error_return;

      /* Read the raw ELF version symbol information.  */

      if (verhdr != NULL
	  && verhdr->sh_size / sizeof (Elf_External_Versym) != symcount)
	{
	  (*_bfd_error_handler)
	    (_("%s: version count (%ld) does not match symbol count (%ld)"),
	     abfd->filename,
	     (long) (verhdr->sh_size / sizeof (Elf_External_Versym)),
	     symcount);

	  /* Slurp in the symbols without the version information,
             since that is more helpful than just quitting.  */
	  verhdr = NULL;
	}

      if (verhdr != NULL)
	{
	  if (bfd_seek (abfd, verhdr->sh_offset, SEEK_SET) != 0)
	    goto error_return;

	  x_versymp = (Elf_External_Versym *) bfd_malloc (verhdr->sh_size);
	  if (x_versymp == NULL && verhdr->sh_size != 0)
	    goto error_return;

	  if (bfd_read ((PTR) x_versymp, 1, verhdr->sh_size, abfd)
	      != verhdr->sh_size)
	    goto error_return;
	}

      /* Skip first symbol, which is a null dummy.  */
      for (i = 1; i < symcount; i++)
	{
	  elf_swap_symbol_in (abfd, x_symp + i, &i_sym);
	  memcpy (&sym->internal_elf_sym, &i_sym, sizeof (Elf_Internal_Sym));
#ifdef ELF_KEEP_EXTSYM
	  memcpy (&sym->native_elf_sym, x_symp + i, sizeof (Elf_External_Sym));
#endif
	  sym->symbol.the_bfd = abfd;

	  sym->symbol.name = bfd_elf_string_from_elf_section (abfd,
							      hdr->sh_link,
							      i_sym.st_name);

	  sym->symbol.value = i_sym.st_value;

	  if (i_sym.st_shndx > 0 && i_sym.st_shndx < SHN_LORESERVE)
	    {
	      sym->symbol.section = section_from_elf_index (abfd,
							    i_sym.st_shndx);
	      if (sym->symbol.section == NULL)
		{
		  /* This symbol is in a section for which we did not
		     create a BFD section.  Just use bfd_abs_section,
		     although it is wrong.  FIXME.  */
		  sym->symbol.section = bfd_abs_section_ptr;
		}
	    }
	  else if (i_sym.st_shndx == SHN_ABS)
	    {
	      sym->symbol.section = bfd_abs_section_ptr;
	    }
	  else if (i_sym.st_shndx == SHN_COMMON)
	    {
	      sym->symbol.section = bfd_com_section_ptr;
	      /* Elf puts the alignment into the `value' field, and
		 the size into the `size' field.  BFD wants to see the
		 size in the value field, and doesn't care (at the
		 moment) about the alignment.  */
	      sym->symbol.value = i_sym.st_size;
	    }
	  else if (i_sym.st_shndx == SHN_UNDEF)
	    {
	      sym->symbol.section = bfd_und_section_ptr;
	    }
	  else
	    sym->symbol.section = bfd_abs_section_ptr;

	  /* If this is a relocateable file, then the symbol value is
             already section relative.  */
	  if ((abfd->flags & (EXEC_P | DYNAMIC)) != 0)
	    sym->symbol.value -= sym->symbol.section->vma;

	  switch (ELF_ST_BIND (i_sym.st_info))
	    {
	    case STB_LOCAL:
	      sym->symbol.flags |= BSF_LOCAL;
	      break;
	    case STB_GLOBAL:
	      if (i_sym.st_shndx != SHN_UNDEF
		  && i_sym.st_shndx != SHN_COMMON)
		sym->symbol.flags |= BSF_GLOBAL;
	      break;
	    case STB_WEAK:
	      sym->symbol.flags |= BSF_WEAK;
	      break;
	    }

	  switch (ELF_ST_TYPE (i_sym.st_info))
	    {
	    case STT_SECTION:
	      sym->symbol.flags |= BSF_SECTION_SYM | BSF_DEBUGGING;
	      break;
	    case STT_FILE:
	      sym->symbol.flags |= BSF_FILE | BSF_DEBUGGING;
	      break;
	    case STT_FUNC:
	      sym->symbol.flags |= BSF_FUNCTION;
	      break;
	    case STT_OBJECT:
	      sym->symbol.flags |= BSF_OBJECT;
	      break;
	    }

	  if (dynamic)
	    sym->symbol.flags |= BSF_DYNAMIC;

	  if (x_versymp != NULL)
	    {
	      Elf_Internal_Versym iversym;

	      _bfd_elf_swap_versym_in (abfd, x_versymp + i, &iversym);
	      sym->version = iversym.vs_vers;
	    }

	  /* Do some backend-specific processing on this symbol.  */
	  {
	    struct elf_backend_data *ebd = get_elf_backend_data (abfd);
	    if (ebd->elf_backend_symbol_processing)
	      (*ebd->elf_backend_symbol_processing) (abfd, &sym->symbol);
	  }

	  sym++;
	}
    }

  /* Do some backend-specific processing on this symbol table.  */
  {
    struct elf_backend_data *ebd = get_elf_backend_data (abfd);
    if (ebd->elf_backend_symbol_table_processing)
      (*ebd->elf_backend_symbol_table_processing) (abfd, symbase, symcount);
  }

  /* We rely on the zalloc to clear out the final symbol entry.  */

  symcount = sym - symbase;

  /* Fill in the user's symbol pointer vector if needed.  */
  if (symptrs)
    {
      long l = symcount;

      sym = symbase;
      while (l-- > 0)
	{
	  *symptrs++ = &sym->symbol;
	  sym++;
	}
      *symptrs = 0;		/* Final null pointer */
    }

  if (x_versymp != NULL)
    free (x_versymp);
  if (x_symp != NULL)
    free (x_symp);
  return symcount;
error_return:
  if (x_versymp != NULL)
    free (x_versymp);
  if (x_symp != NULL)
    free (x_symp);
  return -1;
}

/* Read  relocations for ASECT from REL_HDR.  There are RELOC_COUNT of
   them.  */

static boolean
elf_slurp_reloc_table_from_section (abfd, asect, rel_hdr, reloc_count,
				    relents, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     Elf_Internal_Shdr *rel_hdr;
     bfd_size_type reloc_count;
     arelent *relents;
     asymbol **symbols;
     boolean dynamic;
{
  struct elf_backend_data * const ebd = get_elf_backend_data (abfd);
  PTR allocated = NULL;
  bfd_byte *native_relocs;
  arelent *relent;
  unsigned int i;
  int entsize;

  allocated = (PTR) bfd_malloc ((size_t) rel_hdr->sh_size);
  if (allocated == NULL)
    goto error_return;

  if (bfd_seek (abfd, rel_hdr->sh_offset, SEEK_SET) != 0
      || (bfd_read (allocated, 1, rel_hdr->sh_size, abfd)
	  != rel_hdr->sh_size))
    goto error_return;

  native_relocs = (bfd_byte *) allocated;

  entsize = rel_hdr->sh_entsize;
  BFD_ASSERT (entsize == sizeof (Elf_External_Rel)
	      || entsize == sizeof (Elf_External_Rela));

  for (i = 0, relent = relents;
       i < reloc_count;
       i++, relent++, native_relocs += entsize)
    {
      Elf_Internal_Rela rela;
      Elf_Internal_Rel rel;

      if (entsize == sizeof (Elf_External_Rela))
	elf_swap_reloca_in (abfd, (Elf_External_Rela *) native_relocs, &rela);
      else
	{
	  elf_swap_reloc_in (abfd, (Elf_External_Rel *) native_relocs, &rel);
	  rela.r_offset = rel.r_offset;
	  rela.r_info = rel.r_info;
	  rela.r_addend = 0;
	}

      /* The address of an ELF reloc is section relative for an object
	 file, and absolute for an executable file or shared library.
	 The address of a normal BFD reloc is always section relative,
	 and the address of a dynamic reloc is absolute..  */
      if ((abfd->flags & (EXEC_P | DYNAMIC)) == 0 || dynamic)
	relent->address = rela.r_offset;
      else
	relent->address = rela.r_offset - asect->vma;

      if (ELF_R_SYM (rela.r_info) == 0)
	relent->sym_ptr_ptr = bfd_abs_section_ptr->symbol_ptr_ptr;
      else
	{
	  asymbol **ps, *s;

	  ps = symbols + ELF_R_SYM (rela.r_info) - 1;
	  s = *ps;

	  /* Canonicalize ELF section symbols.  FIXME: Why?  */
	  if ((s->flags & BSF_SECTION_SYM) == 0)
	    relent->sym_ptr_ptr = ps;
	  else
	    relent->sym_ptr_ptr = s->section->symbol_ptr_ptr;
	}

      relent->addend = rela.r_addend;

      if (entsize == sizeof (Elf_External_Rela))
	(*ebd->elf_info_to_howto) (abfd, relent, &rela);
      else
	(*ebd->elf_info_to_howto_rel) (abfd, relent, &rel);
    }

  if (allocated != NULL)
    free (allocated);

  return true;

 error_return:
  if (allocated != NULL)
    free (allocated);
  return false;
}

/* Read in and swap the external relocs.  */

boolean
elf_slurp_reloc_table (abfd, asect, symbols, dynamic)
     bfd *abfd;
     asection *asect;
     asymbol **symbols;
     boolean dynamic;
{
  struct bfd_elf_section_data * const d = elf_section_data (asect);
  Elf_Internal_Shdr *rel_hdr;
  Elf_Internal_Shdr *rel_hdr2;
  bfd_size_type reloc_count;
  bfd_size_type reloc_count2;
  arelent *relents;

  if (asect->relocation != NULL)
    return true;

  if (! dynamic)
    {
      if ((asect->flags & SEC_RELOC) == 0
	  || asect->reloc_count == 0)
	return true;

      rel_hdr = &d->rel_hdr;
      reloc_count = NUM_SHDR_ENTRIES (rel_hdr);
      rel_hdr2 = d->rel_hdr2;
      reloc_count2 = (rel_hdr2 ? NUM_SHDR_ENTRIES (rel_hdr2) : 0);

      BFD_ASSERT (asect->reloc_count == reloc_count + reloc_count2);
      BFD_ASSERT (asect->rel_filepos == rel_hdr->sh_offset
		  || (rel_hdr2 && asect->rel_filepos == rel_hdr2->sh_offset));

    }
  else
    {
      /* Note that ASECT->RELOC_COUNT tends not to be accurate in this
	 case because relocations against this section may use the
	 dynamic symbol table, and in that case bfd_section_from_shdr
	 in elf.c does not update the RELOC_COUNT.  */
      if (asect->_raw_size == 0)
	return true;

      rel_hdr = &d->this_hdr;
      reloc_count = NUM_SHDR_ENTRIES (rel_hdr);
      rel_hdr2 = NULL;
      reloc_count2 = 0;
    }

  relents = ((arelent *)
	     bfd_alloc (abfd,
			(reloc_count + reloc_count2) * sizeof (arelent)));
  if (relents == NULL)
    return false;

  if (!elf_slurp_reloc_table_from_section (abfd, asect,
					   rel_hdr, reloc_count,
					   relents,
					   symbols, dynamic))
    return false;

  if (rel_hdr2
      && !elf_slurp_reloc_table_from_section (abfd, asect,
					      rel_hdr2, reloc_count2,
					      relents + reloc_count,
					      symbols, dynamic))
    return false;

  asect->relocation = relents;
  return true;
}

#ifdef DEBUG
static void
elf_debug_section (num, hdr)
     int num;
     Elf_Internal_Shdr *hdr;
{
  fprintf (stderr, "\nSection#%d '%s' 0x%.8lx\n", num,
	   hdr->bfd_section != NULL ? hdr->bfd_section->name : "",
	   (long) hdr);
  fprintf (stderr,
	   "sh_name      = %ld\tsh_type      = %ld\tsh_flags     = %ld\n",
	   (long) hdr->sh_name,
	   (long) hdr->sh_type,
	   (long) hdr->sh_flags);
  fprintf (stderr,
	   "sh_addr      = %ld\tsh_offset    = %ld\tsh_size      = %ld\n",
	   (long) hdr->sh_addr,
	   (long) hdr->sh_offset,
	   (long) hdr->sh_size);
  fprintf (stderr,
	   "sh_link      = %ld\tsh_info      = %ld\tsh_addralign = %ld\n",
	   (long) hdr->sh_link,
	   (long) hdr->sh_info,
	   (long) hdr->sh_addralign);
  fprintf (stderr, "sh_entsize   = %ld\n",
	   (long) hdr->sh_entsize);
  fflush (stderr);
}

static void
elf_debug_file (ehdrp)
     Elf_Internal_Ehdr *ehdrp;
{
  fprintf (stderr, "e_entry      = 0x%.8lx\n", (long) ehdrp->e_entry);
  fprintf (stderr, "e_phoff      = %ld\n", (long) ehdrp->e_phoff);
  fprintf (stderr, "e_phnum      = %ld\n", (long) ehdrp->e_phnum);
  fprintf (stderr, "e_phentsize  = %ld\n", (long) ehdrp->e_phentsize);
  fprintf (stderr, "e_shoff      = %ld\n", (long) ehdrp->e_shoff);
  fprintf (stderr, "e_shnum      = %ld\n", (long) ehdrp->e_shnum);
  fprintf (stderr, "e_shentsize  = %ld\n", (long) ehdrp->e_shentsize);
}

static char *
elf_symbol_flags (flags)
     flagword flags;
{
  static char buffer[1024];

  buffer[0] = '\0';
  if (flags & BSF_LOCAL)
    strcat (buffer, " local");

  if (flags & BSF_GLOBAL)
    strcat (buffer, " global");

  if (flags & BSF_DEBUGGING)
    strcat (buffer, " debug");

  if (flags & BSF_FUNCTION)
    strcat (buffer, " function");

  if (flags & BSF_KEEP)
    strcat (buffer, " keep");

  if (flags & BSF_KEEP_G)
    strcat (buffer, " keep_g");

  if (flags & BSF_WEAK)
    strcat (buffer, " weak");

  if (flags & BSF_SECTION_SYM)
    strcat (buffer, " section-sym");

  if (flags & BSF_OLD_COMMON)
    strcat (buffer, " old-common");

  if (flags & BSF_NOT_AT_END)
    strcat (buffer, " not-at-end");

  if (flags & BSF_CONSTRUCTOR)
    strcat (buffer, " constructor");

  if (flags & BSF_WARNING)
    strcat (buffer, " warning");

  if (flags & BSF_INDIRECT)
    strcat (buffer, " indirect");

  if (flags & BSF_FILE)
    strcat (buffer, " file");

  if (flags & DYNAMIC)
    strcat (buffer, " dynamic");

  if (flags & ~(BSF_LOCAL
		| BSF_GLOBAL
		| BSF_DEBUGGING
		| BSF_FUNCTION
		| BSF_KEEP
		| BSF_KEEP_G
		| BSF_WEAK
		| BSF_SECTION_SYM
		| BSF_OLD_COMMON
		| BSF_NOT_AT_END
		| BSF_CONSTRUCTOR
		| BSF_WARNING
		| BSF_INDIRECT
		| BSF_FILE
		| BSF_DYNAMIC))
    strcat (buffer, " unknown-bits");

  return buffer;
}
#endif

#include "elfcore.h"
#include "elflink.h"

/* Size-dependent data and functions.  */
const struct elf_size_info NAME(_bfd_elf,size_info) = {
  sizeof (Elf_External_Ehdr),
  sizeof (Elf_External_Phdr),
  sizeof (Elf_External_Shdr),
  sizeof (Elf_External_Rel),
  sizeof (Elf_External_Rela),
  sizeof (Elf_External_Sym),
  sizeof (Elf_External_Dyn),
  sizeof (Elf_External_Note),
  4,
  1,
  ARCH_SIZE, FILE_ALIGN,
  ELFCLASS, EV_CURRENT,
  elf_write_out_phdrs,
  elf_write_shdrs_and_ehdr,
  elf_write_relocs,
  elf_swap_symbol_out,
  elf_slurp_reloc_table,
  elf_slurp_symbol_table,
  elf_swap_dyn_in,
  elf_swap_dyn_out,
  NULL,
  NULL,
  NULL,
  NULL
};
