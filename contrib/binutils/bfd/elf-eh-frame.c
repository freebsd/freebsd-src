/* .eh_frame section optimization.
   Copyright 2001, 2002, 2003 Free Software Foundation, Inc.
   Written by Jakub Jelinek <jakub@redhat.com>.

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

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/dwarf2.h"

#define EH_FRAME_HDR_SIZE 8

/* Helper function for reading uleb128 encoded data.  */

static bfd_vma
read_unsigned_leb128 (bfd *abfd ATTRIBUTE_UNUSED,
		      char *buf,
		      unsigned int *bytes_read_ptr)
{
  bfd_vma result;
  unsigned int num_read;
  int shift;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  do
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= (((bfd_vma) byte & 0x7f) << shift);
      shift += 7;
    }
  while (byte & 0x80);
  *bytes_read_ptr = num_read;
  return result;
}

/* Helper function for reading sleb128 encoded data.  */

static bfd_signed_vma
read_signed_leb128 (bfd *abfd ATTRIBUTE_UNUSED,
		    char *buf,
		    unsigned int * bytes_read_ptr)
{
  bfd_vma result;
  int shift;
  int num_read;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  do
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf ++;
      num_read ++;
      result |= (((bfd_vma) byte & 0x7f) << shift);
      shift += 7;
    }
  while (byte & 0x80);
  if (byte & 0x40)
    result |= (((bfd_vma) -1) << (shift - 7)) << 7;
  *bytes_read_ptr = num_read;
  return result;
}

#define read_uleb128(VAR, BUF)					\
do								\
  {								\
    (VAR) = read_unsigned_leb128 (abfd, buf, &leb128_tmp);	\
    (BUF) += leb128_tmp;					\
  }								\
while (0)

#define read_sleb128(VAR, BUF)					\
do								\
  {								\
    (VAR) = read_signed_leb128 (abfd, buf, &leb128_tmp);	\
    (BUF) += leb128_tmp;					\
  }								\
while (0)

/* Return 0 if either encoding is variable width, or not yet known to bfd.  */

static
int get_DW_EH_PE_width (int encoding, int ptr_size)
{
  /* DW_EH_PE_ values of 0x60 and 0x70 weren't defined at the time .eh_frame
     was added to bfd.  */
  if ((encoding & 0x60) == 0x60)
    return 0;

  switch (encoding & 7)
    {
    case DW_EH_PE_udata2: return 2;
    case DW_EH_PE_udata4: return 4;
    case DW_EH_PE_udata8: return 8;
    case DW_EH_PE_absptr: return ptr_size;
    default:
      break;
    }

  return 0;
}

#define get_DW_EH_PE_signed(encoding) (((encoding) & DW_EH_PE_signed) != 0)

/* Read a width sized value from memory.  */

static bfd_vma
read_value (bfd *abfd, bfd_byte *buf, int width, int is_signed)
{
  bfd_vma value;

  switch (width)
    {
    case 2:
      if (is_signed)
	value = bfd_get_signed_16 (abfd, buf);
      else
	value = bfd_get_16 (abfd, buf);
      break;
    case 4:
      if (is_signed)
	value = bfd_get_signed_32 (abfd, buf);
      else
	value = bfd_get_32 (abfd, buf);
      break;
    case 8:
      if (is_signed)
	value = bfd_get_signed_64 (abfd, buf);
      else
	value = bfd_get_64 (abfd, buf);
      break;
    default:
      BFD_FAIL ();
      return 0;
    }

  return value;
}

/* Store a width sized value to memory.  */

static void
write_value (bfd *abfd, bfd_byte *buf, bfd_vma value, int width)
{
  switch (width)
    {
    case 2: bfd_put_16 (abfd, value, buf); break;
    case 4: bfd_put_32 (abfd, value, buf); break;
    case 8: bfd_put_64 (abfd, value, buf); break;
    default: BFD_FAIL ();
    }
}

/* Return zero if C1 and C2 CIEs can be merged.  */

static
int cie_compare (struct cie *c1, struct cie *c2)
{
  if (c1->hdr.length == c2->hdr.length
      && c1->version == c2->version
      && strcmp (c1->augmentation, c2->augmentation) == 0
      && strcmp (c1->augmentation, "eh") != 0
      && c1->code_align == c2->code_align
      && c1->data_align == c2->data_align
      && c1->ra_column == c2->ra_column
      && c1->augmentation_size == c2->augmentation_size
      && c1->personality == c2->personality
      && c1->per_encoding == c2->per_encoding
      && c1->lsda_encoding == c2->lsda_encoding
      && c1->fde_encoding == c2->fde_encoding
      && c1->initial_insn_length == c2->initial_insn_length
      && memcmp (c1->initial_instructions,
		 c2->initial_instructions,
		 c1->initial_insn_length) == 0)
    return 0;

  return 1;
}

/* This function is called for each input file before the .eh_frame
   section is relocated.  It discards duplicate CIEs and FDEs for discarded
   functions.  The function returns TRUE iff any entries have been
   deleted.  */

bfd_boolean
_bfd_elf_discard_section_eh_frame
   (bfd *abfd, struct bfd_link_info *info, asection *sec,
    bfd_boolean (*reloc_symbol_deleted_p) (bfd_vma, void *),
    struct elf_reloc_cookie *cookie)
{
  bfd_byte *ehbuf = NULL, *buf;
  bfd_byte *last_cie, *last_fde;
  struct cie_header hdr;
  struct cie cie;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  struct eh_frame_sec_info *sec_info = NULL;
  unsigned int leb128_tmp;
  unsigned int cie_usage_count, last_cie_ndx, i, offset;
  unsigned int make_relative, make_lsda_relative;
  bfd_size_type new_size;
  unsigned int ptr_size;

  if (sec->_raw_size == 0)
    {
      /* This file does not contain .eh_frame information.  */
      return FALSE;
    }

  if ((sec->output_section != NULL
       && bfd_is_abs_section (sec->output_section)))
    {
      /* At least one of the sections is being discarded from the
         link, so we should just ignore them.  */
      return FALSE;
    }

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;

  /* Read the frame unwind information from abfd.  */

  ehbuf = bfd_malloc (sec->_raw_size);
  if (ehbuf == NULL)
    goto free_no_table;

  if (! bfd_get_section_contents (abfd, sec, ehbuf, 0, sec->_raw_size))
    goto free_no_table;

  if (sec->_raw_size >= 4
      && bfd_get_32 (abfd, ehbuf) == 0
      && cookie->rel == cookie->relend)
    {
      /* Empty .eh_frame section.  */
      free (ehbuf);
      return FALSE;
    }

  /* If .eh_frame section size doesn't fit into int, we cannot handle
     it (it would need to use 64-bit .eh_frame format anyway).  */
  if (sec->_raw_size != (unsigned int) sec->_raw_size)
    goto free_no_table;

  ptr_size = (elf_elfheader (abfd)->e_ident[EI_CLASS]
	      == ELFCLASS64) ? 8 : 4;
  buf = ehbuf;
  last_cie = NULL;
  last_cie_ndx = 0;
  memset (&cie, 0, sizeof (cie));
  cie_usage_count = 0;
  new_size = sec->_raw_size;
  make_relative = hdr_info->last_cie.make_relative;
  make_lsda_relative = hdr_info->last_cie.make_lsda_relative;
  sec_info = bfd_zmalloc (sizeof (struct eh_frame_sec_info)
			  + 99 * sizeof (struct eh_cie_fde));
  if (sec_info == NULL)
    goto free_no_table;
  sec_info->alloced = 100;

#define ENSURE_NO_RELOCS(buf)				\
  if (cookie->rel < cookie->relend			\
      && (cookie->rel->r_offset				\
	  < (bfd_size_type) ((buf) - ehbuf))		\
      && cookie->rel->r_info != 0)			\
    goto free_no_table

#define SKIP_RELOCS(buf)				\
  while (cookie->rel < cookie->relend			\
         && (cookie->rel->r_offset			\
	     < (bfd_size_type) ((buf) - ehbuf)))	\
    cookie->rel++

#define GET_RELOC(buf)					\
  ((cookie->rel < cookie->relend			\
    && (cookie->rel->r_offset				\
        == (bfd_size_type) ((buf) - ehbuf)))		\
   ? cookie->rel : NULL)

  for (;;)
    {
      unsigned char *aug;

      if (sec_info->count == sec_info->alloced)
	{
	  sec_info = bfd_realloc (sec_info,
				  sizeof (struct eh_frame_sec_info)
				  + (sec_info->alloced + 99)
				     * sizeof (struct eh_cie_fde));
	  if (sec_info == NULL)
	    goto free_no_table;

	  memset (&sec_info->entry[sec_info->alloced], 0,
		  100 * sizeof (struct eh_cie_fde));
	  sec_info->alloced += 100;
	}

      last_fde = buf;
      /* If we are at the end of the section, we still need to decide
	 on whether to output or discard last encountered CIE (if any).  */
      if ((bfd_size_type) (buf - ehbuf) == sec->_raw_size)
	hdr.id = (unsigned int) -1;
      else
	{
	  if ((bfd_size_type) (buf + 4 - ehbuf) > sec->_raw_size)
	    /* No space for CIE/FDE header length.  */
	    goto free_no_table;

	  hdr.length = bfd_get_32 (abfd, buf);
	  if (hdr.length == 0xffffffff)
	    /* 64-bit .eh_frame is not supported.  */
	    goto free_no_table;
	  buf += 4;
	  if ((bfd_size_type) (buf - ehbuf) + hdr.length > sec->_raw_size)
	    /* CIE/FDE not contained fully in this .eh_frame input section.  */
	    goto free_no_table;

	  sec_info->entry[sec_info->count].offset = last_fde - ehbuf;
	  sec_info->entry[sec_info->count].size = 4 + hdr.length;

	  if (hdr.length == 0)
	    {
	      /* CIE with length 0 must be only the last in the section.  */
	      if ((bfd_size_type) (buf - ehbuf) < sec->_raw_size)
		goto free_no_table;
	      ENSURE_NO_RELOCS (buf);
	      sec_info->count++;
	      /* Now just finish last encountered CIE processing and break
		 the loop.  */
	      hdr.id = (unsigned int) -1;
	    }
	  else
	    {
	      hdr.id = bfd_get_32 (abfd, buf);
	      buf += 4;
	      if (hdr.id == (unsigned int) -1)
		goto free_no_table;
	    }
	}

      if (hdr.id == 0 || hdr.id == (unsigned int) -1)
	{
	  unsigned int initial_insn_length;

	  /* CIE  */
	  if (last_cie != NULL)
	    {
	      /* Now check if this CIE is identical to the last CIE,
		 in which case we can remove it provided we adjust
		 all FDEs.  Also, it can be removed if we have removed
		 all FDEs using it.  */
	      if ((!info->relocatable
		   && hdr_info->last_cie_sec
		   && (sec->output_section
		       == hdr_info->last_cie_sec->output_section)
		   && cie_compare (&cie, &hdr_info->last_cie) == 0)
		  || cie_usage_count == 0)
		{
		  new_size -= cie.hdr.length + 4;
		  sec_info->entry[last_cie_ndx].removed = 1;
		  sec_info->entry[last_cie_ndx].sec = hdr_info->last_cie_sec;
		  sec_info->entry[last_cie_ndx].new_offset
		    = hdr_info->last_cie_offset;
		}
	      else
		{
		  hdr_info->last_cie = cie;
		  hdr_info->last_cie_sec = sec;
		  hdr_info->last_cie_offset = last_cie - ehbuf;
		  sec_info->entry[last_cie_ndx].make_relative
		    = cie.make_relative;
		  sec_info->entry[last_cie_ndx].make_lsda_relative
		    = cie.make_lsda_relative;
		  sec_info->entry[last_cie_ndx].per_encoding_relative
		    = (cie.per_encoding & 0x70) == DW_EH_PE_pcrel;
		}
	    }

	  if (hdr.id == (unsigned int) -1)
	    break;

	  last_cie_ndx = sec_info->count;
	  sec_info->entry[sec_info->count].cie = 1;

	  cie_usage_count = 0;
	  memset (&cie, 0, sizeof (cie));
	  cie.hdr = hdr;
	  cie.version = *buf++;

	  /* Cannot handle unknown versions.  */
	  if (cie.version != 1)
	    goto free_no_table;
	  if (strlen (buf) > sizeof (cie.augmentation) - 1)
	    goto free_no_table;

	  strcpy (cie.augmentation, buf);
	  buf = strchr (buf, '\0') + 1;
	  ENSURE_NO_RELOCS (buf);
	  if (buf[0] == 'e' && buf[1] == 'h')
	    {
	      /* GCC < 3.0 .eh_frame CIE */
	      /* We cannot merge "eh" CIEs because __EXCEPTION_TABLE__
		 is private to each CIE, so we don't need it for anything.
		 Just skip it.  */
	      buf += ptr_size;
	      SKIP_RELOCS (buf);
	    }
	  read_uleb128 (cie.code_align, buf);
	  read_sleb128 (cie.data_align, buf);
	  /* Note - in DWARF2 the return address column is an unsigned byte.
	     In DWARF3 it is a ULEB128.  We are following DWARF3.  For most
	     ports this will not matter as the value will be less than 128.
	     For the others (eg FRV, SH, MMIX, IA64) they need a fixed GCC
	     which conforms to the DWARF3 standard.  */
	  read_uleb128 (cie.ra_column, buf);
	  ENSURE_NO_RELOCS (buf);
	  cie.lsda_encoding = DW_EH_PE_omit;
	  cie.fde_encoding = DW_EH_PE_omit;
	  cie.per_encoding = DW_EH_PE_omit;
	  aug = cie.augmentation;
	  if (aug[0] != 'e' || aug[1] != 'h')
	    {
	      if (*aug == 'z')
		{
		  aug++;
		  read_uleb128 (cie.augmentation_size, buf);
	  	  ENSURE_NO_RELOCS (buf);
		}

	      while (*aug != '\0')
		switch (*aug++)
		  {
		  case 'L':
		    cie.lsda_encoding = *buf++;
		    ENSURE_NO_RELOCS (buf);
		    if (get_DW_EH_PE_width (cie.lsda_encoding, ptr_size) == 0)
		      goto free_no_table;
		    break;
		  case 'R':
		    cie.fde_encoding = *buf++;
		    ENSURE_NO_RELOCS (buf);
		    if (get_DW_EH_PE_width (cie.fde_encoding, ptr_size) == 0)
		      goto free_no_table;
		    break;
		  case 'P':
		    {
		      int per_width;

		      cie.per_encoding = *buf++;
		      per_width = get_DW_EH_PE_width (cie.per_encoding,
						      ptr_size);
		      if (per_width == 0)
			goto free_no_table;
		      if ((cie.per_encoding & 0xf0) == DW_EH_PE_aligned)
			buf = (ehbuf
			       + ((buf - ehbuf + per_width - 1)
				  & ~((bfd_size_type) per_width - 1)));
		      ENSURE_NO_RELOCS (buf);
		      /* Ensure we have a reloc here, against
			 a global symbol.  */
		      if (GET_RELOC (buf) != NULL)
			{
			  unsigned long r_symndx;

#ifdef BFD64
			  if (ptr_size == 8)
			    r_symndx = ELF64_R_SYM (cookie->rel->r_info);
			  else
#endif
			    r_symndx = ELF32_R_SYM (cookie->rel->r_info);
			  if (r_symndx >= cookie->locsymcount)
			    {
			      struct elf_link_hash_entry *h;

			      r_symndx -= cookie->extsymoff;
			      h = cookie->sym_hashes[r_symndx];

			      while (h->root.type == bfd_link_hash_indirect
				     || h->root.type == bfd_link_hash_warning)
				h = (struct elf_link_hash_entry *)
				    h->root.u.i.link;

			      cie.personality = h;
			    }
			  cookie->rel++;
			}
		      buf += per_width;
		    }
		    break;
		  default:
		    /* Unrecognized augmentation. Better bail out.  */
		    goto free_no_table;
		  }
	    }

	  /* For shared libraries, try to get rid of as many RELATIVE relocs
	     as possible.  */
          if (info->shared
	      && (get_elf_backend_data (abfd)
		  ->elf_backend_can_make_relative_eh_frame
		  (abfd, info, sec))
	      && (cie.fde_encoding & 0xf0) == DW_EH_PE_absptr)
	    cie.make_relative = 1;

	  if (info->shared
	      && (get_elf_backend_data (abfd)
		  ->elf_backend_can_make_lsda_relative_eh_frame
		  (abfd, info, sec))
	      && (cie.lsda_encoding & 0xf0) == DW_EH_PE_absptr)
	    cie.make_lsda_relative = 1;

	  /* If FDE encoding was not specified, it defaults to
	     DW_EH_absptr.  */
	  if (cie.fde_encoding == DW_EH_PE_omit)
	    cie.fde_encoding = DW_EH_PE_absptr;

	  initial_insn_length = cie.hdr.length - (buf - last_fde - 4);
	  if (initial_insn_length <= 50)
	    {
	      cie.initial_insn_length = initial_insn_length;
	      memcpy (cie.initial_instructions, buf, initial_insn_length);
	    }
	  buf += initial_insn_length;
	  ENSURE_NO_RELOCS (buf);
	  last_cie = last_fde;
	}
      else
	{
	  /* Ensure this FDE uses the last CIE encountered.  */
	  if (last_cie == NULL
	      || hdr.id != (unsigned int) (buf - 4 - last_cie))
	    goto free_no_table;

	  ENSURE_NO_RELOCS (buf);
	  if (GET_RELOC (buf) == NULL)
	    /* This should not happen.  */
	    goto free_no_table;
	  if ((*reloc_symbol_deleted_p) (buf - ehbuf, cookie))
	    {
	      /* This is a FDE against a discarded section.  It should
		 be deleted.  */
	      new_size -= hdr.length + 4;
	      sec_info->entry[sec_info->count].removed = 1;
	    }
	  else
	    {
	      if (info->shared
		  && (((cie.fde_encoding & 0xf0) == DW_EH_PE_absptr
		       && cie.make_relative == 0)
		      || (cie.fde_encoding & 0xf0) == DW_EH_PE_aligned))
		{
		  /* If a shared library uses absolute pointers
		     which we cannot turn into PC relative,
		     don't create the binary search table,
		     since it is affected by runtime relocations.  */
		  hdr_info->table = FALSE;
		}
	      cie_usage_count++;
	      hdr_info->fde_count++;
	    }
	  if (cie.lsda_encoding != DW_EH_PE_omit)
	    {
	      unsigned int dummy;

	      aug = buf;
	      buf += 2 * get_DW_EH_PE_width (cie.fde_encoding, ptr_size);
	      if (cie.augmentation[0] == 'z')
		read_uleb128 (dummy, buf);
	      /* If some new augmentation data is added before LSDA
		 in FDE augmentation area, this need to be adjusted.  */
	      sec_info->entry[sec_info->count].lsda_offset = (buf - aug);
	    }
	  buf = last_fde + 4 + hdr.length;
	  SKIP_RELOCS (buf);
	}

      sec_info->entry[sec_info->count].fde_encoding = cie.fde_encoding;
      sec_info->entry[sec_info->count].lsda_encoding = cie.lsda_encoding;
      sec_info->count++;
    }

  elf_section_data (sec)->sec_info = sec_info;
  sec->sec_info_type = ELF_INFO_TYPE_EH_FRAME;

  /* Ok, now we can assign new offsets.  */
  offset = 0;
  last_cie_ndx = 0;
  for (i = 0; i < sec_info->count; i++)
    {
      if (! sec_info->entry[i].removed)
	{
	  sec_info->entry[i].new_offset = offset;
	  offset += sec_info->entry[i].size;
	  if (sec_info->entry[i].cie)
	    {
	      last_cie_ndx = i;
	      make_relative = sec_info->entry[i].make_relative;
	      make_lsda_relative = sec_info->entry[i].make_lsda_relative;
	    }
	  else
	    {
	      sec_info->entry[i].make_relative = make_relative;
	      sec_info->entry[i].make_lsda_relative = make_lsda_relative;
	      sec_info->entry[i].per_encoding_relative = 0;
	    }
	}
      else if (sec_info->entry[i].cie && sec_info->entry[i].sec == sec)
	{
	  /* Need to adjust new_offset too.  */
	  BFD_ASSERT (sec_info->entry[last_cie_ndx].offset
		      == sec_info->entry[i].new_offset);
	  sec_info->entry[i].new_offset
	    = sec_info->entry[last_cie_ndx].new_offset;
	}
    }
  if (hdr_info->last_cie_sec == sec)
    {
      BFD_ASSERT (sec_info->entry[last_cie_ndx].offset
		  == hdr_info->last_cie_offset);
      hdr_info->last_cie_offset = sec_info->entry[last_cie_ndx].new_offset;
    }

  /* FIXME: Currently it is not possible to shrink sections to zero size at
     this point, so build a fake minimal CIE.  */
  if (new_size == 0)
    new_size = 16;

  /* Shrink the sec as needed.  */
  sec->_cooked_size = new_size;
  if (sec->_cooked_size == 0)
    sec->flags |= SEC_EXCLUDE;

  free (ehbuf);
  return new_size != sec->_raw_size;

free_no_table:
  if (ehbuf)
    free (ehbuf);
  if (sec_info)
    free (sec_info);
  hdr_info->table = FALSE;
  hdr_info->last_cie.hdr.length = 0;
  return FALSE;
}

/* This function is called for .eh_frame_hdr section after
   _bfd_elf_discard_section_eh_frame has been called on all .eh_frame
   input sections.  It finalizes the size of .eh_frame_hdr section.  */

bfd_boolean
_bfd_elf_discard_section_eh_frame_hdr (bfd *abfd, struct bfd_link_info *info)
{
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  asection *sec;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  sec = hdr_info->hdr_sec;
  if (sec == NULL)
    return FALSE;

  sec->_cooked_size = EH_FRAME_HDR_SIZE;
  if (hdr_info->table)
    sec->_cooked_size += 4 + hdr_info->fde_count * 8;

  /* Request program headers to be recalculated.  */
  elf_tdata (abfd)->program_header_size = 0;
  elf_tdata (abfd)->eh_frame_hdr = sec;
  return TRUE;
}

/* This function is called from size_dynamic_sections.
   It needs to decide whether .eh_frame_hdr should be output or not,
   because later on it is too late for calling _bfd_strip_section_from_output,
   since dynamic symbol table has been sized.  */

bfd_boolean
_bfd_elf_maybe_strip_eh_frame_hdr (struct bfd_link_info *info)
{
  asection *o;
  bfd *abfd;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  if (hdr_info->hdr_sec == NULL)
    return TRUE;

  if (bfd_is_abs_section (hdr_info->hdr_sec->output_section))
    {
      hdr_info->hdr_sec = NULL;
      return TRUE;
    }

  abfd = NULL;
  if (info->eh_frame_hdr)
    for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
      {
	/* Count only sections which have at least a single CIE or FDE.
	   There cannot be any CIE or FDE <= 8 bytes.  */
	o = bfd_get_section_by_name (abfd, ".eh_frame");
	if (o && o->_raw_size > 8 && !bfd_is_abs_section (o->output_section))
	  break;
      }

  if (abfd == NULL)
    {
      _bfd_strip_section_from_output (info, hdr_info->hdr_sec);
      hdr_info->hdr_sec = NULL;
      return TRUE;
    }

  hdr_info->table = TRUE;
  return TRUE;
}

/* Adjust an address in the .eh_frame section.  Given OFFSET within
   SEC, this returns the new offset in the adjusted .eh_frame section,
   or -1 if the address refers to a CIE/FDE which has been removed
   or to offset with dynamic relocation which is no longer needed.  */

bfd_vma
_bfd_elf_eh_frame_section_offset (bfd *output_bfd ATTRIBUTE_UNUSED,
				  asection *sec,
				  bfd_vma offset)
{
  struct eh_frame_sec_info *sec_info;
  unsigned int lo, hi, mid;

  if (sec->sec_info_type != ELF_INFO_TYPE_EH_FRAME)
    return offset;
  sec_info = elf_section_data (sec)->sec_info;

  if (offset >= sec->_raw_size)
    return offset - (sec->_cooked_size - sec->_raw_size);

  lo = 0;
  hi = sec_info->count;
  mid = 0;
  while (lo < hi)
    {
      mid = (lo + hi) / 2;
      if (offset < sec_info->entry[mid].offset)
	hi = mid;
      else if (offset
	       >= sec_info->entry[mid].offset + sec_info->entry[mid].size)
	lo = mid + 1;
      else
	break;
    }

  BFD_ASSERT (lo < hi);

  /* FDE or CIE was removed.  */
  if (sec_info->entry[mid].removed)
    return (bfd_vma) -1;

  /* If converting to DW_EH_PE_pcrel, there will be no need for run-time
     relocation against FDE's initial_location field.  */
  if (sec_info->entry[mid].make_relative
      && ! sec_info->entry[mid].cie
      && offset == sec_info->entry[mid].offset + 8)
    return (bfd_vma) -2;

  /* If converting LSDA pointers to DW_EH_PE_pcrel, there will be no need
     for run-time relocation against LSDA field.  */
  if (sec_info->entry[mid].make_lsda_relative
      && ! sec_info->entry[mid].cie
      && (offset == (sec_info->entry[mid].offset + 8
		     + sec_info->entry[mid].lsda_offset)))
    return (bfd_vma) -2;

  return (offset + sec_info->entry[mid].new_offset
	  - sec_info->entry[mid].offset);
}

/* Write out .eh_frame section.  This is called with the relocated
   contents.  */

bfd_boolean
_bfd_elf_write_section_eh_frame (bfd *abfd,
				 struct bfd_link_info *info,
				 asection *sec,
				 bfd_byte *contents)
{
  struct eh_frame_sec_info *sec_info;
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  unsigned int i;
  bfd_byte *p, *buf;
  unsigned int leb128_tmp;
  unsigned int cie_offset = 0;
  unsigned int ptr_size;

  ptr_size = (elf_elfheader (sec->owner)->e_ident[EI_CLASS]
	      == ELFCLASS64) ? 8 : 4;

  if (sec->sec_info_type != ELF_INFO_TYPE_EH_FRAME)
    return bfd_set_section_contents (abfd, sec->output_section, contents,
				     sec->output_offset, sec->_raw_size);
  sec_info = elf_section_data (sec)->sec_info;
  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  if (hdr_info->table && hdr_info->array == NULL)
    hdr_info->array
      = bfd_malloc (hdr_info->fde_count * sizeof(*hdr_info->array));
  if (hdr_info->array == NULL)
    hdr_info = NULL;

  p = contents;
  for (i = 0; i < sec_info->count; ++i)
    {
      if (sec_info->entry[i].removed)
	{
	  if (sec_info->entry[i].cie)
	    {
	      /* If CIE is removed due to no remaining FDEs referencing it
		 and there were no CIEs kept before it, sec_info->entry[i].sec
		 will be zero.  */
	      if (sec_info->entry[i].sec == NULL)
		cie_offset = 0;
	      else
		{
		  cie_offset = sec_info->entry[i].new_offset;
		  cie_offset += (sec_info->entry[i].sec->output_section->vma
				 + sec_info->entry[i].sec->output_offset
				 - sec->output_section->vma
				 - sec->output_offset);
		}
	    }
	  continue;
	}

      if (sec_info->entry[i].cie)
	{
	  /* CIE */
	  cie_offset = sec_info->entry[i].new_offset;
	  if (sec_info->entry[i].make_relative
	      || sec_info->entry[i].make_lsda_relative
	      || sec_info->entry[i].per_encoding_relative)
	    {
	      unsigned char *aug;
	      unsigned int action;
	      unsigned int dummy, per_width, per_encoding;

	      /* Need to find 'R' or 'L' augmentation's argument and modify
		 DW_EH_PE_* value.  */
	      action = (sec_info->entry[i].make_relative ? 1 : 0)
		       | (sec_info->entry[i].make_lsda_relative ? 2 : 0)
		       | (sec_info->entry[i].per_encoding_relative ? 4 : 0);
	      buf = contents + sec_info->entry[i].offset;
	      /* Skip length, id and version.  */
	      buf += 9;
	      aug = buf;
	      buf = strchr (buf, '\0') + 1;
	      read_uleb128 (dummy, buf);
	      read_sleb128 (dummy, buf);
	      read_uleb128 (dummy, buf);
	      if (*aug == 'z')
		{
		  read_uleb128 (dummy, buf);
		  aug++;
		}

	      while (action)
		switch (*aug++)
		  {
		  case 'L':
		    if (action & 2)
		      {
			BFD_ASSERT (*buf == sec_info->entry[i].lsda_encoding);
			*buf |= DW_EH_PE_pcrel;
			action &= ~2;
		      }
		    buf++;
		    break;
		  case 'P':
		    per_encoding = *buf++;
                    per_width = get_DW_EH_PE_width (per_encoding,
						    ptr_size);
		    BFD_ASSERT (per_width != 0);
		    BFD_ASSERT (((per_encoding & 0x70) == DW_EH_PE_pcrel)
				== sec_info->entry[i].per_encoding_relative);
		    if ((per_encoding & 0xf0) == DW_EH_PE_aligned)
		      buf = (contents
			     + ((buf - contents + per_width - 1)
				& ~((bfd_size_type) per_width - 1)));
		    if (action & 4)
		      {
			bfd_vma value;

			value = read_value (abfd, buf, per_width,
					    get_DW_EH_PE_signed
					    (per_encoding));
			value += (sec_info->entry[i].offset
				  - sec_info->entry[i].new_offset);
			write_value (abfd, buf, value, per_width);
			action &= ~4;
		      }
		    buf += per_width;
		    break;
		  case 'R':
		    if (action & 1)
		      {
			BFD_ASSERT (*buf == sec_info->entry[i].fde_encoding);
			*buf |= DW_EH_PE_pcrel;
			action &= ~1;
		      }
		    buf++;
		    break;
		  default:
		    BFD_FAIL ();
		  }
	    }
	}
      else if (sec_info->entry[i].size > 4)
	{
	  /* FDE */
	  bfd_vma value = 0, address;
	  unsigned int width;

	  buf = contents + sec_info->entry[i].offset;
	  /* Skip length.  */
	  buf += 4;
	  bfd_put_32 (abfd,
		      sec_info->entry[i].new_offset + 4 - cie_offset, buf);
	  buf += 4;
	  width = get_DW_EH_PE_width (sec_info->entry[i].fde_encoding,
				      ptr_size);
	  address = value = read_value (abfd, buf, width,
					get_DW_EH_PE_signed
					(sec_info->entry[i].fde_encoding));
	  if (value)
	    {
	      switch (sec_info->entry[i].fde_encoding & 0xf0)
		{
		case DW_EH_PE_indirect:
		case DW_EH_PE_textrel:
		  BFD_ASSERT (hdr_info == NULL);
		  break;
		case DW_EH_PE_datarel:
		  {
		    asection *got = bfd_get_section_by_name (abfd, ".got");

		    BFD_ASSERT (got != NULL);
		    address += got->vma;
		  }
		  break;
		case DW_EH_PE_pcrel:
		  value += (sec_info->entry[i].offset
			    - sec_info->entry[i].new_offset);
		  address += (sec->output_section->vma + sec->output_offset
			      + sec_info->entry[i].offset + 8);
		  break;
		}
	      if (sec_info->entry[i].make_relative)
		value -= (sec->output_section->vma + sec->output_offset
			  + sec_info->entry[i].new_offset + 8);
	      write_value (abfd, buf, value, width);
	    }

	  if (hdr_info)
	    {
	      hdr_info->array[hdr_info->array_count].initial_loc = address;
	      hdr_info->array[hdr_info->array_count++].fde
		= (sec->output_section->vma + sec->output_offset
		   + sec_info->entry[i].new_offset);
	    }

	  if ((sec_info->entry[i].lsda_encoding & 0xf0) == DW_EH_PE_pcrel
	      || sec_info->entry[i].make_lsda_relative)
	    {
	      buf += sec_info->entry[i].lsda_offset;
	      width = get_DW_EH_PE_width (sec_info->entry[i].lsda_encoding,
					  ptr_size);
	      value = read_value (abfd, buf, width,
				  get_DW_EH_PE_signed
				  (sec_info->entry[i].lsda_encoding));
	      if (value)
		{
		  if ((sec_info->entry[i].lsda_encoding & 0xf0)
		      == DW_EH_PE_pcrel)
		    value += (sec_info->entry[i].offset
			      - sec_info->entry[i].new_offset);
		  else if (sec_info->entry[i].make_lsda_relative)
		    value -= (sec->output_section->vma + sec->output_offset
			      + sec_info->entry[i].new_offset + 8
			      + sec_info->entry[i].lsda_offset);
		  write_value (abfd, buf, value, width);
		}
	    }
	}
      else
	/* Terminating FDE must be at the end of .eh_frame section only.  */
	BFD_ASSERT (i == sec_info->count - 1);

      BFD_ASSERT (p == contents + sec_info->entry[i].new_offset);
      memmove (p, contents + sec_info->entry[i].offset,
	       sec_info->entry[i].size);
      p += sec_info->entry[i].size;
    }

  /* FIXME: Once _bfd_elf_discard_section_eh_frame will be able to
     shrink sections to zero size, this won't be needed any more.  */
  if (p == contents && sec->_cooked_size == 16)
    {
      bfd_put_32 (abfd, 12, p);		/* Fake CIE length */
      bfd_put_32 (abfd, 0, p + 4);	/* Fake CIE id */
      p[8] = 1;				/* Fake CIE version */
      memset (p + 9, 0, 7);		/* Fake CIE augmentation, 3xleb128
					   and 3xDW_CFA_nop as pad  */
      p += 16;
    }
  else
    {
      unsigned int alignment = 1 << sec->alignment_power;
      unsigned int pad = sec->_cooked_size % alignment;

      /* Don't pad beyond the raw size of the output section. It
	 can happen at the last input section.  */
      if (pad
	  && ((sec->output_offset + sec->_cooked_size + pad)
	      <= sec->output_section->_raw_size))
	{
	  /* Find the last CIE/FDE.  */
	  for (i = sec_info->count - 1; i > 0; i--)
	    if (! sec_info->entry[i].removed)
	      break;

	  /* The size of the last CIE/FDE must be at least 4.  */
	  if (sec_info->entry[i].removed
	      || sec_info->entry[i].size < 4)
	    abort ();

	  pad = alignment - pad;

	  buf = contents + sec_info->entry[i].new_offset;

	  /* Update length.  */
	  sec_info->entry[i].size += pad;
	  bfd_put_32 (abfd, sec_info->entry[i].size - 4, buf);

	  /* Pad it with DW_CFA_nop  */
	  memset (p, 0, pad);
	  p += pad;

	  sec->_cooked_size += pad;
	}
    }

  BFD_ASSERT ((bfd_size_type) (p - contents) == sec->_cooked_size);

  return bfd_set_section_contents (abfd, sec->output_section,
                                   contents, (file_ptr) sec->output_offset,
                                   sec->_cooked_size);
}

/* Helper function used to sort .eh_frame_hdr search table by increasing
   VMA of FDE initial location.  */

static int
vma_compare (const void *a, const void *b)
{
  const struct eh_frame_array_ent *p = a;
  const struct eh_frame_array_ent *q = b;
  if (p->initial_loc > q->initial_loc)
    return 1;
  if (p->initial_loc < q->initial_loc)
    return -1;
  return 0;
}

/* Write out .eh_frame_hdr section.  This must be called after
   _bfd_elf_write_section_eh_frame has been called on all input
   .eh_frame sections.
   .eh_frame_hdr format:
   ubyte version		(currently 1)
   ubyte eh_frame_ptr_enc  	(DW_EH_PE_* encoding of pointer to start of
				 .eh_frame section)
   ubyte fde_count_enc		(DW_EH_PE_* encoding of total FDE count
				 number (or DW_EH_PE_omit if there is no
				 binary search table computed))
   ubyte table_enc		(DW_EH_PE_* encoding of binary search table,
				 or DW_EH_PE_omit if not present.
				 DW_EH_PE_datarel is using address of
				 .eh_frame_hdr section start as base)
   [encoded] eh_frame_ptr	(pointer to start of .eh_frame section)
   optionally followed by:
   [encoded] fde_count		(total number of FDEs in .eh_frame section)
   fde_count x [encoded] initial_loc, fde
				(array of encoded pairs containing
				 FDE initial_location field and FDE address,
				 sorted by increasing initial_loc).  */

bfd_boolean
_bfd_elf_write_section_eh_frame_hdr (bfd *abfd, struct bfd_link_info *info)
{
  struct elf_link_hash_table *htab;
  struct eh_frame_hdr_info *hdr_info;
  asection *sec;
  bfd_byte *contents;
  asection *eh_frame_sec;
  bfd_size_type size;
  bfd_boolean retval;
  bfd_vma encoded_eh_frame;

  htab = elf_hash_table (info);
  hdr_info = &htab->eh_info;
  sec = hdr_info->hdr_sec;
  if (sec == NULL)
    return TRUE;

  size = EH_FRAME_HDR_SIZE;
  if (hdr_info->array && hdr_info->array_count == hdr_info->fde_count)
    size += 4 + hdr_info->fde_count * 8;
  contents = bfd_malloc (size);
  if (contents == NULL)
    return FALSE;

  eh_frame_sec = bfd_get_section_by_name (abfd, ".eh_frame");
  if (eh_frame_sec == NULL)
    {
      free (contents);
      return FALSE;
    }

  memset (contents, 0, EH_FRAME_HDR_SIZE);
  contents[0] = 1;				/* Version.  */
  contents[1] = get_elf_backend_data (abfd)->elf_backend_encode_eh_address
    (abfd, info, eh_frame_sec, 0, sec, 4,
     &encoded_eh_frame);			/* .eh_frame offset.  */

  if (hdr_info->array && hdr_info->array_count == hdr_info->fde_count)
    {
      contents[2] = DW_EH_PE_udata4;		/* FDE count encoding.  */
      contents[3] = DW_EH_PE_datarel | DW_EH_PE_sdata4; /* Search table enc.  */
    }
  else
    {
      contents[2] = DW_EH_PE_omit;
      contents[3] = DW_EH_PE_omit;
    }
  bfd_put_32 (abfd, encoded_eh_frame, contents + 4);

  if (contents[2] != DW_EH_PE_omit)
    {
      unsigned int i;

      bfd_put_32 (abfd, hdr_info->fde_count, contents + EH_FRAME_HDR_SIZE);
      qsort (hdr_info->array, hdr_info->fde_count, sizeof (*hdr_info->array),
	     vma_compare);
      for (i = 0; i < hdr_info->fde_count; i++)
	{
	  bfd_put_32 (abfd,
		      hdr_info->array[i].initial_loc
		      - sec->output_section->vma,
		      contents + EH_FRAME_HDR_SIZE + i * 8 + 4);
	  bfd_put_32 (abfd,
		      hdr_info->array[i].fde - sec->output_section->vma,
		      contents + EH_FRAME_HDR_SIZE + i * 8 + 8);
	}
    }

  retval = bfd_set_section_contents (abfd, sec->output_section,
				     contents, (file_ptr) sec->output_offset,
				     sec->_cooked_size);
  free (contents);
  return retval;
}

/* Decide whether we can use a PC-relative encoding within the given
   EH frame section.  This is the default implementation.  */

bfd_boolean
_bfd_elf_can_make_relative (bfd *input_bfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info *info ATTRIBUTE_UNUSED,
			    asection *eh_frame_section ATTRIBUTE_UNUSED)
{
  return TRUE;
}

/* Select an encoding for the given address.  Preference is given to
   PC-relative addressing modes.  */

bfd_byte
_bfd_elf_encode_eh_address (bfd *abfd ATTRIBUTE_UNUSED,
			    struct bfd_link_info *info ATTRIBUTE_UNUSED,
			    asection *osec, bfd_vma offset,
			    asection *loc_sec, bfd_vma loc_offset,
			    bfd_vma *encoded)
{
  *encoded = osec->vma + offset -
    (loc_sec->output_section->vma + loc_sec->output_offset + loc_offset);
  return DW_EH_PE_pcrel | DW_EH_PE_sdata4;
}
