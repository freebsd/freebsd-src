/* Matsushita 10200 specific support for 32-bit ELF
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"

static reloc_howto_type *bfd_elf32_bfd_reloc_type_lookup
  PARAMS ((bfd *abfd, bfd_reloc_code_real_type code));
static void mn10200_info_to_howto
  PARAMS ((bfd *, arelent *, Elf_Internal_Rela *));
static bfd_boolean mn10200_elf_relax_delete_bytes
  PARAMS ((bfd *, asection *, bfd_vma, int));
static bfd_boolean mn10200_elf_symbol_address_p
  PARAMS ((bfd *, asection *, Elf_Internal_Sym *, bfd_vma));
static bfd_reloc_status_type mn10200_elf_final_link_relocate
  PARAMS ((reloc_howto_type *, bfd *, bfd *, asection *,
	   bfd_byte *, bfd_vma, bfd_vma, bfd_vma,
	   struct bfd_link_info *, asection *, int));
static bfd_boolean mn10200_elf_relocate_section
  PARAMS ((bfd *, struct bfd_link_info *, bfd *, asection *,
	   bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Sym *,
	   asection **));
static bfd_boolean mn10200_elf_relax_section
  PARAMS ((bfd *, asection *, struct bfd_link_info *, bfd_boolean *));
static bfd_byte * mn10200_elf_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_link_info *, struct bfd_link_order *,
	   bfd_byte *, bfd_boolean, asymbol **));

enum reloc_type {
  R_MN10200_NONE = 0,
  R_MN10200_32,
  R_MN10200_16,
  R_MN10200_8,
  R_MN10200_24,
  R_MN10200_PCREL8,
  R_MN10200_PCREL16,
  R_MN10200_PCREL24,
  R_MN10200_MAX
};

static reloc_howto_type elf_mn10200_howto_table[] = {
  /* Dummy relocation.  Does nothing.  */
  HOWTO (R_MN10200_NONE,
	 0,
	 2,
	 16,
	 FALSE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_NONE",
	 FALSE,
	 0,
	 0,
	 FALSE),
  /* Standard 32 bit reloc.  */
  HOWTO (R_MN10200_32,
	 0,
	 2,
	 32,
	 FALSE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_32",
	 FALSE,
	 0xffffffff,
	 0xffffffff,
	 FALSE),
  /* Standard 16 bit reloc.  */
  HOWTO (R_MN10200_16,
	 0,
	 1,
	 16,
	 FALSE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_16",
	 FALSE,
	 0xffff,
	 0xffff,
	 FALSE),
  /* Standard 8 bit reloc.  */
  HOWTO (R_MN10200_8,
	 0,
	 0,
	 8,
	 FALSE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_8",
	 FALSE,
	 0xff,
	 0xff,
	 FALSE),
  /* Standard 24 bit reloc.  */
  HOWTO (R_MN10200_24,
	 0,
	 2,
	 24,
	 FALSE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_24",
	 FALSE,
	 0xffffff,
	 0xffffff,
	 FALSE),
  /* Simple 8 pc-relative reloc.  */
  HOWTO (R_MN10200_PCREL8,
	 0,
	 0,
	 8,
	 TRUE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_PCREL8",
	 FALSE,
	 0xff,
	 0xff,
	 TRUE),
  /* Simple 16 pc-relative reloc.  */
  HOWTO (R_MN10200_PCREL16,
	 0,
	 1,
	 16,
	 TRUE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_PCREL16",
	 FALSE,
	 0xffff,
	 0xffff,
	 TRUE),
  /* Simple 32bit pc-relative reloc with a 1 byte adjustment
     to get the pc-relative offset correct.  */
  HOWTO (R_MN10200_PCREL24,
	 0,
	 2,
	 24,
	 TRUE,
	 0,
	 complain_overflow_bitfield,
	 bfd_elf_generic_reloc,
	 "R_MN10200_PCREL24",
	 FALSE,
	 0xffffff,
	 0xffffff,
	 TRUE),
};

struct mn10200_reloc_map {
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct mn10200_reloc_map mn10200_reloc_map[] = {
  { BFD_RELOC_NONE    , R_MN10200_NONE   , },
  { BFD_RELOC_32      , R_MN10200_32     , },
  { BFD_RELOC_16      , R_MN10200_16     , },
  { BFD_RELOC_8       , R_MN10200_8      , },
  { BFD_RELOC_24      , R_MN10200_24     , },
  { BFD_RELOC_8_PCREL , R_MN10200_PCREL8 , },
  { BFD_RELOC_16_PCREL, R_MN10200_PCREL16, },
  { BFD_RELOC_24_PCREL, R_MN10200_PCREL24, },
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (abfd, code)
     bfd *abfd ATTRIBUTE_UNUSED;
     bfd_reloc_code_real_type code;
{
  unsigned int i;

  for (i = 0;
       i < sizeof (mn10200_reloc_map) / sizeof (struct mn10200_reloc_map);
       i++)
    {
      if (mn10200_reloc_map[i].bfd_reloc_val == code)
	return &elf_mn10200_howto_table[mn10200_reloc_map[i].elf_reloc_val];
    }

  return NULL;
}

/* Set the howto pointer for an MN10200 ELF reloc.  */

static void
mn10200_info_to_howto (abfd, cache_ptr, dst)
     bfd *abfd ATTRIBUTE_UNUSED;
     arelent *cache_ptr;
     Elf_Internal_Rela *dst;
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MN10200_MAX);
  cache_ptr->howto = &elf_mn10200_howto_table[r_type];
}

/* Perform a relocation as part of a final link.  */

static bfd_reloc_status_type
mn10200_elf_final_link_relocate (howto, input_bfd, output_bfd,
				 input_section, contents, offset, value,
				 addend, info, sym_sec, is_local)
     reloc_howto_type *howto;
     bfd *input_bfd;
     bfd *output_bfd ATTRIBUTE_UNUSED;
     asection *input_section;
     bfd_byte *contents;
     bfd_vma offset;
     bfd_vma value;
     bfd_vma addend;
     struct bfd_link_info *info ATTRIBUTE_UNUSED;
     asection *sym_sec ATTRIBUTE_UNUSED;
     int is_local ATTRIBUTE_UNUSED;
{
  unsigned long r_type = howto->type;
  bfd_byte *hit_data = contents + offset;

  switch (r_type)
    {

    case R_MN10200_NONE:
      return bfd_reloc_ok;

    case R_MN10200_32:
      value += addend;
      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10200_16:
      value += addend;

      if ((long) value > 0x7fff || (long) value < -0x8000)
	return bfd_reloc_overflow;

      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10200_8:
      value += addend;

      if ((long) value > 0x7f || (long) value < -0x80)
	return bfd_reloc_overflow;

      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10200_24:
      value += addend;

      if ((long) value > 0x7fffff || (long) value < -0x800000)
	return bfd_reloc_overflow;

      value &= 0xffffff;
      value |= (bfd_get_32 (input_bfd, hit_data) & 0xff000000);
      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10200_PCREL8:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= (offset + 1);
      value += addend;

      if ((long) value > 0xff || (long) value < -0x100)
	return bfd_reloc_overflow;

      bfd_put_8 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10200_PCREL16:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= (offset + 2);
      value += addend;

      if ((long) value > 0xffff || (long) value < -0x10000)
	return bfd_reloc_overflow;

      bfd_put_16 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    case R_MN10200_PCREL24:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= (offset + 3);
      value += addend;

      if ((long) value > 0xffffff || (long) value < -0x1000000)
	return bfd_reloc_overflow;

      value &= 0xffffff;
      value |= (bfd_get_32 (input_bfd, hit_data) & 0xff000000);
      bfd_put_32 (input_bfd, value, hit_data);
      return bfd_reloc_ok;

    default:
      return bfd_reloc_notsupported;
    }
}

/* Relocate an MN10200 ELF section.  */
static bfd_boolean
mn10200_elf_relocate_section (output_bfd, info, input_bfd, input_section,
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
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel, *relend;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      Elf_Internal_Sym *sym;
      asection *sec;
      struct elf_link_hash_entry *h;
      bfd_vma relocation;
      bfd_reloc_status_type r;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);
      howto = elf_mn10200_howto_table + r_type;

      h = NULL;
      sym = NULL;
      sec = NULL;
      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	}
      else
	{
	  bfd_boolean unresolved_reloc, warned;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      r = mn10200_elf_final_link_relocate (howto, input_bfd, output_bfd,
					   input_section,
					   contents, rel->r_offset,
					   relocation, rel->r_addend,
					   info, sec, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  const char *name;
	  const char *msg = (const char *) 0;

	  if (h != NULL)
	    name = h->root.root.string;
	  else
	    {
	      name = (bfd_elf_string_from_elf_section
		      (input_bfd, symtab_hdr->sh_link, sym->st_name));
	      if (name == NULL || *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }

	  switch (r)
	    {
	    case bfd_reloc_overflow:
	      if (! ((*info->callbacks->reloc_overflow)
		     (info, (h ? &h->root : NULL), name, howto->name,
		      (bfd_vma) 0, input_bfd, input_section,
		      rel->r_offset)))
		return FALSE;
	      break;

	    case bfd_reloc_undefined:
	      if (! ((*info->callbacks->undefined_symbol)
		     (info, name, input_bfd, input_section,
		      rel->r_offset, TRUE)))
		return FALSE;
	      break;

	    case bfd_reloc_outofrange:
	      msg = _("internal error: out of range error");
	      goto common_error;

	    case bfd_reloc_notsupported:
	      msg = _("internal error: unsupported relocation error");
	      goto common_error;

	    case bfd_reloc_dangerous:
	      msg = _("internal error: dangerous error");
	      goto common_error;

	    default:
	      msg = _("internal error: unknown error");
	      /* fall through */

	    common_error:
	      if (!((*info->callbacks->warning)
		    (info, msg, name, input_bfd, input_section,
		     rel->r_offset)))
		return FALSE;
	      break;
	    }
	}
    }

  return TRUE;
}

/* This function handles relaxing for the mn10200.

   There are quite a few relaxing opportunities available on the mn10200:

	* jsr:24 -> jsr:16 					   2 bytes

	* jmp:24 -> jmp:16					   2 bytes
	* jmp:16 -> bra:8					   1 byte

		* If the previous instruction is a conditional branch
		around the jump/bra, we may be able to reverse its condition
		and change its target to the jump's target.  The jump/bra
		can then be deleted.				   2 bytes

	* mov abs24 -> mov abs16	2 byte savings

	* Most instructions which accept imm24 can relax to imm16  2 bytes
	- Most instructions which accept imm16 can relax to imm8   1 byte

	* Most instructions which accept d24 can relax to d16	   2 bytes
	- Most instructions which accept d16 can relax to d8	   1 byte

	abs24, imm24, d24 all look the same at the reloc level.  It
	might make the code simpler if we had different relocs for
	the various relaxable operand types.

	We don't handle imm16->imm8 or d16->d8 as they're very rare
	and somewhat more difficult to support.  */

static bfd_boolean
mn10200_elf_relax_section (abfd, sec, link_info, again)
     bfd *abfd;
     asection *sec;
     struct bfd_link_info *link_info;
     bfd_boolean *again;
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel, *irelend;
  bfd_byte *contents = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* Assume nothing changes.  */
  *again = FALSE;

  /* We don't have to do anything for a relocatable link, if
     this section does not have relocs, or if this is not a
     code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, sec, (PTR) NULL, (Elf_Internal_Rela *) NULL,
		      link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  /* Walk through them looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;

      /* If this isn't something that can be relaxed, then ignore
	 this reloc.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_NONE
	  || ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_8
	  || ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_MAX)
	continue;

      /* Get the section contents if we haven't done so already.  */
      if (contents == NULL)
	{
	  /* Get cached copy if it exists.  */
	  if (elf_section_data (sec)->this_hdr.contents != NULL)
	    contents = elf_section_data (sec)->this_hdr.contents;
	  else
	    {
	      /* Go get them off disk.  */
	      if (!bfd_malloc_and_get_section (abfd, sec, &contents))
		goto error_return;
	    }
	}

      /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL && symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;
	  asection *sym_sec;

	  isym = isymbuf + ELF32_R_SYM (irel->r_info);
	  if (isym->st_shndx == SHN_UNDEF)
	    sym_sec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    sym_sec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    sym_sec = bfd_com_section_ptr;
	  else
	    sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
	  symval = (isym->st_value
		    + sym_sec->output_section->vma
		    + sym_sec->output_offset);
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry *h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];
	  BFD_ASSERT (h != NULL);
	  if (h->root.type != bfd_link_hash_defined
	      && h->root.type != bfd_link_hash_defweak)
	    {
	      /* This appears to be a reference to an undefined
                 symbol.  Just ignore it--it will be caught by the
                 regular reloc processing.  */
	      continue;
	    }

	  symval = (h->root.u.def.value
		    + h->root.u.def.section->output_section->vma
		    + h->root.u.def.section->output_offset);
	}

      /* For simplicity of coding, we are going to modify the section
	 contents, the section relocs, and the BFD symbol table.  We
	 must tell the rest of the code not to free up this
	 information.  It would be possible to instead create a table
	 of changes which have to be made, as is done in coff-mips.c;
	 that would be more work, but would require less memory when
	 the linker is run.  */

      /* Try to turn a 24bit pc-relative branch/call into a 16bit pc-relative
	 branch/call.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_PCREL24)
	{
	  bfd_vma value = symval;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= (irel->r_offset + 3);
	  value += irel->r_addend;

	  /* See if the value will fit in 16 bits, note the high value is
	     0x7fff + 2 as the target will be two bytes closer if we are
	     able to relax.  */
	  if ((long) value < 0x8001 && (long) value > -0x8000)
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      if (code != 0xe0 && code != 0xe1)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the opcode.  */
	      if (code == 0xe0)
		bfd_put_8 (abfd, 0xfc, contents + irel->r_offset - 2);
	      else if (code == 0xe1)
		bfd_put_8 (abfd, 0xfd, contents + irel->r_offset - 2);

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_MN10200_PCREL16);

	      /* The opcode got shorter too, so we have to fix the offset.  */
	      irel->r_offset -= 1;

	      /* Delete two bytes of data.  */
	      if (!mn10200_elf_relax_delete_bytes (abfd, sec,
						   irel->r_offset + 1, 2))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}

      /* Try to turn a 16bit pc-relative branch into a 8bit pc-relative
	 branch.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_PCREL16)
	{
	  bfd_vma value = symval;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= (irel->r_offset + 2);
	  value += irel->r_addend;

	  /* See if the value will fit in 8 bits, note the high value is
	     0x7f + 1 as the target will be one bytes closer if we are
	     able to relax.  */
	  if ((long) value < 0x80 && (long) value > -0x80)
	    {
	      unsigned char code;

	      /* Get the opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      if (code != 0xfc)
		continue;

	      /* Note that we've changed the relocs, section contents, etc.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (unsigned char *) isymbuf;

	      /* Fix the opcode.  */
	      bfd_put_8 (abfd, 0xea, contents + irel->r_offset - 1);

	      /* Fix the relocation's type.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					   R_MN10200_PCREL8);

	      /* Delete one byte of data.  */
	      if (!mn10200_elf_relax_delete_bytes (abfd, sec,
						   irel->r_offset + 1, 1))
		goto error_return;

	      /* That will change things, so, we should relax again.
		 Note that this is not required, and it may be slow.  */
	      *again = TRUE;
	    }
	}

      /* Try to eliminate an unconditional 8 bit pc-relative branch
	 which immediately follows a conditional 8 bit pc-relative
	 branch around the unconditional branch.

	    original:		new:
	    bCC lab1		bCC' lab2
	    bra lab2
	   lab1:	       lab1:

	 This happens when the bCC can't reach lab2 at assembly time,
	 but due to other relaxations it can reach at link time.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_PCREL8)
	{
	  Elf_Internal_Rela *nrel;
	  bfd_vma value = symval;
	  unsigned char code;

	  /* Deal with pc-relative gunk.  */
	  value -= (sec->output_section->vma + sec->output_offset);
	  value -= (irel->r_offset + 1);
	  value += irel->r_addend;

	  /* Do nothing if this reloc is the last byte in the section.  */
	  if (irel->r_offset == sec->size)
	    continue;

	  /* See if the next instruction is an unconditional pc-relative
	     branch, more often than not this test will fail, so we
	     test it first to speed things up.  */
	  code = bfd_get_8 (abfd, contents + irel->r_offset + 1);
	  if (code != 0xea)
	    continue;

	  /* Also make sure the next relocation applies to the next
	     instruction and that it's a pc-relative 8 bit branch.  */
	  nrel = irel + 1;
	  if (nrel == irelend
	      || irel->r_offset + 2 != nrel->r_offset
	      || ELF32_R_TYPE (nrel->r_info) != (int) R_MN10200_PCREL8)
	    continue;

	  /* Make sure our destination immediately follows the
	     unconditional branch.  */
	  if (symval != (sec->output_section->vma + sec->output_offset
			 + irel->r_offset + 3))
	    continue;

	  /* Now make sure we are a conditional branch.  This may not
	     be necessary, but why take the chance.

	     Note these checks assume that R_MN10200_PCREL8 relocs
	     only occur on bCC and bCCx insns.  If they occured
	     elsewhere, we'd need to know the start of this insn
	     for this check to be accurate.  */
	  code = bfd_get_8 (abfd, contents + irel->r_offset - 1);
	  if (code != 0xe0 && code != 0xe1 && code != 0xe2
	      && code != 0xe3 && code != 0xe4 && code != 0xe5
	      && code != 0xe6 && code != 0xe7 && code != 0xe8
	      && code != 0xe9 && code != 0xec && code != 0xed
	      && code != 0xee && code != 0xef && code != 0xfc
	      && code != 0xfd && code != 0xfe && code != 0xff)
	    continue;

	  /* We also have to be sure there is no symbol/label
	     at the unconditional branch.  */
	  if (mn10200_elf_symbol_address_p (abfd, sec, isymbuf,
					    irel->r_offset + 1))
	    continue;

	  /* Note that we've changed the relocs, section contents, etc.  */
	  elf_section_data (sec)->relocs = internal_relocs;
	  elf_section_data (sec)->this_hdr.contents = contents;
	  symtab_hdr->contents = (unsigned char *) isymbuf;

	  /* Reverse the condition of the first branch.  */
	  switch (code)
	    {
	    case 0xfc:
	      code = 0xfd;
	      break;
	    case 0xfd:
	      code = 0xfc;
	      break;
	    case 0xfe:
	      code = 0xff;
	      break;
	    case 0xff:
	      code = 0xfe;
	      break;
	    case 0xe8:
	      code = 0xe9;
	      break;
	    case 0xe9:
	      code = 0xe8;
	      break;
	    case 0xe0:
	      code = 0xe2;
	      break;
	    case 0xe2:
	      code = 0xe0;
	      break;
	    case 0xe3:
	      code = 0xe1;
	      break;
	    case 0xe1:
	      code = 0xe3;
	      break;
	    case 0xe4:
	      code = 0xe6;
	      break;
	    case 0xe6:
	      code = 0xe4;
	      break;
	    case 0xe7:
	      code = 0xe5;
	      break;
	    case 0xe5:
	      code = 0xe7;
	      break;
	    case 0xec:
	      code = 0xed;
	      break;
	    case 0xed:
	      code = 0xec;
	      break;
	    case 0xee:
	      code = 0xef;
	      break;
	    case 0xef:
	      code = 0xee;
	      break;
	    }
	  bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

	  /* Set the reloc type and symbol for the first branch
	     from the second branch.  */
	  irel->r_info = nrel->r_info;

	  /* Make the reloc for the second branch a null reloc.  */
	  nrel->r_info = ELF32_R_INFO (ELF32_R_SYM (nrel->r_info),
				       R_MN10200_NONE);

	  /* Delete two bytes of data.  */
	  if (!mn10200_elf_relax_delete_bytes (abfd, sec,
					       irel->r_offset + 1, 2))
	    goto error_return;

	  /* That will change things, so, we should relax again.
	     Note that this is not required, and it may be slow.  */
	  *again = TRUE;
	}

      /* Try to turn a 24bit immediate, displacement or absolute address
	 into a 16bit immediate, displacement or absolute address.  */
      if (ELF32_R_TYPE (irel->r_info) == (int) R_MN10200_24)
	{
	  bfd_vma value = symval;

	  /* See if the value will fit in 16 bits.
	     We allow any 16bit match here.  We prune those we can't
	     handle below.  */
	  if ((long) value < 0x7fff && (long) value > -0x8000)
	    {
	      unsigned char code;

	      /* All insns which have 24bit operands are 5 bytes long,
		 the first byte will always be 0xf4, but we double check
		 it just in case.  */

	      /* Get the first opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 2);

	      if (code != 0xf4)
		continue;

	      /* Get the second opcode.  */
	      code = bfd_get_8 (abfd, contents + irel->r_offset - 1);

	      switch (code & 0xfc)
		{
		/* mov imm24,dn -> mov imm16,dn */
		case 0x70:
		  /* Not safe if the high bit is on as relaxing may
		     move the value out of high mem and thus not fit
		     in a signed 16bit value.  */
		  if (value & 0x8000)
		    continue;

		  /* Note that we've changed the relocation contents, etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  elf_section_data (sec)->this_hdr.contents = contents;
		  symtab_hdr->contents = (unsigned char *) isymbuf;

		  /* Fix the opcode.  */
		  bfd_put_8 (abfd, 0xf8 + (code & 0x03),
			     contents + irel->r_offset - 2);

		  /* Fix the relocation's type.  */
		  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					       R_MN10200_16);

		  /* The opcode got shorter too, so we have to fix the
		     offset.  */
		  irel->r_offset -= 1;

		  /* Delete two bytes of data.  */
		  if (!mn10200_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 1, 2))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = TRUE;
		  break;

		/* mov imm24,an -> mov imm16,an
		   cmp imm24,an -> cmp imm16,an
		   mov (abs24),dn -> mov (abs16),dn
		   mov dn,(abs24) -> mov dn,(abs16)
		   movb dn,(abs24) -> movb dn,(abs16)
		   movbu (abs24),dn -> movbu (abs16),dn */
		case 0x74:
		case 0x7c:
		case 0xc0:
		case 0x40:
		case 0x44:
		case 0xc8:
		  /* Note that we've changed the relocation contents, etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  elf_section_data (sec)->this_hdr.contents = contents;
		  symtab_hdr->contents = (unsigned char *) isymbuf;

		  if ((code & 0xfc) == 0x74)
		    code = 0xdc + (code & 0x03);
		  else if ((code & 0xfc) == 0x7c)
		    code = 0xec + (code & 0x03);
		  else if ((code & 0xfc) == 0xc0)
		    code = 0xc8 + (code & 0x03);
		  else if ((code & 0xfc) == 0x40)
		    code = 0xc0 + (code & 0x03);
		  else if ((code & 0xfc) == 0x44)
		    code = 0xc4 + (code & 0x03);
		  else if ((code & 0xfc) == 0xc8)
		    code = 0xcc + (code & 0x03);

		  /* Fix the opcode.  */
		  bfd_put_8 (abfd, code, contents + irel->r_offset - 2);

		  /* Fix the relocation's type.  */
		  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					       R_MN10200_16);

		  /* The opcode got shorter too, so we have to fix the
		     offset.  */
		  irel->r_offset -= 1;

		  /* Delete two bytes of data.  */
		  if (!mn10200_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 1, 2))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = TRUE;
		  break;

		/* cmp imm24,dn -> cmp imm16,dn
		   mov (abs24),an -> mov (abs16),an
		   mov an,(abs24) -> mov an,(abs16)
		   add imm24,dn -> add imm16,dn
		   add imm24,an -> add imm16,an
		   sub imm24,dn -> sub imm16,dn
		   sub imm24,an -> sub imm16,an
		   And all d24->d16 in memory ops.  */
		case 0x78:
		case 0xd0:
		case 0x50:
		case 0x60:
		case 0x64:
		case 0x68:
		case 0x6c:
		case 0x80:
		case 0xf0:
		case 0x00:
		case 0x10:
		case 0xb0:
		case 0x30:
		case 0xa0:
		case 0x20:
		case 0x90:
		  /* Not safe if the high bit is on as relaxing may
		     move the value out of high mem and thus not fit
		     in a signed 16bit value.  */
		  if (((code & 0xfc) == 0x78
		       || (code & 0xfc) == 0x60
		       || (code & 0xfc) == 0x64
		       || (code & 0xfc) == 0x68
		       || (code & 0xfc) == 0x6c
		       || (code & 0xfc) == 0x80
		       || (code & 0xfc) == 0xf0
		       || (code & 0xfc) == 0x00
		       || (code & 0xfc) == 0x10
		       || (code & 0xfc) == 0xb0
		       || (code & 0xfc) == 0x30
		       || (code & 0xfc) == 0xa0
		       || (code & 0xfc) == 0x20
		       || (code & 0xfc) == 0x90)
		      && (value & 0x8000) != 0)
		    continue;

		  /* Note that we've changed the relocation contents, etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  elf_section_data (sec)->this_hdr.contents = contents;
		  symtab_hdr->contents = (unsigned char *) isymbuf;

		  /* Fix the opcode.  */
		  bfd_put_8 (abfd, 0xf7, contents + irel->r_offset - 2);

		  if ((code & 0xfc) == 0x78)
		    code = 0x48 + (code & 0x03);
		  else if ((code & 0xfc) == 0xd0)
		    code = 0x30 + (code & 0x03);
		  else if ((code & 0xfc) == 0x50)
		    code = 0x20 + (code & 0x03);
		  else if ((code & 0xfc) == 0x60)
		    code = 0x18 + (code & 0x03);
		  else if ((code & 0xfc) == 0x64)
		    code = 0x08 + (code & 0x03);
		  else if ((code & 0xfc) == 0x68)
		    code = 0x1c + (code & 0x03);
		  else if ((code & 0xfc) == 0x6c)
		    code = 0x0c + (code & 0x03);
		  else if ((code & 0xfc) == 0x80)
		    code = 0xc0 + (code & 0x07);
		  else if ((code & 0xfc) == 0xf0)
		    code = 0xb0 + (code & 0x07);
		  else if ((code & 0xfc) == 0x00)
		    code = 0x80 + (code & 0x07);
		  else if ((code & 0xfc) == 0x10)
		    code = 0xa0 + (code & 0x07);
		  else if ((code & 0xfc) == 0xb0)
		    code = 0x70 + (code & 0x07);
		  else if ((code & 0xfc) == 0x30)
		    code = 0x60 + (code & 0x07);
		  else if ((code & 0xfc) == 0xa0)
		    code = 0xd0 + (code & 0x07);
		  else if ((code & 0xfc) == 0x20)
		    code = 0x90 + (code & 0x07);
		  else if ((code & 0xfc) == 0x90)
		    code = 0x50 + (code & 0x07);

		  bfd_put_8 (abfd, code, contents + irel->r_offset - 1);

		  /* Fix the relocation's type.  */
		  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					       R_MN10200_16);

		  /* Delete one bytes of data.  */
		  if (!mn10200_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 2, 1))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = TRUE;
		  break;

		/* movb (abs24),dn ->movbu (abs16),dn extxb bn */
		case 0xc4:
		  /* Note that we've changed the reldection contents, etc.  */
		  elf_section_data (sec)->relocs = internal_relocs;
		  elf_section_data (sec)->this_hdr.contents = contents;
		  symtab_hdr->contents = (unsigned char *) isymbuf;

		  bfd_put_8 (abfd, 0xcc + (code & 0x03),
			     contents + irel->r_offset - 2);

		  bfd_put_8 (abfd, 0xb8 + (code & 0x03),
			     contents + irel->r_offset - 1);

		  /* Fix the relocation's type.  */
		  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
					       R_MN10200_16);

		  /* The reloc will be applied one byte in front of its
		     current location.  */
		  irel->r_offset -= 1;

		  /* Delete one bytes of data.  */
		  if (!mn10200_elf_relax_delete_bytes (abfd, sec,
						       irel->r_offset + 2, 1))
		    goto error_return;

		  /* That will change things, so, we should relax again.
		     Note that this is not required, and it may be slow.  */
		  *again = TRUE;
		  break;
		}
	    }
	}
    }

  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    {
      if (! link_info->keep_memory)
	free (isymbuf);
      else
	{
	  /* Cache the symbols for elf_link_input_bfd.  */
	  symtab_hdr->contents = (unsigned char *) isymbuf;
	}
    }

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    {
      if (! link_info->keep_memory)
	free (contents);
      else
	{
	  /* Cache the section contents for elf_link_input_bfd.  */
	  elf_section_data (sec)->this_hdr.contents = contents;
	}
    }

  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return TRUE;

 error_return:
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  return FALSE;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
mn10200_elf_relax_delete_bytes (abfd, sec, addr, count)
     bfd *abfd;
     asection *sec;
     bfd_vma addr;
     int count;
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Rela *irelalign;
  bfd_vma toaddr;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an aligment
     power larger than the number of bytes we are deleting.  */

  irelalign = NULL;
  toaddr = sec->size;

  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;

  /* Actually delete the bytes.  */
  memmove (contents + addr, contents + addr + count,
	   (size_t) (toaddr - addr - count));
  sec->size -= count;

  /* Adjust all the relocs.  */
  for (irel = elf_section_data (sec)->relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if ((irel->r_offset > addr
	   && irel->r_offset < toaddr))
	irel->r_offset -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isym = (Elf_Internal_Sym *) symtab_hdr->contents;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
	  && isym->st_value > addr
	  && isym->st_value < toaddr)
	isym->st_value -= count;
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value > addr
	  && sym_hash->root.u.def.value < toaddr)
	{
	  sym_hash->root.u.def.value -= count;
	}
    }

  return TRUE;
}

/* Return TRUE if a symbol exists at the given address, else return
   FALSE.  */
static bfd_boolean
mn10200_elf_symbol_address_p (abfd, sec, isym, addr)
     bfd *abfd;
     asection *sec;
     Elf_Internal_Sym *isym;
     bfd_vma addr;
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  Elf_Internal_Sym *isymend;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  /* Examine all the local symbols.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  for (isymend = isym + symtab_hdr->sh_info; isym < isymend; isym++)
    {
      if (isym->st_shndx == sec_shndx
	  && isym->st_value == addr)
	return TRUE;
    }

  symcount = (symtab_hdr->sh_size / sizeof (Elf32_External_Sym)
	      - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec
	  && sym_hash->root.u.def.value == addr)
	return TRUE;
    }

  return FALSE;
}

/* This is a version of bfd_generic_get_relocated_section_contents
   which uses mn10200_elf_relocate_section.  */

static bfd_byte *
mn10200_elf_get_relocated_section_contents (output_bfd, link_info, link_order,
					    data, relocatable, symbols)
     bfd *output_bfd;
     struct bfd_link_info *link_info;
     struct bfd_link_order *link_order;
     bfd_byte *data;
     bfd_boolean relocatable;
     asymbol **symbols;
{
  Elf_Internal_Shdr *symtab_hdr;
  asection *input_section = link_order->u.indirect.section;
  bfd *input_bfd = input_section->owner;
  asection **sections = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  Elf_Internal_Sym *isymbuf = NULL;

  /* We only need to handle the case of relaxing, or of having a
     particular set of section contents, specially.  */
  if (relocatable
      || elf_section_data (input_section)->this_hdr.contents == NULL)
    return bfd_generic_get_relocated_section_contents (output_bfd, link_info,
						       link_order, data,
						       relocatable,
						       symbols);

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;

  memcpy (data, elf_section_data (input_section)->this_hdr.contents,
	  (size_t) input_section->size);

  if ((input_section->flags & SEC_RELOC) != 0
      && input_section->reloc_count > 0)
    {
      Elf_Internal_Sym *isym;
      Elf_Internal_Sym *isymend;
      asection **secpp;
      bfd_size_type amt;

      internal_relocs = (_bfd_elf_link_read_relocs
			 (input_bfd, input_section, (PTR) NULL,
			  (Elf_Internal_Rela *) NULL, FALSE));
      if (internal_relocs == NULL)
	goto error_return;

      if (symtab_hdr->sh_info != 0)
	{
	  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	  if (isymbuf == NULL)
	    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr,
					    symtab_hdr->sh_info, 0,
					    NULL, NULL, NULL);
	  if (isymbuf == NULL)
	    goto error_return;
	}

      amt = symtab_hdr->sh_info;
      amt *= sizeof (asection *);
      sections = (asection **) bfd_malloc (amt);
      if (sections == NULL && amt != 0)
	goto error_return;

      isymend = isymbuf + symtab_hdr->sh_info;
      for (isym = isymbuf, secpp = sections; isym < isymend; ++isym, ++secpp)
	{
	  asection *isec;

	  if (isym->st_shndx == SHN_UNDEF)
	    isec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    isec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    isec = bfd_com_section_ptr;
	  else
	    isec = bfd_section_from_elf_index (input_bfd, isym->st_shndx);

	  *secpp = isec;
	}

      if (! mn10200_elf_relocate_section (output_bfd, link_info, input_bfd,
				     input_section, data, internal_relocs,
				     isymbuf, sections))
	goto error_return;

      if (sections != NULL)
	free (sections);
      if (isymbuf != NULL
	  && symtab_hdr->contents != (unsigned char *) isymbuf)
	free (isymbuf);
      if (elf_section_data (input_section)->relocs != internal_relocs)
	free (internal_relocs);
    }

  return data;

 error_return:
  if (sections != NULL)
    free (sections);
  if (isymbuf != NULL
      && symtab_hdr->contents != (unsigned char *) isymbuf)
    free (isymbuf);
  if (internal_relocs != NULL
      && elf_section_data (input_section)->relocs != internal_relocs)
    free (internal_relocs);
  return NULL;
}

#define TARGET_LITTLE_SYM	bfd_elf32_mn10200_vec
#define TARGET_LITTLE_NAME	"elf32-mn10200"
#define ELF_ARCH		bfd_arch_mn10200
#define ELF_MACHINE_CODE	EM_MN10200
#define ELF_MACHINE_ALT1	EM_CYGNUS_MN10200
#define ELF_MAXPAGESIZE		0x1000

#define elf_backend_rela_normal 1
#define elf_info_to_howto	mn10200_info_to_howto
#define elf_info_to_howto_rel	0
#define elf_backend_relocate_section mn10200_elf_relocate_section
#define bfd_elf32_bfd_relax_section	mn10200_elf_relax_section
#define bfd_elf32_bfd_get_relocated_section_contents \
				mn10200_elf_get_relocated_section_contents

#define elf_symbol_leading_char '_'

#include "elf32-target.h"
