/* V850-specific support for 32-bit ELF
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* XXX FIXME: This code is littered with 32bit int, 16bit short, 8bit char
   dependencies.  As is the gas & simulator code for the v850.  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/v850.h"
#include "libiberty.h"

/* Sign-extend a 24-bit number.  */
#define SEXT24(x)	((((x) & 0xffffff) ^ 0x800000) - 0x800000)

/* Look through the relocs for a section during the first phase, and
   allocate space in the global offset table or procedure linkage
   table.  */

static bfd_boolean
v850_elf_check_relocs (bfd *abfd,
		       struct bfd_link_info *info,
		       asection *sec,
		       const Elf_Internal_Rela *relocs)
{
  bfd_boolean ret = TRUE;
  bfd *dynobj;
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;
  asection *sreloc;
  enum v850_reloc_type r_type;
  int other = 0;
  const char *common = NULL;

  if (info->relocatable)
    return TRUE;

#ifdef DEBUG
  _bfd_error_handler ("v850_elf_check_relocs called for section %A in %B",
		      sec, abfd);
#endif

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  sreloc = NULL;

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      r_type = (enum v850_reloc_type) ELF32_R_TYPE (rel->r_info);
      switch (r_type)
	{
	default:
	case R_V850_NONE:
	case R_V850_9_PCREL:
	case R_V850_22_PCREL:
	case R_V850_HI16_S:
	case R_V850_HI16:
	case R_V850_LO16:
	case R_V850_LO16_SPLIT_OFFSET:
	case R_V850_ABS32:
	case R_V850_REL32:
	case R_V850_16:
	case R_V850_8:
	case R_V850_CALLT_6_7_OFFSET:
	case R_V850_CALLT_16_16_OFFSET:
	  break;

        /* This relocation describes the C++ object vtable hierarchy.
           Reconstruct it for later use during GC.  */
        case R_V850_GNU_VTINHERIT:
          if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
            return FALSE;
          break;

        /* This relocation describes which C++ vtable entries
	   are actually used.  Record for later use during GC.  */
        case R_V850_GNU_VTENTRY:
          if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
            return FALSE;
          break;

	case R_V850_SDA_16_16_SPLIT_OFFSET:
	case R_V850_SDA_16_16_OFFSET:
	case R_V850_SDA_15_16_OFFSET:
	  other = V850_OTHER_SDA;
	  common = ".scommon";
	  goto small_data_common;

	case R_V850_ZDA_16_16_SPLIT_OFFSET:
	case R_V850_ZDA_16_16_OFFSET:
	case R_V850_ZDA_15_16_OFFSET:
	  other = V850_OTHER_ZDA;
	  common = ".zcommon";
	  goto small_data_common;

	case R_V850_TDA_4_5_OFFSET:
	case R_V850_TDA_4_4_OFFSET:
	case R_V850_TDA_6_8_OFFSET:
	case R_V850_TDA_7_8_OFFSET:
	case R_V850_TDA_7_7_OFFSET:
	case R_V850_TDA_16_16_OFFSET:
	  other = V850_OTHER_TDA;
	  common = ".tcommon";
	  /* fall through */

#define V850_OTHER_MASK (V850_OTHER_TDA | V850_OTHER_SDA | V850_OTHER_ZDA)

	small_data_common:
	  if (h)
	    {
	      /* Flag which type of relocation was used.  */
	      h->other |= other;
	      if ((h->other & V850_OTHER_MASK) != (other & V850_OTHER_MASK)
		  && (h->other & V850_OTHER_ERROR) == 0)
		{
		  const char * msg;
		  static char  buff[200]; /* XXX */

		  switch (h->other & V850_OTHER_MASK)
		    {
		    default:
		      msg = _("Variable `%s' cannot occupy in multiple small data regions");
		      break;
		    case V850_OTHER_SDA | V850_OTHER_ZDA | V850_OTHER_TDA:
		      msg = _("Variable `%s' can only be in one of the small, zero, and tiny data regions");
		      break;
		    case V850_OTHER_SDA | V850_OTHER_ZDA:
		      msg = _("Variable `%s' cannot be in both small and zero data regions simultaneously");
		      break;
		    case V850_OTHER_SDA | V850_OTHER_TDA:
		      msg = _("Variable `%s' cannot be in both small and tiny data regions simultaneously");
		      break;
		    case V850_OTHER_ZDA | V850_OTHER_TDA:
		      msg = _("Variable `%s' cannot be in both zero and tiny data regions simultaneously");
		      break;
		    }

		  sprintf (buff, msg, h->root.root.string);
		  info->callbacks->warning (info, buff, h->root.root.string,
					    abfd, h->root.u.def.section,
					    (bfd_vma) 0);

		  bfd_set_error (bfd_error_bad_value);
		  h->other |= V850_OTHER_ERROR;
		  ret = FALSE;
		}
	    }

	  if (h && h->root.type == bfd_link_hash_common
	      && h->root.u.c.p
	      && !strcmp (bfd_get_section_name (abfd, h->root.u.c.p->section), "COMMON"))
	    {
	      asection * section;

	      section = h->root.u.c.p->section = bfd_make_section_old_way (abfd, common);
	      section->flags |= SEC_IS_COMMON;
	    }

#ifdef DEBUG
	  fprintf (stderr, "v850_elf_check_relocs, found %s relocation for %s%s\n",
		   v850_elf_howto_table[ (int)r_type ].name,
		   (h && h->root.root.string) ? h->root.root.string : "<unknown>",
		   (h->root.type == bfd_link_hash_common) ? ", symbol is common" : "");
#endif
	  break;
	}
    }

  return ret;
}

/* In the old version, when an entry was checked out from the table,
   it was deleted.  This produced an error if the entry was needed
   more than once, as the second attempted retry failed.

   In the current version, the entry is not deleted, instead we set
   the field 'found' to TRUE.  If a second lookup matches the same
   entry, then we know that the hi16s reloc has already been updated
   and does not need to be updated a second time.

   TODO - TOFIX: If it is possible that we need to restore 2 different
   addresses from the same table entry, where the first generates an
   overflow, whilst the second do not, then this code will fail.  */

typedef struct hi16s_location
{
  bfd_vma                 addend;
  bfd_byte *              address;
  unsigned long           counter;
  bfd_boolean             found;
  struct hi16s_location * next;
}
hi16s_location;

static hi16s_location * previous_hi16s;
static hi16s_location * free_hi16s;
static unsigned long    hi16s_counter;

static void
remember_hi16s_reloc (bfd *abfd, bfd_vma addend, bfd_byte *address)
{
  hi16s_location * entry = NULL;
  bfd_size_type amt = sizeof (* free_hi16s);

  /* Find a free structure.  */
  if (free_hi16s == NULL)
    free_hi16s = bfd_zalloc (abfd, amt);

  entry      = free_hi16s;
  free_hi16s = free_hi16s->next;

  entry->addend  = addend;
  entry->address = address;
  entry->counter = hi16s_counter ++;
  entry->found   = FALSE;
  entry->next    = previous_hi16s;
  previous_hi16s = entry;

  /* Cope with wrap around of our counter.  */
  if (hi16s_counter == 0)
    {
      /* XXX: Assume that all counter entries differ only in their low 16 bits.  */
      for (entry = previous_hi16s; entry != NULL; entry = entry->next)
	entry->counter &= 0xffff;

      hi16s_counter = 0x10000;
    }
}

static bfd_byte *
find_remembered_hi16s_reloc (bfd_vma addend, bfd_boolean *already_found)
{
  hi16s_location *match = NULL;
  hi16s_location *entry;
  hi16s_location *previous = NULL;
  hi16s_location *prev;
  bfd_byte *addr;

  /* Search the table.  Record the most recent entry that matches.  */
  for (entry = previous_hi16s; entry; entry = entry->next)
    {
      if (entry->addend == addend
	  && (match == NULL || match->counter < entry->counter))
	{
	  previous = prev;
	  match    = entry;
	}

      prev = entry;
    }

  if (match == NULL)
    return NULL;

  /* Extract the address.  */
  addr = match->address;

  /* Remember if this entry has already been used before.  */
  if (already_found)
    * already_found = match->found;

  /* Note that this entry has now been used.  */
  match->found = TRUE;

  return addr;
}

/* Calculate the final operand value for a R_V850_LO16 or
   R_V850_LO16_SPLIT_OFFSET.  *INSN is the current operand value and
   ADDEND is the sum of the relocation symbol and offset.  Store the
   operand value in *INSN and return true on success.

   The assembler has already done some of this: If the value stored in
   the instruction has its 15th bit set, (counting from zero) then the
   assembler will have added 1 to the value stored in the associated
   HI16S reloc.  So for example, these relocations:

       movhi hi( fred ), r0, r1
       movea lo( fred ), r1, r1

   will store 0 in the value fields for the MOVHI and MOVEA instructions
   and addend will be the address of fred, but for these instructions:

       movhi hi( fred + 0x123456), r0, r1
       movea lo( fred + 0x123456), r1, r1

   the value stored in the MOVHI instruction will be 0x12 and the value
   stored in the MOVEA instruction will be 0x3456.  If however the
   instructions were:

       movhi hi( fred + 0x10ffff), r0, r1
       movea lo( fred + 0x10ffff), r1, r1

   then the value stored in the MOVHI instruction would be 0x11 (not
   0x10) and the value stored in the MOVEA instruction would be 0xffff.
   Thus (assuming for the moment that the addend is 0), at run time the
   MOVHI instruction loads 0x110000 into r1, then the MOVEA instruction
   adds 0xffffffff (sign extension!) producing 0x10ffff.  Similarly if
   the instructions were:

       movhi hi( fred - 1), r0, r1
       movea lo( fred - 1), r1, r1

   then 0 is stored in the MOVHI instruction and -1 is stored in the
   MOVEA instruction.

   Overflow can occur if the addition of the value stored in the
   instruction plus the addend sets the 15th bit when before it was clear.
   This is because the 15th bit will be sign extended into the high part,
   thus reducing its value by one, but since the 15th bit was originally
   clear, the assembler will not have added 1 to the previous HI16S reloc
   to compensate for this effect.  For example:

      movhi hi( fred + 0x123456), r0, r1
      movea lo( fred + 0x123456), r1, r1

   The value stored in HI16S reloc is 0x12, the value stored in the LO16
   reloc is 0x3456.  If we assume that the address of fred is 0x00007000
   then the relocations become:

     HI16S: 0x0012 + (0x00007000 >> 16)    = 0x12
     LO16:  0x3456 + (0x00007000 & 0xffff) = 0xa456

   but when the instructions are executed, the MOVEA instruction's value
   is signed extended, so the sum becomes:

	0x00120000
      + 0xffffa456
      ------------
	0x0011a456    but 'fred + 0x123456' = 0x0012a456

   Note that if the 15th bit was set in the value stored in the LO16
   reloc, then we do not have to do anything:

      movhi hi( fred + 0x10ffff), r0, r1
      movea lo( fred + 0x10ffff), r1, r1

      HI16S:  0x0011 + (0x00007000 >> 16)    = 0x11
      LO16:   0xffff + (0x00007000 & 0xffff) = 0x6fff

	0x00110000
      + 0x00006fff
      ------------
	0x00116fff  = fred + 0x10ffff = 0x7000 + 0x10ffff

   Overflow can also occur if the computation carries into the 16th bit
   and it also results in the 15th bit having the same value as the 15th
   bit of the original value.   What happens is that the HI16S reloc
   will have already examined the 15th bit of the original value and
   added 1 to the high part if the bit is set.  This compensates for the
   sign extension of 15th bit of the result of the computation.  But now
   there is a carry into the 16th bit, and this has not been allowed for.

   So, for example if fred is at address 0xf000:

     movhi hi( fred + 0xffff), r0, r1    [bit 15 of the offset is set]
     movea lo( fred + 0xffff), r1, r1

     HI16S: 0x0001 + (0x0000f000 >> 16)    = 0x0001
     LO16:  0xffff + (0x0000f000 & 0xffff) = 0xefff   (carry into bit 16 is lost)

       0x00010000
     + 0xffffefff
     ------------
       0x0000efff   but 'fred + 0xffff' = 0x0001efff

   Similarly, if the 15th bit remains clear, but overflow occurs into
   the 16th bit then (assuming the address of fred is 0xf000):

     movhi hi( fred + 0x7000), r0, r1    [bit 15 of the offset is clear]
     movea lo( fred + 0x7000), r1, r1

     HI16S: 0x0000 + (0x0000f000 >> 16)    = 0x0000
     LO16:  0x7000 + (0x0000f000 & 0xffff) = 0x6fff  (carry into bit 16 is lost)

       0x00000000
     + 0x00006fff
     ------------
       0x00006fff   but 'fred + 0x7000' = 0x00016fff

   Note - there is no need to change anything if a carry occurs, and the
   15th bit changes its value from being set to being clear, as the HI16S
   reloc will have already added in 1 to the high part for us:

     movhi hi( fred + 0xffff), r0, r1     [bit 15 of the offset is set]
     movea lo( fred + 0xffff), r1, r1

     HI16S: 0x0001 + (0x00007000 >> 16)
     LO16:  0xffff + (0x00007000 & 0xffff) = 0x6fff  (carry into bit 16 is lost)

       0x00010000
     + 0x00006fff   (bit 15 not set, so the top half is zero)
     ------------
       0x00016fff   which is right (assuming that fred is at 0x7000)

   but if the 15th bit goes from being clear to being set, then we must
   once again handle overflow:

     movhi hi( fred + 0x7000), r0, r1     [bit 15 of the offset is clear]
     movea lo( fred + 0x7000), r1, r1

     HI16S: 0x0000 + (0x0000ffff >> 16)
     LO16:  0x7000 + (0x0000ffff & 0xffff) = 0x6fff  (carry into bit 16)

       0x00000000
     + 0x00006fff   (bit 15 not set, so the top half is zero)
     ------------
       0x00006fff   which is wrong (assuming that fred is at 0xffff).  */

static bfd_boolean
v850_elf_perform_lo16_relocation (bfd *abfd, unsigned long *insn,
				  unsigned long addend)
{
#define BIT15_SET(x) ((x) & 0x8000)
#define OVERFLOWS(a,i) ((((a) & 0xffff) + (i)) > 0xffff)

  if ((BIT15_SET (*insn + addend) && ! BIT15_SET (addend))
      || (OVERFLOWS (addend, *insn)
	  && ((! BIT15_SET (*insn)) || (BIT15_SET (addend)))))
    {
      bfd_boolean already_updated;
      bfd_byte *hi16s_address = find_remembered_hi16s_reloc
	(addend, & already_updated);

      /* Amend the matching HI16_S relocation.  */
      if (hi16s_address != NULL)
	{
	  if (! already_updated)
	    {
	      unsigned long hi_insn = bfd_get_16 (abfd, hi16s_address);
	      hi_insn += 1;
	      bfd_put_16 (abfd, hi_insn, hi16s_address);
	    }
	}
      else
	{
	  fprintf (stderr, _("FAILED to find previous HI16 reloc\n"));
	  return FALSE;
	}
    }
#undef OVERFLOWS
#undef BIT15_SET

  /* Do not complain if value has top bit set, as this has been
     anticipated.  */
  *insn = (*insn + addend) & 0xffff;
  return TRUE;
}

/* FIXME:  The code here probably ought to be removed and the code in reloc.c
   allowed to do its stuff instead.  At least for most of the relocs, anyway.  */

static bfd_reloc_status_type
v850_elf_perform_relocation (bfd *abfd,
			     unsigned int r_type,
			     bfd_vma addend,
			     bfd_byte *address)
{
  unsigned long insn;
  unsigned long result;
  bfd_signed_vma saddend = (bfd_signed_vma) addend;

  switch (r_type)
    {
    default:
      return bfd_reloc_notsupported;

    case R_V850_REL32:
    case R_V850_ABS32:
      bfd_put_32 (abfd, addend, address);
      return bfd_reloc_ok;

    case R_V850_22_PCREL:
      if (saddend > 0x1fffff || saddend < -0x200000)
	return bfd_reloc_overflow;

      if ((addend % 2) != 0)
	return bfd_reloc_dangerous;

      insn  = bfd_get_32 (abfd, address);
      insn &= ~0xfffe003f;
      insn |= (((addend & 0xfffe) << 16) | ((addend & 0x3f0000) >> 16));
      bfd_put_32 (abfd, (bfd_vma) insn, address);
      return bfd_reloc_ok;

    case R_V850_9_PCREL:
      if (saddend > 0xff || saddend < -0x100)
	return bfd_reloc_overflow;

      if ((addend % 2) != 0)
	return bfd_reloc_dangerous;

      insn  = bfd_get_16 (abfd, address);
      insn &= ~ 0xf870;
      insn |= ((addend & 0x1f0) << 7) | ((addend & 0x0e) << 3);
      break;

    case R_V850_HI16:
      addend += (bfd_get_16 (abfd, address) << 16);
      addend = (addend >> 16);
      insn = addend;
      break;

    case R_V850_HI16_S:
      /* Remember where this relocation took place.  */
      remember_hi16s_reloc (abfd, addend, address);

      addend += (bfd_get_16 (abfd, address) << 16);
      addend = (addend >> 16) + ((addend & 0x8000) != 0);

      /* This relocation cannot overflow.  */
      if (addend > 0x7fff)
	addend = 0;

      insn = addend;
      break;

    case R_V850_LO16:
      insn = bfd_get_16 (abfd, address);
      if (! v850_elf_perform_lo16_relocation (abfd, &insn, addend))
	return bfd_reloc_overflow;
      break;

    case R_V850_8:
      addend += (char) bfd_get_8 (abfd, address);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7f || saddend < -0x80)
	return bfd_reloc_overflow;

      bfd_put_8 (abfd, addend, address);
      return bfd_reloc_ok;

    case R_V850_CALLT_16_16_OFFSET:
      addend += bfd_get_16 (abfd, address);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xffff || saddend < 0)
	return bfd_reloc_overflow;

      insn = addend;
      break;

    case R_V850_16:
    case R_V850_SDA_16_16_OFFSET:
    case R_V850_ZDA_16_16_OFFSET:
    case R_V850_TDA_16_16_OFFSET:
      addend += bfd_get_16 (abfd, address);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7fff || saddend < -0x8000)
	return bfd_reloc_overflow;

      insn = addend;
      break;

    case R_V850_SDA_15_16_OFFSET:
    case R_V850_ZDA_15_16_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += (insn & 0xfffe);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7ffe || saddend < -0x8000)
	return bfd_reloc_overflow;

      if (addend & 1)
        return bfd_reloc_dangerous;

      insn = (addend &~ (bfd_vma) 1) | (insn & 1);
      break;

    case R_V850_TDA_6_8_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0x7e) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xfc || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 3)
	return bfd_reloc_dangerous;

      insn &= 0xff81;
      insn |= (addend >> 1);
      break;

    case R_V850_TDA_7_8_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0x7f) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xfe || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 1)
	return bfd_reloc_dangerous;

      insn &= 0xff80;
      insn |= (addend >> 1);
      break;

    case R_V850_TDA_7_7_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += insn & 0x7f;

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7f || saddend < 0)
	return bfd_reloc_overflow;

      insn &= 0xff80;
      insn |= addend;
      break;

    case R_V850_TDA_4_5_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0xf) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x1e || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 1)
	return bfd_reloc_dangerous;

      insn &= 0xfff0;
      insn |= (addend >> 1);
      break;

    case R_V850_TDA_4_4_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += insn & 0xf;

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0xf || saddend < 0)
	return bfd_reloc_overflow;

      insn &= 0xfff0;
      insn |= addend;
      break;

    case R_V850_LO16_SPLIT_OFFSET:
      insn = bfd_get_32 (abfd, address);
      result = ((insn & 0xfffe0000) >> 16) | ((insn & 0x20) >> 5);
      if (! v850_elf_perform_lo16_relocation (abfd, &result, addend))
	return bfd_reloc_overflow;
      insn = (((result << 16) & 0xfffe0000)
	      | ((result << 5) & 0x20)
	      | (insn & ~0xfffe0020));
      bfd_put_32 (abfd, insn, address);
      return bfd_reloc_ok;

    case R_V850_ZDA_16_16_SPLIT_OFFSET:
    case R_V850_SDA_16_16_SPLIT_OFFSET:
      insn = bfd_get_32 (abfd, address);
      addend += ((insn & 0xfffe0000) >> 16) + ((insn & 0x20) >> 5);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7fff || saddend < -0x8000)
	return bfd_reloc_overflow;

      insn &= 0x0001ffdf;
      insn |= (addend & 1) << 5;
      insn |= (addend &~ (bfd_vma) 1) << 16;

      bfd_put_32 (abfd, (bfd_vma) insn, address);
      return bfd_reloc_ok;

    case R_V850_CALLT_6_7_OFFSET:
      insn = bfd_get_16 (abfd, address);
      addend += ((insn & 0x3f) << 1);

      saddend = (bfd_signed_vma) addend;

      if (saddend > 0x7e || saddend < 0)
	return bfd_reloc_overflow;

      if (addend & 1)
	return bfd_reloc_dangerous;

      insn &= 0xff80;
      insn |= (addend >> 1);
      break;

    case R_V850_GNU_VTINHERIT:
    case R_V850_GNU_VTENTRY:
      return bfd_reloc_ok;

    }

  bfd_put_16 (abfd, (bfd_vma) insn, address);
  return bfd_reloc_ok;
}

/* Insert the addend into the instruction.  */

static bfd_reloc_status_type
v850_elf_reloc (bfd *abfd ATTRIBUTE_UNUSED,
		arelent *reloc,
		asymbol *symbol,
		void * data ATTRIBUTE_UNUSED,
		asection *isection,
		bfd *obfd,
		char **err ATTRIBUTE_UNUSED)
{
  long relocation;

  /* If there is an output BFD,
     and the symbol is not a section name (which is only defined at final link time),
     and either we are not putting the addend into the instruction
      or the addend is zero, so there is nothing to add into the instruction
     then just fixup the address and return.  */
  if (obfd != NULL
      && (symbol->flags & BSF_SECTION_SYM) == 0
      && (! reloc->howto->partial_inplace
	  || reloc->addend == 0))
    {
      reloc->address += isection->output_offset;
      return bfd_reloc_ok;
    }

  /* Catch relocs involving undefined symbols.  */
  if (bfd_is_und_section (symbol->section)
      && (symbol->flags & BSF_WEAK) == 0
      && obfd == NULL)
    return bfd_reloc_undefined;

  /* We handle final linking of some relocs ourselves.  */

  /* Is the address of the relocation really within the section?  */
  if (reloc->address > bfd_get_section_limit (abfd, isection))
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targeted at and the
     initial relocation command value.  */

  if (reloc->howto->pc_relative)
    return bfd_reloc_ok;

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  /* Convert input-section-relative symbol value to absolute + addend.  */
  relocation += symbol->section->output_section->vma;
  relocation += symbol->section->output_offset;
  relocation += reloc->addend;

  reloc->addend = relocation;
  return bfd_reloc_ok;
}

/* This function is used for relocs which are only used
   for relaxing, which the linker should otherwise ignore.  */

static bfd_reloc_status_type
v850_elf_ignore_reloc (bfd *abfd ATTRIBUTE_UNUSED,
		       arelent *reloc_entry,
		       asymbol *symbol ATTRIBUTE_UNUSED,
		       void * data ATTRIBUTE_UNUSED,
		       asection *input_section,
		       bfd *output_bfd,
		       char **error_message ATTRIBUTE_UNUSED)
{
  if (output_bfd != NULL)
    reloc_entry->address += input_section->output_offset;

  return bfd_reloc_ok;
}
/* Note: It is REQUIRED that the 'type' value of each entry
   in this array match the index of the entry in the array.  */
static reloc_howto_type v850_elf_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_V850_NONE,			/* Type.  */
	 0,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_bitfield,	/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,		/* Special_function.  */
	 "R_V850_NONE",			/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0,				/* Src_mask.  */
	 0,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* A PC relative 9 bit branch.  */
  HOWTO (R_V850_9_PCREL,		/* Type.  */
	 2,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 26,				/* Bitsize.  */
	 TRUE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_bitfield,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_9_PCREL",		/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x00ffffff,			/* Src_mask.  */
	 0x00ffffff,			/* Dst_mask.  */
	 TRUE),				/* PCrel_offset.  */

  /* A PC relative 22 bit branch.  */
  HOWTO (R_V850_22_PCREL,		/* Type.  */
	 2,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 22,				/* Bitsize.  */
	 TRUE,				/* PC_relative.  */
	 7,				/* Bitpos.  */
	 complain_overflow_signed,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_22_PCREL",		/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x07ffff80,			/* Src_mask.  */
	 0x07ffff80,			/* Dst_mask.  */
	 TRUE),				/* PCrel_offset.  */

  /* High 16 bits of symbol value.  */
  HOWTO (R_V850_HI16_S,			/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_HI16_S",		/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* High 16 bits of symbol value.  */
  HOWTO (R_V850_HI16,			/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_HI16",			/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* Low 16 bits of symbol value.  */
  HOWTO (R_V850_LO16,			/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_LO16",			/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* Simple 32bit reloc.  */
  HOWTO (R_V850_ABS32,			/* Type.  */
	 0,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_ABS32",		/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffffffff,			/* Src_mask.  */
	 0xffffffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* Simple 16bit reloc.  */
  HOWTO (R_V850_16,			/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,		/* Special_function.  */
	 "R_V850_16",			/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* Simple 8bit reloc.	 */
  HOWTO (R_V850_8,			/* Type.  */
	 0,				/* Rightshift.  */
	 0,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 8,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,		/* Special_function.  */
	 "R_V850_8",			/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xff,				/* Src_mask.  */
	 0xff,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 16 bit offset from the short data area pointer.  */
  HOWTO (R_V850_SDA_16_16_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_SDA_16_16_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 15 bit offset from the short data area pointer.  */
  HOWTO (R_V850_SDA_15_16_OFFSET,	/* Type.  */
	 1,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 1,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_SDA_15_16_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xfffe,			/* Src_mask.  */
	 0xfffe,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 16 bit offset from the zero data area pointer.  */
  HOWTO (R_V850_ZDA_16_16_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_ZDA_16_16_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 15 bit offset from the zero data area pointer.  */
  HOWTO (R_V850_ZDA_15_16_OFFSET,	/* Type.  */
	 1,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 1,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_ZDA_15_16_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xfffe,			/* Src_mask.  */
	 0xfffe,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 6 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_6_8_OFFSET,		/* Type.  */
	 2,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 8,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 1,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_TDA_6_8_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x7e,				/* Src_mask.  */
	 0x7e,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 8 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_7_8_OFFSET,		/* Type.  */
	 1,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 8,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_TDA_7_8_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x7f,				/* Src_mask.  */
	 0x7f,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 7 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_7_7_OFFSET,		/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 7,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_TDA_7_7_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x7f,				/* Src_mask.  */
	 0x7f,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 16 bit offset from the tiny data area pointer!  */
  HOWTO (R_V850_TDA_16_16_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_TDA_16_16_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xfff,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 5 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_4_5_OFFSET,		/* Type.  */
	 1,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 5,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_TDA_4_5_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x0f,				/* Src_mask.  */
	 0x0f,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 4 bit offset from the tiny data area pointer.  */
  HOWTO (R_V850_TDA_4_4_OFFSET,		/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 4,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_TDA_4_4_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x0f,				/* Src_mask.  */
	 0x0f,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 16 bit offset from the short data area pointer.  */
  HOWTO (R_V850_SDA_16_16_SPLIT_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_SDA_16_16_SPLIT_OFFSET",/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xfffe0020,			/* Src_mask.  */
	 0xfffe0020,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 16 bit offset from the zero data area pointer.  */
  HOWTO (R_V850_ZDA_16_16_SPLIT_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_ZDA_16_16_SPLIT_OFFSET",/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xfffe0020,			/* Src_mask.  */
	 0xfffe0020,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 6 bit offset from the call table base pointer.  */
  HOWTO (R_V850_CALLT_6_7_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 7,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_CALLT_6_7_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0x3f,				/* Src_mask.  */
	 0x3f,				/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* 16 bit offset from the call table base pointer.  */
  HOWTO (R_V850_CALLT_16_16_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 1,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_CALLT_16_16_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffff,			/* Src_mask.  */
	 0xffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* GNU extension to record C++ vtable hierarchy */
  HOWTO (R_V850_GNU_VTINHERIT, /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         0,                     /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_dont, /* Complain_on_overflow.  */
         NULL,                  /* Special_function.  */
         "R_V850_GNU_VTINHERIT", /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0,                     /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */

  /* GNU extension to record C++ vtable member usage */
  HOWTO (R_V850_GNU_VTENTRY,     /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         0,                     /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_dont, /* Complain_on_overflow.  */
         _bfd_elf_rel_vtable_reloc_fn,  /* Special_function.  */
         "R_V850_GNU_VTENTRY",   /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0,                     /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */

  /* Indicates a .longcall pseudo-op.  The compiler will generate a .longcall
     pseudo-op when it finds a function call which can be relaxed.  */
  HOWTO (R_V850_LONGCALL,     /* Type.  */
       0,                     /* Rightshift.  */
       2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
       32,                    /* Bitsize.  */
       TRUE,                  /* PC_relative.  */
       0,                     /* Bitpos.  */
       complain_overflow_signed, /* Complain_on_overflow.  */
       v850_elf_ignore_reloc, /* Special_function.  */
       "R_V850_LONGCALL",     /* Name.  */
       FALSE,                 /* Partial_inplace.  */
       0,                     /* Src_mask.  */
       0,                     /* Dst_mask.  */
       TRUE),                 /* PCrel_offset.  */

  /* Indicates a .longjump pseudo-op.  The compiler will generate a
     .longjump pseudo-op when it finds a branch which can be relaxed.  */
  HOWTO (R_V850_LONGJUMP,     /* Type.  */
       0,                     /* Rightshift.  */
       2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
       32,                    /* Bitsize.  */
       TRUE,                  /* PC_relative.  */
       0,                     /* Bitpos.  */
       complain_overflow_signed, /* Complain_on_overflow.  */
       v850_elf_ignore_reloc, /* Special_function.  */
       "R_V850_LONGJUMP",     /* Name.  */
       FALSE,                 /* Partial_inplace.  */
       0,                     /* Src_mask.  */
       0,                     /* Dst_mask.  */
       TRUE),                 /* PCrel_offset.  */

  HOWTO (R_V850_ALIGN,        /* Type.  */
       0,                     /* Rightshift.  */
       1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
       0,                     /* Bitsize.  */
       FALSE,                 /* PC_relative.  */
       0,                     /* Bitpos.  */
       complain_overflow_unsigned, /* Complain_on_overflow.  */
       v850_elf_ignore_reloc, /* Special_function.  */
       "R_V850_ALIGN",        /* Name.  */
       FALSE,                 /* Partial_inplace.  */
       0,                     /* Src_mask.  */
       0,                     /* Dst_mask.  */
       TRUE),                 /* PCrel_offset.  */
  
  /* Simple pc-relative 32bit reloc.  */
  HOWTO (R_V850_REL32,			/* Type.  */
	 0,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,				/* Bitsize.  */
	 TRUE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_REL32",		/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xffffffff,			/* Src_mask.  */
	 0xffffffff,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */

  /* An ld.bu version of R_V850_LO16.  */
  HOWTO (R_V850_LO16_SPLIT_OFFSET,	/* Type.  */
	 0,				/* Rightshift.  */
	 2,				/* Size (0 = byte, 1 = short, 2 = long).  */
	 16,				/* Bitsize.  */
	 FALSE,				/* PC_relative.  */
	 0,				/* Bitpos.  */
	 complain_overflow_dont,	/* Complain_on_overflow.  */
	 v850_elf_reloc,		/* Special_function.  */
	 "R_V850_LO16_SPLIT_OFFSET",	/* Name.  */
	 FALSE,				/* Partial_inplace.  */
	 0xfffe0020,			/* Src_mask.  */
	 0xfffe0020,			/* Dst_mask.  */
	 FALSE),			/* PCrel_offset.  */
};

/* Map BFD reloc types to V850 ELF reloc types.  */

struct v850_elf_reloc_map
{
  /* BFD_RELOC_V850_CALLT_16_16_OFFSET is 258, which will not fix in an
     unsigned char.  */
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned int elf_reloc_val;
};

static const struct v850_elf_reloc_map v850_elf_reloc_map[] =
{
  { BFD_RELOC_NONE,		           R_V850_NONE                   },
  { BFD_RELOC_V850_9_PCREL,	           R_V850_9_PCREL                },
  { BFD_RELOC_V850_22_PCREL,	           R_V850_22_PCREL               },
  { BFD_RELOC_HI16_S,		           R_V850_HI16_S                 },
  { BFD_RELOC_HI16,		           R_V850_HI16                   },
  { BFD_RELOC_LO16,		           R_V850_LO16                   },
  { BFD_RELOC_32,		           R_V850_ABS32                  },
  { BFD_RELOC_32_PCREL,		           R_V850_REL32                  },
  { BFD_RELOC_16,		           R_V850_16                     },
  { BFD_RELOC_8,		           R_V850_8                      },
  { BFD_RELOC_V850_SDA_16_16_OFFSET,       R_V850_SDA_16_16_OFFSET       },
  { BFD_RELOC_V850_SDA_15_16_OFFSET,       R_V850_SDA_15_16_OFFSET       },
  { BFD_RELOC_V850_ZDA_16_16_OFFSET,       R_V850_ZDA_16_16_OFFSET       },
  { BFD_RELOC_V850_ZDA_15_16_OFFSET,       R_V850_ZDA_15_16_OFFSET       },
  { BFD_RELOC_V850_TDA_6_8_OFFSET,         R_V850_TDA_6_8_OFFSET         },
  { BFD_RELOC_V850_TDA_7_8_OFFSET,         R_V850_TDA_7_8_OFFSET         },
  { BFD_RELOC_V850_TDA_7_7_OFFSET,         R_V850_TDA_7_7_OFFSET         },
  { BFD_RELOC_V850_TDA_16_16_OFFSET,       R_V850_TDA_16_16_OFFSET       },
  { BFD_RELOC_V850_TDA_4_5_OFFSET,         R_V850_TDA_4_5_OFFSET         },
  { BFD_RELOC_V850_TDA_4_4_OFFSET,         R_V850_TDA_4_4_OFFSET         },
  { BFD_RELOC_V850_LO16_SPLIT_OFFSET,      R_V850_LO16_SPLIT_OFFSET      },
  { BFD_RELOC_V850_SDA_16_16_SPLIT_OFFSET, R_V850_SDA_16_16_SPLIT_OFFSET },
  { BFD_RELOC_V850_ZDA_16_16_SPLIT_OFFSET, R_V850_ZDA_16_16_SPLIT_OFFSET },
  { BFD_RELOC_V850_CALLT_6_7_OFFSET,       R_V850_CALLT_6_7_OFFSET       },
  { BFD_RELOC_V850_CALLT_16_16_OFFSET,     R_V850_CALLT_16_16_OFFSET     },
  { BFD_RELOC_VTABLE_INHERIT,              R_V850_GNU_VTINHERIT          },
  { BFD_RELOC_VTABLE_ENTRY,                R_V850_GNU_VTENTRY            },
  { BFD_RELOC_V850_LONGCALL,               R_V850_LONGCALL               },
  { BFD_RELOC_V850_LONGJUMP,               R_V850_LONGJUMP               },
  { BFD_RELOC_V850_ALIGN,                  R_V850_ALIGN                  },

};

/* Map a bfd relocation into the appropriate howto structure.  */

static reloc_howto_type *
v850_elf_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = ARRAY_SIZE (v850_elf_reloc_map); i --;)
    if (v850_elf_reloc_map[i].bfd_reloc_val == code)
      {
	unsigned int elf_reloc_val = v850_elf_reloc_map[i].elf_reloc_val;

	BFD_ASSERT (v850_elf_howto_table[elf_reloc_val].type == elf_reloc_val);

	return v850_elf_howto_table + elf_reloc_val;
      }

  return NULL;
}

/* Set the howto pointer for an V850 ELF reloc.  */

static void
v850_elf_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
			    arelent *cache_ptr,
			    Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_V850_max);
  cache_ptr->howto = &v850_elf_howto_table[r_type];
}

/* Set the howto pointer for a V850 ELF reloc (type RELA).  */

static void
v850_elf_info_to_howto_rela (bfd *abfd ATTRIBUTE_UNUSED,
			     arelent * cache_ptr,
			     Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_V850_max);
  cache_ptr->howto = &v850_elf_howto_table[r_type];
}

static bfd_boolean
v850_elf_is_local_label_name (bfd *abfd ATTRIBUTE_UNUSED, const char *name)
{
  return (   (name[0] == '.' && (name[1] == 'L' || name[1] == '.'))
	  || (name[0] == '_' &&  name[1] == '.' && name[2] == 'L' && name[3] == '_'));
}

/* We overload some of the bfd_reloc error codes for own purposes.  */
#define bfd_reloc_gp_not_found		bfd_reloc_other
#define bfd_reloc_ep_not_found		bfd_reloc_continue
#define bfd_reloc_ctbp_not_found	(bfd_reloc_dangerous + 1)

/* Perform a relocation as part of a final link.  */

static bfd_reloc_status_type
v850_elf_final_link_relocate (reloc_howto_type *howto,
			      bfd *input_bfd,
			      bfd *output_bfd ATTRIBUTE_UNUSED,
			      asection *input_section,
			      bfd_byte *contents,
			      bfd_vma offset,
			      bfd_vma value,
			      bfd_vma addend,
			      struct bfd_link_info *info,
			      asection *sym_sec,
			      int is_local ATTRIBUTE_UNUSED)
{
  unsigned int r_type = howto->type;
  bfd_byte *hit_data = contents + offset;

  /* Adjust the value according to the relocation.  */
  switch (r_type)
    {
    case R_V850_9_PCREL:
      value -= (input_section->output_section->vma
		+ input_section->output_offset);
      value -= offset;
      break;

    case R_V850_22_PCREL:
      value -= (input_section->output_section->vma
		+ input_section->output_offset
		+ offset);

      /* If the sign extension will corrupt the value then we have overflowed.  */
      if (((value & 0xff000000) != 0x0) && ((value & 0xff000000) != 0xff000000))
	return bfd_reloc_overflow;

      /* Only the bottom 24 bits of the PC are valid.  */
      value = SEXT24 (value);
      break;

    case R_V850_REL32:
      value -= (input_section->output_section->vma
		+ input_section->output_offset
		+ offset);
      break;

    case R_V850_HI16_S:
    case R_V850_HI16:
    case R_V850_LO16:
    case R_V850_LO16_SPLIT_OFFSET:
    case R_V850_16:
    case R_V850_ABS32:
    case R_V850_8:
      break;

    case R_V850_ZDA_15_16_OFFSET:
    case R_V850_ZDA_16_16_OFFSET:
    case R_V850_ZDA_16_16_SPLIT_OFFSET:
      if (sym_sec == NULL)
	return bfd_reloc_undefined;

      value -= sym_sec->output_section->vma;
      break;

    case R_V850_SDA_15_16_OFFSET:
    case R_V850_SDA_16_16_OFFSET:
    case R_V850_SDA_16_16_SPLIT_OFFSET:
      {
	unsigned long                gp;
	struct bfd_link_hash_entry * h;

	if (sym_sec == NULL)
	  return bfd_reloc_undefined;

	/* Get the value of __gp.  */
	h = bfd_link_hash_lookup (info->hash, "__gp", FALSE, FALSE, TRUE);
	if (h == NULL
	    || h->type != bfd_link_hash_defined)
	  return bfd_reloc_gp_not_found;

	gp = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);

	value -= sym_sec->output_section->vma;
	value -= (gp - sym_sec->output_section->vma);
      }
    break;

    case R_V850_TDA_4_4_OFFSET:
    case R_V850_TDA_4_5_OFFSET:
    case R_V850_TDA_16_16_OFFSET:
    case R_V850_TDA_7_7_OFFSET:
    case R_V850_TDA_7_8_OFFSET:
    case R_V850_TDA_6_8_OFFSET:
      {
	unsigned long                ep;
	struct bfd_link_hash_entry * h;

	/* Get the value of __ep.  */
	h = bfd_link_hash_lookup (info->hash, "__ep", FALSE, FALSE, TRUE);
	if (h == NULL
	    || h->type != bfd_link_hash_defined)
	  return bfd_reloc_ep_not_found;

	ep = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);

	value -= ep;
      }
    break;

    case R_V850_CALLT_6_7_OFFSET:
      {
	unsigned long                ctbp;
	struct bfd_link_hash_entry * h;

	/* Get the value of __ctbp.  */
	h = bfd_link_hash_lookup (info->hash, "__ctbp", FALSE, FALSE, TRUE);
	if (h == NULL
	    || h->type != bfd_link_hash_defined)
	  return bfd_reloc_ctbp_not_found;

	ctbp = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);
	value -= ctbp;
      }
    break;

    case R_V850_CALLT_16_16_OFFSET:
      {
	unsigned long                ctbp;
	struct bfd_link_hash_entry * h;

	if (sym_sec == NULL)
	  return bfd_reloc_undefined;

	/* Get the value of __ctbp.  */
	h = bfd_link_hash_lookup (info->hash, "__ctbp", FALSE, FALSE, TRUE);
	if (h == NULL
	    || h->type != bfd_link_hash_defined)
	  return bfd_reloc_ctbp_not_found;

	ctbp = (h->u.def.value
	      + h->u.def.section->output_section->vma
	      + h->u.def.section->output_offset);

	value -= sym_sec->output_section->vma;
	value -= (ctbp - sym_sec->output_section->vma);
      }
    break;

    case R_V850_NONE:
    case R_V850_GNU_VTINHERIT:
    case R_V850_GNU_VTENTRY:
    case R_V850_LONGCALL:
    case R_V850_LONGJUMP:
    case R_V850_ALIGN:
      return bfd_reloc_ok;

    default:
      return bfd_reloc_notsupported;
    }

  /* Perform the relocation.  */
  return v850_elf_perform_relocation (input_bfd, r_type, value + addend, hit_data);
}

/* Relocate an V850 ELF section.  */

static bfd_boolean
v850_elf_relocate_section (bfd *output_bfd,
			   struct bfd_link_info *info,
			   bfd *input_bfd,
			   asection *input_section,
			   bfd_byte *contents,
			   Elf_Internal_Rela *relocs,
			   Elf_Internal_Sym *local_syms,
			   asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  /* Reset the list of remembered HI16S relocs to empty.  */
  free_hi16s     = previous_hi16s;
  previous_hi16s = NULL;
  hi16s_counter  = 0;

  rel    = relocs;
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
      r_type   = ELF32_R_TYPE (rel->r_info);

      if (r_type == R_V850_GNU_VTENTRY
          || r_type == R_V850_GNU_VTINHERIT)
        continue;

      /* This is a final link.  */
      howto = v850_elf_howto_table + r_type;
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

	  /* Note - this check is delayed until now as it is possible and
	     valid to have a file without any symbols but with relocs that
	     can be processed.  */
	  if (sym_hashes == NULL)
	    {
	      info->callbacks->warning
		(info, "no hash table available",
		 NULL, input_bfd, input_section, (bfd_vma) 0);

	      return FALSE;
	    }

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);
	}

      /* FIXME: We should use the addend, but the COFF relocations don't.  */
      r = v850_elf_final_link_relocate (howto, input_bfd, output_bfd,
					input_section,
					contents, rel->r_offset,
					relocation, rel->r_addend,
					info, sec, h == NULL);

      if (r != bfd_reloc_ok)
	{
	  const char * name;
	  const char * msg = NULL;

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
	      msg = _("internal error: dangerous relocation");
	      goto common_error;

	    case bfd_reloc_gp_not_found:
	      msg = _("could not locate special linker symbol __gp");
	      goto common_error;

	    case bfd_reloc_ep_not_found:
	      msg = _("could not locate special linker symbol __ep");
	      goto common_error;

	    case bfd_reloc_ctbp_not_found:
	      msg = _("could not locate special linker symbol __ctbp");
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

static bfd_boolean
v850_elf_gc_sweep_hook (bfd *abfd ATTRIBUTE_UNUSED,
			struct bfd_link_info *info ATTRIBUTE_UNUSED,
			asection *sec ATTRIBUTE_UNUSED,
			const Elf_Internal_Rela *relocs ATTRIBUTE_UNUSED)
{
  /* No got and plt entries for v850-elf.  */
  return TRUE;
}

static asection *
v850_elf_gc_mark_hook (asection *sec,
		       struct bfd_link_info *info ATTRIBUTE_UNUSED,
		       Elf_Internal_Rela *rel,
		       struct elf_link_hash_entry *h,
		       Elf_Internal_Sym *sym)
{
  if (h != NULL)
    {
      switch (ELF32_R_TYPE (rel->r_info))
      {
      case R_V850_GNU_VTINHERIT:
      case R_V850_GNU_VTENTRY:
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
     return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}

/* Set the right machine number.  */

static bfd_boolean
v850_elf_object_p (bfd *abfd)
{
  switch (elf_elfheader (abfd)->e_flags & EF_V850_ARCH)
    {
    default:
    case E_V850_ARCH:
      bfd_default_set_arch_mach (abfd, bfd_arch_v850, bfd_mach_v850);
      break;
    case E_V850E_ARCH:
      bfd_default_set_arch_mach (abfd, bfd_arch_v850, bfd_mach_v850e);
      break;
    case E_V850E1_ARCH:
      bfd_default_set_arch_mach (abfd, bfd_arch_v850, bfd_mach_v850e1);
      break;
    }
  return TRUE;
}

/* Store the machine number in the flags field.  */

static void
v850_elf_final_write_processing (bfd *abfd,
				 bfd_boolean linker ATTRIBUTE_UNUSED)
{
  unsigned long val;

  switch (bfd_get_mach (abfd))
    {
    default:
    case bfd_mach_v850:   val = E_V850_ARCH; break;
    case bfd_mach_v850e:  val = E_V850E_ARCH; break;
    case bfd_mach_v850e1: val = E_V850E1_ARCH; break;
    }

  elf_elfheader (abfd)->e_flags &=~ EF_V850_ARCH;
  elf_elfheader (abfd)->e_flags |= val;
}

/* Function to keep V850 specific file flags.  */

static bfd_boolean
v850_elf_set_private_flags (bfd *abfd, flagword flags)
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags = flags;
  elf_flags_init (abfd) = TRUE;
  return TRUE;
}

/* Merge backend specific data from an object file
   to the output object file when linking.  */

static bfd_boolean
v850_elf_merge_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  flagword out_flags;
  flagword in_flags;

  if (   bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return TRUE;

  in_flags = elf_elfheader (ibfd)->e_flags;
  out_flags = elf_elfheader (obfd)->e_flags;

  if (! elf_flags_init (obfd))
    {
      /* If the input is the default architecture then do not
	 bother setting the flags for the output architecture,
	 instead allow future merges to do this.  If no future
	 merges ever set these flags then they will retain their
	 unitialised values, which surprise surprise, correspond
	 to the default values.  */
      if (bfd_get_arch_info (ibfd)->the_default)
	return TRUE;

      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flags;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd), bfd_get_mach (ibfd));

      return TRUE;
    }

  /* Check flag compatibility.  */
  if (in_flags == out_flags)
    return TRUE;

  if ((in_flags & EF_V850_ARCH) != (out_flags & EF_V850_ARCH)
      && (in_flags & EF_V850_ARCH) != E_V850_ARCH)
    {
      /* Allow v850e1 binaries to be linked with v850e binaries.
	 Set the output binary to v850e.  */
      if ((in_flags & EF_V850_ARCH) == E_V850E1_ARCH
	  && (out_flags & EF_V850_ARCH) == E_V850E_ARCH)
	return TRUE;

      if ((in_flags & EF_V850_ARCH) == E_V850E_ARCH
	  && (out_flags & EF_V850_ARCH) == E_V850E1_ARCH)
	{
	  elf_elfheader (obfd)->e_flags =
	    ((out_flags & ~ EF_V850_ARCH) | E_V850E_ARCH);
	  return TRUE;
	}

      _bfd_error_handler (_("%B: Architecture mismatch with previous modules"),
			  ibfd);
    }

  return TRUE;
}

/* Display the flags field.  */

static bfd_boolean
v850_elf_print_private_bfd_data (bfd *abfd, void * ptr)
{
  FILE * file = (FILE *) ptr;

  BFD_ASSERT (abfd != NULL && ptr != NULL);

  _bfd_elf_print_private_bfd_data (abfd, ptr);

  /* xgettext:c-format */
  fprintf (file, _("private flags = %lx: "), elf_elfheader (abfd)->e_flags);

  switch (elf_elfheader (abfd)->e_flags & EF_V850_ARCH)
    {
    default:
    case E_V850_ARCH: fprintf (file, _("v850 architecture")); break;
    case E_V850E_ARCH: fprintf (file, _("v850e architecture")); break;
    case E_V850E1_ARCH: fprintf (file, _("v850e1 architecture")); break;
    }

  fputc ('\n', file);

  return TRUE;
}

/* V850 ELF uses four common sections.  One is the usual one, and the
   others are for (small) objects in one of the special data areas:
   small, tiny and zero.  All the objects are kept together, and then
   referenced via the gp register, the ep register or the r0 register
   respectively, which yields smaller, faster assembler code.  This
   approach is copied from elf32-mips.c.  */

static asection  v850_elf_scom_section;
static asymbol   v850_elf_scom_symbol;
static asymbol * v850_elf_scom_symbol_ptr;
static asection  v850_elf_tcom_section;
static asymbol   v850_elf_tcom_symbol;
static asymbol * v850_elf_tcom_symbol_ptr;
static asection  v850_elf_zcom_section;
static asymbol   v850_elf_zcom_symbol;
static asymbol * v850_elf_zcom_symbol_ptr;

/* Given a BFD section, try to locate the
   corresponding ELF section index.  */

static bfd_boolean
v850_elf_section_from_bfd_section (bfd *abfd ATTRIBUTE_UNUSED,
				   asection *sec,
				   int *retval)
{
  if (strcmp (bfd_get_section_name (abfd, sec), ".scommon") == 0)
    *retval = SHN_V850_SCOMMON;
  else if (strcmp (bfd_get_section_name (abfd, sec), ".tcommon") == 0)
    *retval = SHN_V850_TCOMMON;
  else if (strcmp (bfd_get_section_name (abfd, sec), ".zcommon") == 0)
    *retval = SHN_V850_ZCOMMON;
  else
    return FALSE;

  return TRUE;
}

/* Handle the special V850 section numbers that a symbol may use.  */

static void
v850_elf_symbol_processing (bfd *abfd, asymbol *asym)
{
  elf_symbol_type * elfsym = (elf_symbol_type *) asym;
  unsigned int indx;

  indx = elfsym->internal_elf_sym.st_shndx;

  /* If the section index is an "ordinary" index, then it may
     refer to a v850 specific section created by the assembler.
     Check the section's type and change the index it matches.

     FIXME: Should we alter the st_shndx field as well ?  */

  if (indx < elf_numsections (abfd))
    switch (elf_elfsections(abfd)[indx]->sh_type)
      {
      case SHT_V850_SCOMMON:
	indx = SHN_V850_SCOMMON;
	break;

      case SHT_V850_TCOMMON:
	indx = SHN_V850_TCOMMON;
	break;

      case SHT_V850_ZCOMMON:
	indx = SHN_V850_ZCOMMON;
	break;

      default:
	break;
      }

  switch (indx)
    {
    case SHN_V850_SCOMMON:
      if (v850_elf_scom_section.name == NULL)
	{
	  /* Initialize the small common section.  */
	  v850_elf_scom_section.name           = ".scommon";
	  v850_elf_scom_section.flags          = SEC_IS_COMMON | SEC_ALLOC | SEC_DATA;
	  v850_elf_scom_section.output_section = & v850_elf_scom_section;
	  v850_elf_scom_section.symbol         = & v850_elf_scom_symbol;
	  v850_elf_scom_section.symbol_ptr_ptr = & v850_elf_scom_symbol_ptr;
	  v850_elf_scom_symbol.name            = ".scommon";
	  v850_elf_scom_symbol.flags           = BSF_SECTION_SYM;
	  v850_elf_scom_symbol.section         = & v850_elf_scom_section;
	  v850_elf_scom_symbol_ptr             = & v850_elf_scom_symbol;
	}
      asym->section = & v850_elf_scom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_V850_TCOMMON:
      if (v850_elf_tcom_section.name == NULL)
	{
	  /* Initialize the tcommon section.  */
	  v850_elf_tcom_section.name           = ".tcommon";
	  v850_elf_tcom_section.flags          = SEC_IS_COMMON;
	  v850_elf_tcom_section.output_section = & v850_elf_tcom_section;
	  v850_elf_tcom_section.symbol         = & v850_elf_tcom_symbol;
	  v850_elf_tcom_section.symbol_ptr_ptr = & v850_elf_tcom_symbol_ptr;
	  v850_elf_tcom_symbol.name            = ".tcommon";
	  v850_elf_tcom_symbol.flags           = BSF_SECTION_SYM;
	  v850_elf_tcom_symbol.section         = & v850_elf_tcom_section;
	  v850_elf_tcom_symbol_ptr             = & v850_elf_tcom_symbol;
	}
      asym->section = & v850_elf_tcom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;

    case SHN_V850_ZCOMMON:
      if (v850_elf_zcom_section.name == NULL)
	{
	  /* Initialize the zcommon section.  */
	  v850_elf_zcom_section.name           = ".zcommon";
	  v850_elf_zcom_section.flags          = SEC_IS_COMMON;
	  v850_elf_zcom_section.output_section = & v850_elf_zcom_section;
	  v850_elf_zcom_section.symbol         = & v850_elf_zcom_symbol;
	  v850_elf_zcom_section.symbol_ptr_ptr = & v850_elf_zcom_symbol_ptr;
	  v850_elf_zcom_symbol.name            = ".zcommon";
	  v850_elf_zcom_symbol.flags           = BSF_SECTION_SYM;
	  v850_elf_zcom_symbol.section         = & v850_elf_zcom_section;
	  v850_elf_zcom_symbol_ptr             = & v850_elf_zcom_symbol;
	}
      asym->section = & v850_elf_zcom_section;
      asym->value = elfsym->internal_elf_sym.st_size;
      break;
    }
}

/* Hook called by the linker routine which adds symbols from an object
   file.  We must handle the special v850 section numbers here.  */

static bfd_boolean
v850_elf_add_symbol_hook (bfd *abfd,
			  struct bfd_link_info *info ATTRIBUTE_UNUSED,
			  Elf_Internal_Sym *sym,
			  const char **namep ATTRIBUTE_UNUSED,
			  flagword *flagsp ATTRIBUTE_UNUSED,
			  asection **secp,
			  bfd_vma *valp)
{
  unsigned int indx = sym->st_shndx;

  /* If the section index is an "ordinary" index, then it may
     refer to a v850 specific section created by the assembler.
     Check the section's type and change the index it matches.

     FIXME: Should we alter the st_shndx field as well ?  */

  if (indx < elf_numsections (abfd))
    switch (elf_elfsections(abfd)[indx]->sh_type)
      {
      case SHT_V850_SCOMMON:
	indx = SHN_V850_SCOMMON;
	break;

      case SHT_V850_TCOMMON:
	indx = SHN_V850_TCOMMON;
	break;

      case SHT_V850_ZCOMMON:
	indx = SHN_V850_ZCOMMON;
	break;

      default:
	break;
      }

  switch (indx)
    {
    case SHN_V850_SCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".scommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_V850_TCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".tcommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;

    case SHN_V850_ZCOMMON:
      *secp = bfd_make_section_old_way (abfd, ".zcommon");
      (*secp)->flags |= SEC_IS_COMMON;
      *valp = sym->st_size;
      break;
    }

  return TRUE;
}

static bfd_boolean
v850_elf_link_output_symbol_hook (struct bfd_link_info *info ATTRIBUTE_UNUSED,
				  const char *name ATTRIBUTE_UNUSED,
				  Elf_Internal_Sym *sym,
				  asection *input_sec,
				  struct elf_link_hash_entry *h ATTRIBUTE_UNUSED)
{
  /* If we see a common symbol, which implies a relocatable link, then
     if a symbol was in a special common section in an input file, mark
     it as a special common in the output file.  */

  if (sym->st_shndx == SHN_COMMON)
    {
      if (strcmp (input_sec->name, ".scommon") == 0)
	sym->st_shndx = SHN_V850_SCOMMON;
      else if (strcmp (input_sec->name, ".tcommon") == 0)
	sym->st_shndx = SHN_V850_TCOMMON;
      else if (strcmp (input_sec->name, ".zcommon") == 0)
	sym->st_shndx = SHN_V850_ZCOMMON;
    }

  return TRUE;
}

static bfd_boolean
v850_elf_section_from_shdr (bfd *abfd,
			    Elf_Internal_Shdr *hdr,
			    const char *name,
			    int shindex)
{
  /* There ought to be a place to keep ELF backend specific flags, but
     at the moment there isn't one.  We just keep track of the
     sections by their name, instead.  */

  if (! _bfd_elf_make_section_from_shdr (abfd, hdr, name, shindex))
    return FALSE;

  switch (hdr->sh_type)
    {
    case SHT_V850_SCOMMON:
    case SHT_V850_TCOMMON:
    case SHT_V850_ZCOMMON:
      if (! bfd_set_section_flags (abfd, hdr->bfd_section,
				   (bfd_get_section_flags (abfd,
							   hdr->bfd_section)
				    | SEC_IS_COMMON)))
	return FALSE;
    }

  return TRUE;
}

/* Set the correct type for a V850 ELF section.  We do this
   by the section name, which is a hack, but ought to work.  */

static bfd_boolean
v850_elf_fake_sections (bfd *abfd ATTRIBUTE_UNUSED,
			Elf_Internal_Shdr *hdr,
			asection *sec)
{
  const char * name;

  name = bfd_get_section_name (abfd, sec);

  if (strcmp (name, ".scommon") == 0)
    hdr->sh_type = SHT_V850_SCOMMON;
  else if (strcmp (name, ".tcommon") == 0)
    hdr->sh_type = SHT_V850_TCOMMON;
  else if (strcmp (name, ".zcommon") == 0)
    hdr->sh_type = SHT_V850_ZCOMMON;

  return TRUE;
}

/* Delete some bytes from a section while relaxing.  */

static bfd_boolean
v850_elf_relax_delete_bytes (bfd *abfd,
			     asection *sec,
			     bfd_vma addr,
			     bfd_vma toaddr,
			     int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf32_External_Sym *extsyms;
  Elf32_External_Sym *esym;
  Elf32_External_Sym *esymend;
  int index;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel;
  Elf_Internal_Rela *irelend;
  struct elf_link_hash_entry *sym_hash;
  Elf_Internal_Shdr *shndx_hdr;
  Elf_External_Sym_Shndx *shndx;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  extsyms = (Elf32_External_Sym *) symtab_hdr->contents;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  contents = elf_section_data (sec)->this_hdr.contents;

  /* The deletion must stop at the next ALIGN reloc for an alignment
     power larger than the number of bytes we are deleting.  */

  /* Actually delete the bytes.  */
#if (DEBUG_RELAX & 2)
  fprintf (stderr, "relax_delete: contents: sec: %s  %p .. %p %x\n",
	   sec->name, addr, toaddr, count );
#endif
  memmove (contents + addr, contents + addr + count,
	   toaddr - addr - count);
  memset (contents + toaddr-count, 0, count);

  /* Adjust all the relocs.  */
  irel = elf_section_data (sec)->relocs;
  irelend = irel + sec->reloc_count;
  shndx_hdr = &elf_tdata (abfd)->symtab_shndx_hdr;
  shndx = (Elf_External_Sym_Shndx *) shndx_hdr->contents;

  for (; irel < irelend; irel++)
    {
      bfd_vma raddr, paddr, symval;
      Elf_Internal_Sym isym;

      /* Get the new reloc address.  */
      raddr = irel->r_offset;
      if ((raddr >= (addr + count) && raddr < toaddr))
	irel->r_offset -= count;

      if (raddr >= addr && raddr < addr + count)
	{
	  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
				       (int) R_V850_NONE);
	  continue;
	}

      if (ELF32_R_TYPE (irel->r_info) == (int) R_V850_ALIGN)
	continue;

      bfd_elf32_swap_symbol_in (abfd,
				extsyms + ELF32_R_SYM (irel->r_info),
				shndx ? shndx + ELF32_R_SYM (irel->r_info) : NULL,
				& isym);

      if (isym.st_shndx != sec_shndx)
	continue;

      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF32_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  symval = isym.st_value;
#if (DEBUG_RELAX & 2)
	  {
	    char * name = bfd_elf_string_from_elf_section
	                   (abfd, symtab_hdr->sh_link, isym.st_name);
	    fprintf (stderr,
	       "relax_delete: local: sec: %s, sym: %s (%d), value: %x + %x + %x addend %x\n",
	       sec->name, name, isym.st_name,
	       sec->output_section->vma, sec->output_offset,
	       isym.st_value, irel->r_addend);
	  }
#endif
	}
      else
	{
	  unsigned long indx;
	  struct elf_link_hash_entry * h;

	  /* An external symbol.  */
	  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;

	  h = elf_sym_hashes (abfd) [indx];
	  BFD_ASSERT (h != NULL);

	  symval = h->root.u.def.value;
#if (DEBUG_RELAX & 2)
	  fprintf (stderr,
		   "relax_delete: defined: sec: %s, name: %s, value: %x + %x + %x addend %x\n",
		   sec->name, h->root.root.string, h->root.u.def.value,
		   sec->output_section->vma, sec->output_offset, irel->r_addend);
#endif
	}

      paddr = symval + irel->r_addend;

      if ( (symval >= addr + count && symval < toaddr)
	  && (paddr < addr + count || paddr >= toaddr))
	irel->r_addend += count;
      else if (    (symval < addr + count || symval >= toaddr)
	        && (paddr >= addr + count && paddr < toaddr))
	irel->r_addend -= count;
    }

  /* Adjust the local symbols defined in this section.  */
  esym = extsyms;
  esymend = esym + symtab_hdr->sh_info;

  for (; esym < esymend; esym++, shndx = (shndx ? shndx + 1 : NULL))
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, shndx, & isym);

      if (isym.st_shndx == sec_shndx
	  && isym.st_value >= addr + count
	  && isym.st_value < toaddr)
	{
	  isym.st_value -= count;

	  if (isym.st_value + isym.st_size >= toaddr)
	    isym.st_size += count;

	  bfd_elf32_swap_symbol_out (abfd, & isym, esym, shndx);
	}
      else if (isym.st_shndx == sec_shndx
	       && isym.st_value < addr + count)
	{
	  if (isym.st_value+isym.st_size >= addr + count
	      && isym.st_value+isym.st_size < toaddr)
	    isym.st_size -= count;

	  if (isym.st_value >= addr
	      && isym.st_value <  addr + count)
	    isym.st_value = addr;

	  bfd_elf32_swap_symbol_out (abfd, & isym, esym, shndx);
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  esym = extsyms + symtab_hdr->sh_info;
  esymend = extsyms + (symtab_hdr->sh_size / sizeof (Elf32_External_Sym));

  for (index = 0; esym < esymend; esym ++, index ++)
    {
      Elf_Internal_Sym isym;

      bfd_elf32_swap_symbol_in (abfd, esym, shndx, & isym);
      sym_hash = elf_sym_hashes (abfd) [index];

      if (isym.st_shndx == sec_shndx
	  && ((sym_hash)->root.type == bfd_link_hash_defined
	      || (sym_hash)->root.type == bfd_link_hash_defweak)
	  && (sym_hash)->root.u.def.section == sec
	  && (sym_hash)->root.u.def.value >= addr + count
	  && (sym_hash)->root.u.def.value < toaddr)
	{
	  if ((sym_hash)->root.u.def.value + isym.st_size >= toaddr)
	    {
	      isym.st_size += count;
	      bfd_elf32_swap_symbol_out (abfd, & isym, esym, shndx);
	    }

	  (sym_hash)->root.u.def.value -= count;
	}
      else if (isym.st_shndx == sec_shndx
	       && ((sym_hash)->root.type == bfd_link_hash_defined
		   || (sym_hash)->root.type == bfd_link_hash_defweak)
	       && (sym_hash)->root.u.def.section == sec
	       && (sym_hash)->root.u.def.value < addr + count)
	{
	  if ((sym_hash)->root.u.def.value+isym.st_size >= addr + count
	      && (sym_hash)->root.u.def.value+isym.st_size < toaddr)
	    isym.st_size -= count;

	  if ((sym_hash)->root.u.def.value >= addr
	      && (sym_hash)->root.u.def.value < addr + count)
	    (sym_hash)->root.u.def.value = addr;

	  bfd_elf32_swap_symbol_out (abfd, & isym, esym, shndx);
	}

      if (shndx)
	++ shndx;
    }

  return TRUE;
}

#define NOP_OPCODE 	(0x0000)
#define MOVHI	    	0x0640				/* 4byte */
#define MOVHI_MASK  	0x07e0
#define MOVHI_R1(insn)	((insn) & 0x1f)			/* 4byte */
#define MOVHI_R2(insn)	((insn) >> 11)
#define MOVEA	    	0x0620				/* 2byte */
#define MOVEA_MASK  	0x07e0
#define MOVEA_R1(insn)	((insn) & 0x1f)
#define MOVEA_R2(insn)	((insn) >> 11)
#define JARL_4	    	0x00040780				/* 4byte */
#define JARL_4_MASK 	0xFFFF07FF
#define JARL_R2(insn)	(int)(((insn) & (~JARL_4_MASK)) >> 11)
#define ADD_I       	0x0240					/* 2byte */
#define ADD_I_MASK  	0x07e0
#define ADD_I5(insn)	((((insn) & 0x001f) << 11) >> 11)	/* 2byte */
#define ADD_R2(insn)	((insn) >> 11)
#define JMP_R	    	0x0060					/* 2byte */
#define JMP_R_MASK 	0xFFE0
#define JMP_R1(insn)	((insn) & 0x1f)

static bfd_boolean
v850_elf_relax_section (bfd *abfd,
			asection *sec,
			struct bfd_link_info *link_info,
			bfd_boolean *again)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel;
  Elf_Internal_Rela *irelend;
  Elf_Internal_Rela *irelalign = NULL;
  Elf_Internal_Sym *isymbuf = NULL;
  bfd_byte *contents = NULL;
  bfd_vma addr = 0;
  bfd_vma toaddr;
  int align_pad_size = 0;
  bfd_boolean result = TRUE;

  *again = FALSE;

  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0)
    return TRUE;

  symtab_hdr = & elf_tdata (abfd)->symtab_hdr;

  internal_relocs = (_bfd_elf_link_read_relocs
		     (abfd, sec, NULL, NULL, link_info->keep_memory));
  if (internal_relocs == NULL)
    goto error_return;

  irelend = internal_relocs + sec->reloc_count;

  while (addr < sec->size)
    {
      toaddr = sec->size;

      for (irel = internal_relocs; irel < irelend; irel ++)
	if (ELF32_R_TYPE (irel->r_info) == (int) R_V850_ALIGN
	    && irel->r_offset > addr
	    && irel->r_offset < toaddr)
	  toaddr = irel->r_offset;

#ifdef DEBUG_RELAX
      fprintf (stderr, "relax region 0x%x to 0x%x align pad %d\n",
	       addr, toaddr, align_pad_size);
#endif
      if (irelalign)
	{
	  bfd_vma alignto;
	  bfd_vma alignmoveto;

	  alignmoveto = BFD_ALIGN (addr - align_pad_size, 1 << irelalign->r_addend);
	  alignto = BFD_ALIGN (addr, 1 << irelalign->r_addend);

	  if (alignmoveto < alignto)
	    {
	      unsigned int i;

	      align_pad_size = alignto - alignmoveto;
#ifdef DEBUG_RELAX
	      fprintf (stderr, "relax move region 0x%x to 0x%x delete size 0x%x\n",
		       alignmoveto, toaddr, align_pad_size);
#endif
	      if (!v850_elf_relax_delete_bytes (abfd, sec, alignmoveto,
						toaddr, align_pad_size))
		goto error_return;

	      for (i  = BFD_ALIGN (toaddr - align_pad_size, 1);
		   (i + 1) < toaddr; i += 2)
		bfd_put_16 (abfd, NOP_OPCODE, contents + i);

	      addr = alignmoveto;
	    }
	  else
	    align_pad_size = 0;
	}

      for (irel = internal_relocs; irel < irelend; irel++)
	{
	  bfd_vma laddr;
	  bfd_vma addend;
	  bfd_vma symval;
	  int insn[5];
	  int no_match = -1;
	  Elf_Internal_Rela *hi_irelfn;
	  Elf_Internal_Rela *lo_irelfn;
	  Elf_Internal_Rela *irelcall;
	  bfd_signed_vma foff;

	  if (! (irel->r_offset >= addr && irel->r_offset < toaddr
		 && (ELF32_R_TYPE (irel->r_info) == (int) R_V850_LONGCALL
		     || ELF32_R_TYPE (irel->r_info) == (int) R_V850_LONGJUMP)))
	    continue;

#ifdef DEBUG_RELAX
	  fprintf (stderr, "relax check r_info 0x%x r_offset 0x%x r_addend 0x%x\n",
		   irel->r_info,
		   irel->r_offset,
		   irel->r_addend );
#endif

	  /* Get the section contents.  */
	  if (contents == NULL)
	    {
	      if (elf_section_data (sec)->this_hdr.contents != NULL)
		contents = elf_section_data (sec)->this_hdr.contents;
	      else
		{
		  if (! bfd_malloc_and_get_section (abfd, sec, &contents))
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

	  laddr = irel->r_offset;

	  if (ELF32_R_TYPE (irel->r_info) == (int) R_V850_LONGCALL)
	    {
	      /* Check code for -mlong-calls output. */
	      if (laddr + 16 <= (bfd_vma) sec->size)
		{
		  insn[0] = bfd_get_16 (abfd, contents + laddr);
		  insn[1] = bfd_get_16 (abfd, contents + laddr + 4);
		  insn[2] = bfd_get_32 (abfd, contents + laddr + 8);
		  insn[3] = bfd_get_16 (abfd, contents + laddr + 12);
		  insn[4] = bfd_get_16 (abfd, contents + laddr + 14);

		  if ((insn[0] & MOVHI_MASK) != MOVHI
		       || MOVHI_R1 (insn[0]) != 0)
		    no_match = 0;

		  if (no_match < 0
		      && ((insn[1] & MOVEA_MASK) != MOVEA
			   || MOVHI_R2 (insn[0]) != MOVEA_R1 (insn[1])))
		    no_match = 1;

		  if (no_match < 0
		      && (insn[2] & JARL_4_MASK) != JARL_4)
		    no_match = 2;

		  if (no_match < 0
		      && ((insn[3] & ADD_I_MASK) != ADD_I
			   || ADD_I5 (insn[3]) != 4
			   || JARL_R2 (insn[2]) != ADD_R2 (insn[3])))
		    no_match = 3;

		  if (no_match < 0
		      && ((insn[4] & JMP_R_MASK) != JMP_R
			   || MOVEA_R2 (insn[1]) != JMP_R1 (insn[4])))
		    no_match = 4;
		}
	      else
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGCALL points to unrecognized insns",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset));

		  continue;
		}

	      if (no_match >= 0)
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGCALL points to unrecognized insn 0x%x",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset+no_match, insn[no_match]));

		  continue;
		}

	      /* Get the reloc for the address from which the register is
	         being loaded.  This reloc will tell us which function is
	         actually being called.  */
	      for (hi_irelfn = internal_relocs; hi_irelfn < irelend; hi_irelfn ++)
		if (hi_irelfn->r_offset == laddr + 2
		    && ELF32_R_TYPE (hi_irelfn->r_info)
		        == (int) R_V850_HI16_S)
		  break;

	      for (lo_irelfn = internal_relocs; lo_irelfn < irelend; lo_irelfn ++)
		if (lo_irelfn->r_offset == laddr + 6
		    && ELF32_R_TYPE (lo_irelfn->r_info)
		        == (int) R_V850_LO16)
		  break;

	      for (irelcall = internal_relocs; irelcall < irelend; irelcall ++)
		if (irelcall->r_offset == laddr + 8
		    && ELF32_R_TYPE (irelcall->r_info)
                        == (int) R_V850_22_PCREL)
		  break;

	      if (   hi_irelfn == irelend
		  || lo_irelfn == irelend
		  || irelcall  == irelend)
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGCALL points to unrecognized reloc",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset ));

		  continue;
		}

	      if (ELF32_R_SYM (irelcall->r_info) < symtab_hdr->sh_info)
		{
		  Elf_Internal_Sym *  isym;

		  /* A local symbol.  */
		  isym = isymbuf + ELF32_R_SYM (irelcall->r_info);

		  symval = isym->st_value;
		}
	      else
		{
		  unsigned long indx;
		  struct elf_link_hash_entry * h;

		  /* An external symbol.  */
		  indx = ELF32_R_SYM (irelcall->r_info) - symtab_hdr->sh_info;
		  h = elf_sym_hashes (abfd)[indx];
		  BFD_ASSERT (h != NULL);

		  if (   h->root.type != bfd_link_hash_defined
		      && h->root.type != bfd_link_hash_defweak)
		    /* This appears to be a reference to an undefined
		       symbol.  Just ignore it--it will be caught by the
		       regular reloc processing.  */
		    continue;

		  symval = h->root.u.def.value;
		}

	      if (symval + irelcall->r_addend != irelcall->r_offset + 4)
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGCALL points to unrecognized reloc 0x%lx",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset, irelcall->r_offset ));

		  continue;
		}

	      /* Get the value of the symbol referred to by the reloc.  */
	      if (ELF32_R_SYM (hi_irelfn->r_info) < symtab_hdr->sh_info)
		{
		  Elf_Internal_Sym *isym;
		  asection *sym_sec;

		  /* A local symbol.  */
		  isym = isymbuf + ELF32_R_SYM (hi_irelfn->r_info);

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
		  indx = ELF32_R_SYM (hi_irelfn->r_info) - symtab_hdr->sh_info;
		  h = elf_sym_hashes (abfd)[indx];
		  BFD_ASSERT (h != NULL);

		  if (   h->root.type != bfd_link_hash_defined
		      && h->root.type != bfd_link_hash_defweak)
		    /* This appears to be a reference to an undefined
		       symbol.  Just ignore it--it will be caught by the
		       regular reloc processing.  */
		    continue;

		  symval = (h->root.u.def.value
			    + h->root.u.def.section->output_section->vma
			    + h->root.u.def.section->output_offset);
		}

	      addend = irel->r_addend;

	      foff = (symval + addend
		      - (irel->r_offset
			 + sec->output_section->vma
			 + sec->output_offset
			 + 4));
#ifdef DEBUG_RELAX
	      fprintf (stderr, "relax longcall r_offset 0x%x ptr 0x%x symbol 0x%x addend 0x%x distance 0x%x\n",
		       irel->r_offset,
		       (irel->r_offset
			+ sec->output_section->vma
			+ sec->output_offset),
		       symval, addend, foff);
#endif

	      if (foff < -0x100000 || foff >= 0x100000)
		/* After all that work, we can't shorten this function call.  */
		continue;

	      /* For simplicity of coding, we are going to modify the section
	         contents, the section relocs, and the BFD symbol table.  We
	         must tell the rest of the code not to free up this
	         information.  It would be possible to instead create a table
	         of changes which have to be made, as is done in coff-mips.c;
	         that would be more work, but would require less memory when
	         the linker is run.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (bfd_byte *) isymbuf;

	      /* Replace the long call with a jarl.  */
	      irel->r_info = ELF32_R_INFO (ELF32_R_SYM (hi_irelfn->r_info), R_V850_22_PCREL);

	      addend = 0;

	      if (ELF32_R_SYM (hi_irelfn->r_info) < symtab_hdr->sh_info)
		/* If this needs to be changed because of future relaxing,
		   it will be handled here like other internal IND12W
		   relocs.  */
		bfd_put_32 (abfd,
			    0x00000780 | (JARL_R2 (insn[2])<<11) | ((addend << 16) & 0xffff) | ((addend >> 16) & 0xf),
			    contents + irel->r_offset);
	      else
		/* We can't fully resolve this yet, because the external
		   symbol value may be changed by future relaxing.
		   We let the final link phase handle it.  */
		bfd_put_32 (abfd, 0x00000780 | (JARL_R2 (insn[2])<<11),
			    contents + irel->r_offset);

	      hi_irelfn->r_info =
		ELF32_R_INFO (ELF32_R_SYM (hi_irelfn->r_info), R_V850_NONE);
	      lo_irelfn->r_info =
		ELF32_R_INFO (ELF32_R_SYM (lo_irelfn->r_info), R_V850_NONE);
	      irelcall->r_info =
		ELF32_R_INFO (ELF32_R_SYM (irelcall->r_info), R_V850_NONE);

	      if (! v850_elf_relax_delete_bytes (abfd, sec,
						 irel->r_offset + 4, toaddr, 12))
		goto error_return;

	      align_pad_size += 12;
	    }
	  else if (ELF32_R_TYPE (irel->r_info) == (int) R_V850_LONGJUMP)
	    {
	      /* Check code for -mlong-jumps output.  */
	      if (laddr + 10 <= (bfd_vma) sec->size)
		{
		  insn[0] = bfd_get_16 (abfd, contents + laddr);
		  insn[1] = bfd_get_16 (abfd, contents + laddr + 4);
		  insn[2] = bfd_get_16 (abfd, contents + laddr + 8);

		  if ((insn[0] & MOVHI_MASK) != MOVHI
		       || MOVHI_R1 (insn[0]) != 0)
		    no_match = 0;

		  if (no_match < 0
		      && ((insn[1] & MOVEA_MASK) != MOVEA
			   || MOVHI_R2 (insn[0]) != MOVEA_R1 (insn[1])))
		    no_match = 1;

		  if (no_match < 0
		      && ((insn[2] & JMP_R_MASK) != JMP_R
			   || MOVEA_R2 (insn[1]) != JMP_R1 (insn[2])))
		    no_match = 4;
		}
	      else
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGJUMP points to unrecognized insns",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset));

		  continue;
		}

	      if (no_match >= 0)
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGJUMP points to unrecognized insn 0x%x",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset+no_match, insn[no_match]));

		  continue;
		}

	      /* Get the reloc for the address from which the register is
	         being loaded.  This reloc will tell us which function is
	         actually being called.  */
	      for (hi_irelfn = internal_relocs; hi_irelfn < irelend; hi_irelfn ++)
		if (hi_irelfn->r_offset == laddr + 2
		    && ELF32_R_TYPE (hi_irelfn->r_info) == (int) R_V850_HI16_S)
		  break;

	      for (lo_irelfn = internal_relocs; lo_irelfn < irelend; lo_irelfn ++)
		if (lo_irelfn->r_offset == laddr + 6
		    && ELF32_R_TYPE (lo_irelfn->r_info) == (int) R_V850_LO16)
		  break;

	      if (   hi_irelfn == irelend
		  || lo_irelfn == irelend)
		{
		  ((*_bfd_error_handler)
		   ("%s: 0x%lx: warning: R_V850_LONGJUMP points to unrecognized reloc",
		    bfd_get_filename (abfd), (unsigned long) irel->r_offset ));

		  continue;
		}

	      /* Get the value of the symbol referred to by the reloc.  */
	      if (ELF32_R_SYM (hi_irelfn->r_info) < symtab_hdr->sh_info)
		{
		  Elf_Internal_Sym *  isym;
		  asection *          sym_sec;

		  /* A local symbol.  */
		  isym = isymbuf + ELF32_R_SYM (hi_irelfn->r_info);

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
#ifdef DEBUG_RELAX
		  {
		    char * name = bfd_elf_string_from_elf_section
		      (abfd, symtab_hdr->sh_link, isym->st_name);

		    fprintf (stderr, "relax long jump local: sec: %s, sym: %s (%d), value: %x + %x + %x addend %x\n",
			     sym_sec->name, name, isym->st_name,
			     sym_sec->output_section->vma,
			     sym_sec->output_offset,
			     isym->st_value, irel->r_addend);
		  }
#endif
		}
	      else
		{
		  unsigned long indx;
		  struct elf_link_hash_entry * h;

		  /* An external symbol.  */
		  indx = ELF32_R_SYM (irel->r_info) - symtab_hdr->sh_info;
		  h = elf_sym_hashes (abfd)[indx];
		  BFD_ASSERT (h != NULL);

		  if (   h->root.type != bfd_link_hash_defined
		      && h->root.type != bfd_link_hash_defweak)
		    /* This appears to be a reference to an undefined
		       symbol.  Just ignore it--it will be caught by the
		       regular reloc processing.  */
		    continue;

		  symval = (h->root.u.def.value
			    + h->root.u.def.section->output_section->vma
			    + h->root.u.def.section->output_offset);
#ifdef DEBUG_RELAX
		  fprintf (stderr,
			   "relax longjump defined: sec: %s, name: %s, value: %x + %x + %x addend %x\n",
			   sec->name, h->root.root.string, h->root.u.def.value,
			   sec->output_section->vma, sec->output_offset, irel->r_addend);
#endif
		}

	      addend = irel->r_addend;

	      foff = (symval + addend
		      - (irel->r_offset
			 + sec->output_section->vma
			 + sec->output_offset
			 + 4));
#ifdef DEBUG_RELAX
	      fprintf (stderr, "relax longjump r_offset 0x%x ptr 0x%x symbol 0x%x addend 0x%x distance 0x%x\n",
		       irel->r_offset,
		       (irel->r_offset
			+ sec->output_section->vma
			+ sec->output_offset),
		       symval, addend, foff);
#endif
	      if (foff < -0x100000 || foff >= 0x100000)
		/* After all that work, we can't shorten this function call.  */
		continue;

	      /* For simplicity of coding, we are going to modify the section
	         contents, the section relocs, and the BFD symbol table.  We
	         must tell the rest of the code not to free up this
	         information.  It would be possible to instead create a table
	         of changes which have to be made, as is done in coff-mips.c;
	         that would be more work, but would require less memory when
	         the linker is run.  */
	      elf_section_data (sec)->relocs = internal_relocs;
	      elf_section_data (sec)->this_hdr.contents = contents;
	      symtab_hdr->contents = (bfd_byte *) isymbuf;

	      if (foff < -0x100 || foff >= 0x100)
		{
		  /* Replace the long jump with a jr.  */

		  irel->r_info =
		    ELF32_R_INFO (ELF32_R_SYM (irel->r_info), R_V850_22_PCREL);

		  irel->r_addend = addend;
		  addend = 0;

		  if (ELF32_R_SYM (hi_irelfn->r_info) < symtab_hdr->sh_info)
		    /* If this needs to be changed because of future relaxing,
		       it will be handled here like other internal IND12W
		       relocs.  */
		    bfd_put_32 (abfd,
				0x00000780 | ((addend << 15) & 0xffff0000) | ((addend >> 17) & 0xf),
				contents + irel->r_offset);
		  else
		    /* We can't fully resolve this yet, because the external
		       symbol value may be changed by future relaxing.
		       We let the final link phase handle it.  */
		    bfd_put_32 (abfd, 0x00000780, contents + irel->r_offset);

		  hi_irelfn->r_info =
			ELF32_R_INFO (ELF32_R_SYM (hi_irelfn->r_info), R_V850_NONE);
		  lo_irelfn->r_info =
			ELF32_R_INFO (ELF32_R_SYM (lo_irelfn->r_info), R_V850_NONE);
		  if (!v850_elf_relax_delete_bytes (abfd, sec,
						    irel->r_offset + 4, toaddr, 6))
		    goto error_return;

		  align_pad_size += 6;
		}
	      else
		{
		  /* Replace the long jump with a br.  */

		  irel->r_info =
			ELF32_R_INFO (ELF32_R_SYM (irel->r_info), R_V850_9_PCREL);

		  irel->r_addend = addend;
		  addend = 0;

		  if (ELF32_R_SYM (hi_irelfn->r_info) < symtab_hdr->sh_info)
		    /* If this needs to be changed because of future relaxing,
		       it will be handled here like other internal IND12W
		       relocs.  */
		    bfd_put_16 (abfd,
				0x0585 | ((addend << 10) & 0xf800) | ((addend << 3) & 0x0070),
				contents + irel->r_offset);
		  else
		    /* We can't fully resolve this yet, because the external
		       symbol value may be changed by future relaxing.
		       We let the final link phase handle it.  */
		    bfd_put_16 (abfd, 0x0585, contents + irel->r_offset);

		  hi_irelfn->r_info =
			ELF32_R_INFO (ELF32_R_SYM (hi_irelfn->r_info), R_V850_NONE);
		  lo_irelfn->r_info =
			ELF32_R_INFO (ELF32_R_SYM (lo_irelfn->r_info), R_V850_NONE);
		  if (!v850_elf_relax_delete_bytes (abfd, sec,
						    irel->r_offset + 2, toaddr, 8))
		    goto error_return;

		  align_pad_size += 8;
		}
	    }
	}

      irelalign = NULL;
      for (irel = internal_relocs; irel < irelend; irel++)
	{
	  if (ELF32_R_TYPE (irel->r_info) == (int) R_V850_ALIGN
	      && irel->r_offset == toaddr)
	    {
	      irel->r_offset -= align_pad_size;

	      if (irelalign == NULL || irelalign->r_addend > irel->r_addend)
		irelalign = irel;
	    }
	}

      addr = toaddr;
    }

  if (!irelalign)
    {
#ifdef DEBUG_RELAX
      fprintf (stderr, "relax pad %d shorten %d -> %d\n",
	       align_pad_size,
	       sec->size,
	       sec->size - align_pad_size);
#endif
      sec->size -= align_pad_size;
    }

 finish:
  if (internal_relocs != NULL
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);

  if (contents != NULL
      && elf_section_data (sec)->this_hdr.contents != (unsigned char *) contents)
    free (contents);

  if (isymbuf != NULL
      && symtab_hdr->contents != (bfd_byte *) isymbuf)
    free (isymbuf);

  return result;

 error_return:
  result = FALSE;
  goto finish;
}

static const struct bfd_elf_special_section v850_elf_special_sections[] =
{
  { ".call_table_data", 16,  0, SHT_PROGBITS,     (SHF_ALLOC
                                                   + SHF_WRITE) },
  { ".call_table_text", 16,  0, SHT_PROGBITS,     (SHF_ALLOC + SHF_WRITE
                                                   + SHF_EXECINSTR) },
  { ".rosdata",          8, -2, SHT_PROGBITS,     (SHF_ALLOC
                                                   + SHF_V850_GPREL) },
  { ".rozdata",          8, -2, SHT_PROGBITS,     (SHF_ALLOC
                                                   + SHF_V850_R0REL) },
  { ".sbss",             5, -2, SHT_NOBITS,       (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_GPREL) },
  { ".scommon",          8, -2, SHT_V850_SCOMMON, (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_GPREL) },
  { ".sdata",            6, -2, SHT_PROGBITS,     (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_GPREL) },
  { ".tbss",             5, -2, SHT_NOBITS,       (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_EPREL) },
  { ".tcommon",          8, -2, SHT_V850_TCOMMON, (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_R0REL) },
  { ".tdata",            6, -2, SHT_PROGBITS,     (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_EPREL) },
  { ".zbss",             5, -2, SHT_NOBITS,       (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_R0REL) },
  { ".zcommon",          8, -2, SHT_V850_ZCOMMON, (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_R0REL) },
  { ".zdata",            6, -2, SHT_PROGBITS,     (SHF_ALLOC + SHF_WRITE
                                                   + SHF_V850_R0REL) },
  { NULL,        0, 0, 0,            0 }
};

#define TARGET_LITTLE_SYM			bfd_elf32_v850_vec
#define TARGET_LITTLE_NAME			"elf32-v850"
#define ELF_ARCH				bfd_arch_v850
#define ELF_MACHINE_CODE			EM_V850
#define ELF_MACHINE_ALT1			EM_CYGNUS_V850
#define ELF_MACHINE_ALT2			EM_V800 /* This is the value used by the GreenHills toolchain.  */
#define ELF_MAXPAGESIZE				0x1000

#define elf_info_to_howto			v850_elf_info_to_howto_rela
#define elf_info_to_howto_rel			v850_elf_info_to_howto_rel

#define elf_backend_check_relocs		v850_elf_check_relocs
#define elf_backend_relocate_section    	v850_elf_relocate_section
#define elf_backend_object_p			v850_elf_object_p
#define elf_backend_final_write_processing 	v850_elf_final_write_processing
#define elf_backend_section_from_bfd_section 	v850_elf_section_from_bfd_section
#define elf_backend_symbol_processing		v850_elf_symbol_processing
#define elf_backend_add_symbol_hook		v850_elf_add_symbol_hook
#define elf_backend_link_output_symbol_hook 	v850_elf_link_output_symbol_hook
#define elf_backend_section_from_shdr		v850_elf_section_from_shdr
#define elf_backend_fake_sections		v850_elf_fake_sections
#define elf_backend_gc_mark_hook                v850_elf_gc_mark_hook
#define elf_backend_gc_sweep_hook               v850_elf_gc_sweep_hook
#define elf_backend_special_sections		v850_elf_special_sections

#define elf_backend_can_gc_sections 1
#define elf_backend_rela_normal 1

#define bfd_elf32_bfd_is_local_label_name	v850_elf_is_local_label_name
#define bfd_elf32_bfd_reloc_type_lookup		v850_elf_reloc_type_lookup
#define bfd_elf32_bfd_merge_private_bfd_data 	v850_elf_merge_private_bfd_data
#define bfd_elf32_bfd_set_private_flags		v850_elf_set_private_flags
#define bfd_elf32_bfd_print_private_bfd_data	v850_elf_print_private_bfd_data
#define bfd_elf32_bfd_relax_section		v850_elf_relax_section

#define elf_symbol_leading_char			'_'

#include "elf32-target.h"
