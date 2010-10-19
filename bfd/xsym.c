/* xSYM symbol-file support for BFD.
   Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005
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

#include "xsym.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#define bfd_sym_close_and_cleanup                   _bfd_generic_close_and_cleanup
#define bfd_sym_bfd_free_cached_info                _bfd_generic_bfd_free_cached_info
#define bfd_sym_new_section_hook                    _bfd_generic_new_section_hook
#define bfd_sym_bfd_is_local_label_name             bfd_generic_is_local_label_name
#define bfd_sym_bfd_is_target_special_symbol       ((bfd_boolean (*) (bfd *, asymbol *)) bfd_false)
#define bfd_sym_get_lineno                          _bfd_nosymbols_get_lineno
#define bfd_sym_find_nearest_line                   _bfd_nosymbols_find_nearest_line
#define bfd_sym_find_inliner_info                   _bfd_nosymbols_find_inliner_info
#define bfd_sym_bfd_make_debug_symbol               _bfd_nosymbols_bfd_make_debug_symbol
#define bfd_sym_read_minisymbols                    _bfd_generic_read_minisymbols
#define bfd_sym_minisymbol_to_symbol                _bfd_generic_minisymbol_to_symbol
#define bfd_sym_get_reloc_upper_bound               _bfd_norelocs_get_reloc_upper_bound
#define bfd_sym_canonicalize_reloc                  _bfd_norelocs_canonicalize_reloc
#define bfd_sym_bfd_reloc_type_lookup               _bfd_norelocs_bfd_reloc_type_lookup
#define bfd_sym_set_arch_mach                       _bfd_generic_set_arch_mach
#define bfd_sym_get_section_contents                _bfd_generic_get_section_contents
#define bfd_sym_set_section_contents                _bfd_generic_set_section_contents
#define bfd_sym_bfd_get_relocated_section_contents  bfd_generic_get_relocated_section_contents
#define bfd_sym_bfd_relax_section                   bfd_generic_relax_section
#define bfd_sym_bfd_gc_sections                     bfd_generic_gc_sections
#define bfd_sym_bfd_merge_sections                  bfd_generic_merge_sections
#define bfd_sym_bfd_is_group_section                bfd_generic_is_group_section
#define bfd_sym_bfd_discard_group                   bfd_generic_discard_group
#define bfd_sym_section_already_linked              _bfd_generic_section_already_linked
#define bfd_sym_bfd_link_hash_table_create          _bfd_generic_link_hash_table_create
#define bfd_sym_bfd_link_hash_table_free            _bfd_generic_link_hash_table_free
#define bfd_sym_bfd_link_add_symbols                _bfd_generic_link_add_symbols
#define bfd_sym_bfd_link_just_syms                  _bfd_generic_link_just_syms
#define bfd_sym_bfd_final_link                      _bfd_generic_final_link
#define bfd_sym_bfd_link_split_section              _bfd_generic_link_split_section
#define bfd_sym_get_section_contents_in_window      _bfd_generic_get_section_contents_in_window

extern const bfd_target sym_vec;

static int
pstrcmp (const char *as, const char *bs)
{
  const unsigned char *a = (const unsigned char *) as;
  const unsigned char *b = (const unsigned char *) bs;
  unsigned char clen;
  int ret;

  clen = (a[0] > b[0]) ? b[0] : a[0];
  ret = memcmp (a + 1, b + 1, clen);
  if (ret != 0)
    return ret;

  if (a[0] == b[0])
    return 0;
  else if (a[0] < b[0])
    return -1;
  else
    return 1;
}

static unsigned long
compute_offset (unsigned long first_page,
		unsigned long page_size,
		unsigned long entry_size,
		unsigned long index)
{
  unsigned long entries_per_page = page_size / entry_size;
  unsigned long page_number = first_page + (index / entries_per_page);
  unsigned long page_offset = (index % entries_per_page) * entry_size;

  return (page_number * page_size) + page_offset;
}

bfd_boolean
bfd_sym_mkobject (bfd *abfd ATTRIBUTE_UNUSED)
{
  return 1;
}

void
bfd_sym_print_symbol (bfd *abfd ATTRIBUTE_UNUSED,
		      void * afile ATTRIBUTE_UNUSED,
		      asymbol *symbol ATTRIBUTE_UNUSED,
		      bfd_print_symbol_type how ATTRIBUTE_UNUSED)
{
  return;
}

bfd_boolean
bfd_sym_valid (bfd *abfd)
{
  if (abfd == NULL || abfd->xvec == NULL)
    return 0;

  return abfd->xvec == &sym_vec;
}

unsigned char *
bfd_sym_read_name_table (bfd *abfd, bfd_sym_header_block *dshb)
{
  unsigned char *rstr;
  long ret;
  size_t table_size = dshb->dshb_nte.dti_page_count * dshb->dshb_page_size;
  size_t table_offset = dshb->dshb_nte.dti_first_page * dshb->dshb_page_size;

  rstr = bfd_alloc (abfd, table_size);
  if (rstr == NULL)
    return rstr;

  bfd_seek (abfd, table_offset, SEEK_SET);
  ret = bfd_bread (rstr, table_size, abfd);
  if (ret < 0 || (unsigned long) ret != table_size)
    {
      bfd_release (abfd, rstr);
      return NULL;
    }

  return rstr;
}

void
bfd_sym_parse_file_reference_v32 (unsigned char *buf,
				  size_t len,
				  bfd_sym_file_reference *entry)
{
  BFD_ASSERT (len == 6);

  entry->fref_frte_index = bfd_getb16 (buf);
  entry->fref_offset = bfd_getb32 (buf + 2);
}

void
bfd_sym_parse_disk_table_v32 (unsigned char *buf,
			      size_t len,
			      bfd_sym_table_info *table)
{
  BFD_ASSERT (len == 8);

  table->dti_first_page = bfd_getb16 (buf);
  table->dti_page_count = bfd_getb16 (buf + 2);
  table->dti_object_count = bfd_getb32 (buf + 4);
}

void
bfd_sym_parse_header_v32 (unsigned char *buf,
			  size_t len,
			  bfd_sym_header_block *header)
{
  BFD_ASSERT (len == 154);

  memcpy (header->dshb_id, buf, 32);
  header->dshb_page_size = bfd_getb16 (buf + 32);
  header->dshb_hash_page = bfd_getb16 (buf + 34);
  header->dshb_root_mte = bfd_getb16 (buf + 36);
  header->dshb_mod_date = bfd_getb32 (buf + 38);

  bfd_sym_parse_disk_table_v32 (buf + 42, 8, &header->dshb_frte);
  bfd_sym_parse_disk_table_v32 (buf + 50, 8, &header->dshb_rte);
  bfd_sym_parse_disk_table_v32 (buf + 58, 8, &header->dshb_mte);
  bfd_sym_parse_disk_table_v32 (buf + 66, 8, &header->dshb_cmte);
  bfd_sym_parse_disk_table_v32 (buf + 74, 8, &header->dshb_cvte);
  bfd_sym_parse_disk_table_v32 (buf + 82, 8, &header->dshb_csnte);
  bfd_sym_parse_disk_table_v32 (buf + 90, 8, &header->dshb_clte);
  bfd_sym_parse_disk_table_v32 (buf + 98, 8, &header->dshb_ctte);
  bfd_sym_parse_disk_table_v32 (buf + 106, 8, &header->dshb_tte);
  bfd_sym_parse_disk_table_v32 (buf + 114, 8, &header->dshb_nte);
  bfd_sym_parse_disk_table_v32 (buf + 122, 8, &header->dshb_tinfo);
  bfd_sym_parse_disk_table_v32 (buf + 130, 8, &header->dshb_fite);
  bfd_sym_parse_disk_table_v32 (buf + 138, 8, &header->dshb_const);

  memcpy (&header->dshb_file_creator, buf + 146, 4);
  memcpy (&header->dshb_file_type, buf + 150, 4);
}

int
bfd_sym_read_header_v32 (bfd *abfd, bfd_sym_header_block *header)
{
  unsigned char buf[154];
  long ret;

  ret = bfd_bread (buf, 154, abfd);
  if (ret != 154)
    return -1;

  bfd_sym_parse_header_v32 (buf, 154, header);

  return 0;
}

int
bfd_sym_read_header_v34 (bfd *abfd ATTRIBUTE_UNUSED,
			 bfd_sym_header_block *header ATTRIBUTE_UNUSED)
{
  abort ();
}

int
bfd_sym_read_header (bfd *abfd,
		     bfd_sym_header_block *header,
		     bfd_sym_version version)
{
  switch (version)
    {
    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
      return bfd_sym_read_header_v34 (abfd, header);
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      return bfd_sym_read_header_v32 (abfd, header);
    case BFD_SYM_VERSION_3_1:
    default:
      return 0;
    }
}

int
bfd_sym_read_version (bfd *abfd, bfd_sym_version *version)
{
  char version_string[32];
  long ret;

  ret = bfd_bread (version_string, sizeof (version_string), abfd);
  if (ret != sizeof (version_string))
    return -1;

  if (pstrcmp (version_string, BFD_SYM_VERSION_STR_3_1) == 0)
    *version = BFD_SYM_VERSION_3_1;
  else if (pstrcmp (version_string, BFD_SYM_VERSION_STR_3_2) == 0)
    *version = BFD_SYM_VERSION_3_2;
  else if (pstrcmp (version_string, BFD_SYM_VERSION_STR_3_3) == 0)
    *version = BFD_SYM_VERSION_3_3;
  else if (pstrcmp (version_string, BFD_SYM_VERSION_STR_3_4) == 0)
    *version = BFD_SYM_VERSION_3_4;
  else if (pstrcmp (version_string, BFD_SYM_VERSION_STR_3_5) == 0)
    *version = BFD_SYM_VERSION_3_5;
  else
    return -1;

  return 0;
}

void
bfd_sym_display_table_summary (FILE *f,
			       bfd_sym_table_info *dti,
			       const char *name)
{
  fprintf (f, "%-6s %13ld %13ld %13ld\n",
	   name,
	   dti->dti_first_page,
	   dti->dti_page_count,
	   dti->dti_object_count);
}

void
bfd_sym_display_header (FILE *f, bfd_sym_header_block *dshb)
{
  fprintf (f, "            Version: %.*s\n", dshb->dshb_id[0], dshb->dshb_id + 1);
  fprintf (f, "          Page Size: 0x%x\n", dshb->dshb_page_size);
  fprintf (f, "          Hash Page: %lu\n", dshb->dshb_hash_page);
  fprintf (f, "           Root MTE: %lu\n", dshb->dshb_root_mte);
  fprintf (f, "  Modification Date: ");
  fprintf (f, "[unimplemented]");
  fprintf (f, " (0x%lx)\n", dshb->dshb_mod_date);

  fprintf (f, "       File Creator:  %.4s  Type: %.4s\n\n",
	   dshb->dshb_file_creator, dshb->dshb_file_type);

  fprintf (f, "Table Name   First Page    Page Count   Object Count\n");
  fprintf (f, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

  bfd_sym_display_table_summary (f, &dshb->dshb_nte, "NTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_rte, "RTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_mte, "MTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_frte, "FRTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_cmte, "CMTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_cvte, "CVTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_csnte, "CSNTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_clte, "CLTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_ctte, "CTTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_tte, "TTE");
  bfd_sym_display_table_summary (f, &dshb->dshb_tinfo, "TINFO");
  bfd_sym_display_table_summary (f, &dshb->dshb_fite, "FITE");
  bfd_sym_display_table_summary (f, &dshb->dshb_const, "CONST");

  fprintf (f, "\n");
}

void
bfd_sym_parse_resources_table_entry_v32 (unsigned char *buf,
					 size_t len,
					 bfd_sym_resources_table_entry *entry)
{
  BFD_ASSERT (len == 18);

  memcpy (&entry->rte_res_type, buf, 4);
  entry->rte_res_number = bfd_getb16 (buf + 4);
  entry->rte_nte_index = bfd_getb32 (buf + 6);
  entry->rte_mte_first = bfd_getb16 (buf + 10);
  entry->rte_mte_last = bfd_getb16 (buf + 12);
  entry->rte_res_size = bfd_getb32 (buf + 14);
}

void
bfd_sym_parse_modules_table_entry_v33 (unsigned char *buf,
				       size_t len,
				       bfd_sym_modules_table_entry *entry)
{
  BFD_ASSERT (len == 46);

  entry->mte_rte_index = bfd_getb16 (buf);
  entry->mte_res_offset = bfd_getb32 (buf + 2);
  entry->mte_size = bfd_getb32 (buf + 6);
  entry->mte_kind = buf[10];
  entry->mte_scope = buf[11];
  entry->mte_parent = bfd_getb16 (buf + 12);
  bfd_sym_parse_file_reference_v32 (buf + 14, 6, &entry->mte_imp_fref);
  entry->mte_imp_end = bfd_getb32 (buf + 20);
  entry->mte_nte_index = bfd_getb32 (buf + 24);
  entry->mte_cmte_index = bfd_getb16 (buf + 28);
  entry->mte_cvte_index = bfd_getb32 (buf + 30);
  entry->mte_clte_index = bfd_getb16 (buf + 34);
  entry->mte_ctte_index = bfd_getb16 (buf + 36);
  entry->mte_csnte_idx_1 = bfd_getb32 (buf + 38);
  entry->mte_csnte_idx_2 = bfd_getb32 (buf + 42);
}

void
bfd_sym_parse_file_references_table_entry_v32 (unsigned char *buf,
					       size_t len,
					       bfd_sym_file_references_table_entry *entry)
{
  unsigned int type;

  BFD_ASSERT (len == 10);

  memset (entry, 0, sizeof (bfd_sym_file_references_table_entry));
  type = bfd_getb16 (buf);

  switch (type)
    {
    case BFD_SYM_END_OF_LIST_3_2:
      entry->generic.type = BFD_SYM_END_OF_LIST;
      break;

    case BFD_SYM_FILE_NAME_INDEX_3_2:
      entry->filename.type = BFD_SYM_FILE_NAME_INDEX;
      entry->filename.nte_index = bfd_getb32 (buf + 2);
      entry->filename.mod_date = bfd_getb32 (buf + 6);
      break;

    default:
      entry->entry.mte_index = type;
      entry->entry.file_offset = bfd_getb32 (buf + 2);
    }
}

void
bfd_sym_parse_contained_modules_table_entry_v32 (unsigned char *buf,
						 size_t len,
						 bfd_sym_contained_modules_table_entry *entry)
{
  unsigned int type;

  BFD_ASSERT (len == 6);

  memset (entry, 0, sizeof (bfd_sym_contained_modules_table_entry));
  type = bfd_getb16 (buf);

  switch (type)
    {
    case BFD_SYM_END_OF_LIST_3_2:
      entry->generic.type = BFD_SYM_END_OF_LIST;
      break;

    default:
      entry->entry.mte_index = type;
      entry->entry.nte_index = bfd_getb32 (buf + 2);
      break;
    }
}

void
bfd_sym_parse_contained_variables_table_entry_v32 (unsigned char *buf,
						   size_t len,
						   bfd_sym_contained_variables_table_entry *entry)
{
  unsigned int type;

  BFD_ASSERT (len == 26);

  memset (entry, 0, sizeof (bfd_sym_contained_variables_table_entry));
  type = bfd_getb16 (buf);

  switch (type)
    {
    case BFD_SYM_END_OF_LIST_3_2:
      entry->generic.type = BFD_SYM_END_OF_LIST;
      break;

    case BFD_SYM_SOURCE_FILE_CHANGE_3_2:
      entry->file.type = BFD_SYM_SOURCE_FILE_CHANGE;
      bfd_sym_parse_file_reference_v32 (buf + 2, 6, &entry->file.fref);
      break;

    default:
      entry->entry.tte_index = type;
      entry->entry.nte_index = bfd_getb32 (buf + 2);
      entry->entry.file_delta = bfd_getb16 (buf + 6);
      entry->entry.scope = buf[8];
      entry->entry.la_size = buf[9];

      if (entry->entry.la_size == BFD_SYM_CVTE_SCA)
	{
	  entry->entry.address.scstruct.sca_kind = buf[10];
	  entry->entry.address.scstruct.sca_class = buf[11];
	  entry->entry.address.scstruct.sca_offset = bfd_getb32 (buf + 12);
	}
      else if (entry->entry.la_size <= BFD_SYM_CVTE_SCA)
	{
#if BFD_SYM_CVTE_SCA > 0
	  memcpy (&entry->entry.address.lastruct.la, buf + 10,
		  BFD_SYM_CVTE_SCA);
#endif
	  entry->entry.address.lastruct.la_kind = buf[23];
	}
      else if (entry->entry.la_size == BFD_SYM_CVTE_BIG_LA)
	{
	  entry->entry.address.biglastruct.big_la = bfd_getb32 (buf + 10);
	  entry->entry.address.biglastruct.big_la_kind = buf[12];
	}
    }
}

void
bfd_sym_parse_contained_statements_table_entry_v32 (unsigned char *buf,
						    size_t len,
						    bfd_sym_contained_statements_table_entry *entry)
{
  unsigned int type;

  BFD_ASSERT (len == 8);

  memset (entry, 0, sizeof (bfd_sym_contained_statements_table_entry));
  type = bfd_getb16 (buf);

  switch (type)
    {
    case BFD_SYM_END_OF_LIST_3_2:
      entry->generic.type = BFD_SYM_END_OF_LIST;
      break;

    case BFD_SYM_SOURCE_FILE_CHANGE_3_2:
      entry->file.type = BFD_SYM_SOURCE_FILE_CHANGE;
      bfd_sym_parse_file_reference_v32 (buf + 2, 6, &entry->file.fref);
      break;

    default:
      entry->entry.mte_index = type;
      entry->entry.mte_offset = bfd_getb16 (buf + 2);
      entry->entry.file_delta = bfd_getb32 (buf + 4);
      break;
    }
}

void
bfd_sym_parse_contained_labels_table_entry_v32 (unsigned char *buf,
						size_t len,
						bfd_sym_contained_labels_table_entry *entry)
{
  unsigned int type;

  BFD_ASSERT (len == 12);

  memset (entry, 0, sizeof (bfd_sym_contained_labels_table_entry));
  type = bfd_getb16 (buf);

  switch (type)
    {
    case BFD_SYM_END_OF_LIST_3_2:
      entry->generic.type = BFD_SYM_END_OF_LIST;
      break;

    case BFD_SYM_SOURCE_FILE_CHANGE_3_2:
      entry->file.type = BFD_SYM_SOURCE_FILE_CHANGE;
      bfd_sym_parse_file_reference_v32 (buf + 2, 6, &entry->file.fref);
      break;

    default:
      entry->entry.mte_index = type;
      entry->entry.mte_offset = bfd_getb16 (buf + 2);
      entry->entry.nte_index = bfd_getb32 (buf + 4);
      entry->entry.file_delta = bfd_getb16 (buf + 8);
      entry->entry.scope = bfd_getb16 (buf + 10);
      break;
    }
}

void
bfd_sym_parse_type_table_entry_v32 (unsigned char *buf,
				    size_t len,
				    bfd_sym_type_table_entry *entry)
{
  BFD_ASSERT (len == 4);

  *entry = bfd_getb32 (buf);
}

int
bfd_sym_fetch_resources_table_entry (bfd *abfd,
				     bfd_sym_resources_table_entry *entry,
				     unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_resources_table_entry *);
  unsigned long offset;
  unsigned long entry_size;
  unsigned char buf[18];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
      return -1;

    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 18;
      parser = bfd_sym_parse_resources_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_1:
    default:
      return -1;
    }
  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_rte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_modules_table_entry (bfd *abfd,
				   bfd_sym_modules_table_entry *entry,
				   unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_modules_table_entry *);
  unsigned long offset;
  unsigned long entry_size;
  unsigned char buf[46];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
      return -1;

    case BFD_SYM_VERSION_3_3:
      entry_size = 46;
      parser = bfd_sym_parse_modules_table_entry_v33;
      break;

    case BFD_SYM_VERSION_3_2:
    case BFD_SYM_VERSION_3_1:
    default:
      return -1;
    }
  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_mte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_file_references_table_entry (bfd *abfd,
					   bfd_sym_file_references_table_entry *entry,
					   unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_file_references_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[8];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 10;
      parser = bfd_sym_parse_file_references_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_frte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_contained_modules_table_entry (bfd *abfd,
					     bfd_sym_contained_modules_table_entry *entry,
					     unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_contained_modules_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[6];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 6;
      parser = bfd_sym_parse_contained_modules_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_cmte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_contained_variables_table_entry (bfd *abfd,
					       bfd_sym_contained_variables_table_entry *entry,
					       unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_contained_variables_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[26];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 26;
      parser = bfd_sym_parse_contained_variables_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_cvte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_contained_statements_table_entry (bfd *abfd,
						bfd_sym_contained_statements_table_entry *entry,
						unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_contained_statements_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[8];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 8;
      parser = bfd_sym_parse_contained_statements_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_csnte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_contained_labels_table_entry (bfd *abfd,
					    bfd_sym_contained_labels_table_entry *entry,
					    unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_contained_labels_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[12];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 12;
      parser = bfd_sym_parse_contained_labels_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_clte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_contained_types_table_entry (bfd *abfd,
					   bfd_sym_contained_types_table_entry *entry,
					   unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_contained_types_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[0];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 0;
      parser = NULL;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_ctte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_file_references_index_table_entry (bfd *abfd,
						 bfd_sym_file_references_index_table_entry *entry,
						 unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_file_references_index_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[0];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 0;
      parser = NULL;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_fite.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_constant_pool_entry (bfd *abfd,
				   bfd_sym_constant_pool_entry *entry,
				   unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_constant_pool_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[0];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return -1;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 0;
      parser = NULL;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_fite.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_type_table_entry (bfd *abfd,
				bfd_sym_type_table_entry *entry,
				unsigned long index)
{
  void (*parser) (unsigned char *, size_t, bfd_sym_type_table_entry *);
  unsigned long offset;
  unsigned long entry_size = 0;
  unsigned char buf[4];
  bfd_sym_data_struct *sdata = NULL;

  parser = NULL;
  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  switch (sdata->version)
    {
    case BFD_SYM_VERSION_3_3:
    case BFD_SYM_VERSION_3_2:
      entry_size = 4;
      parser = bfd_sym_parse_type_table_entry_v32;
      break;

    case BFD_SYM_VERSION_3_5:
    case BFD_SYM_VERSION_3_4:
    case BFD_SYM_VERSION_3_1:
    default:
      break;
    }

  if (parser == NULL)
    return -1;

  offset = compute_offset (sdata->header.dshb_tte.dti_first_page,
			   sdata->header.dshb_page_size,
			   entry_size, index);

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;
  if (bfd_bread (buf, entry_size, abfd) != entry_size)
    return -1;

  (*parser) (buf, entry_size, entry);

  return 0;
}

int
bfd_sym_fetch_type_information_table_entry (bfd *abfd,
					    bfd_sym_type_information_table_entry *entry,
					    unsigned long offset)
{
  unsigned char buf[4];
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (offset == 0)
    return -1;

  if (bfd_seek (abfd, offset, SEEK_SET) < 0)
    return -1;

  if (bfd_bread (buf, 4, abfd) != 4)
    return -1;
  entry->nte_index = bfd_getb32 (buf);

  if (bfd_bread (buf, 2, abfd) != 2)
    return -1;
  entry->physical_size = bfd_getb16 (buf);

  if (entry->physical_size & 0x8000)
    {
      if (bfd_bread (buf, 4, abfd) != 4)
	return -1;
      entry->physical_size &= 0x7fff;
      entry->logical_size = bfd_getb32 (buf);
      entry->offset = offset + 10;
    }
  else
    {
      if (bfd_bread (buf, 2, abfd) != 2)
	return -1;
      entry->physical_size &= 0x7fff;
      entry->logical_size = bfd_getb16 (buf);
      entry->offset = offset + 8;
    }

  return 0;
}

int
bfd_sym_fetch_type_table_information (bfd *abfd,
				      bfd_sym_type_information_table_entry *entry,
				      unsigned long index)
{
  bfd_sym_type_table_entry tindex;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (sdata->header.dshb_tte.dti_object_count <= 99)
    return -1;
  if (index < 100)
    return -1;

  if (bfd_sym_fetch_type_table_entry (abfd, &tindex, index - 100) < 0)
    return -1;
  if (bfd_sym_fetch_type_information_table_entry (abfd, entry, tindex) < 0)
    return -1;

  return 0;
}

const unsigned char *
bfd_sym_symbol_name (bfd *abfd, unsigned long index)
{
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (index == 0)
    return (const unsigned char *) "";

  index *= 2;
  if ((index / sdata->header.dshb_page_size)
      > sdata->header.dshb_nte.dti_page_count)
    return (const unsigned char *) "\09[INVALID]";

  return (const unsigned char *) sdata->name_table + index;
}

const unsigned char *
bfd_sym_module_name (bfd *abfd, unsigned long index)
{
  bfd_sym_modules_table_entry entry;

  if (bfd_sym_fetch_modules_table_entry (abfd, &entry, index) < 0)
    return (const unsigned char *) "\09[INVALID]";

  return bfd_sym_symbol_name (abfd, entry.mte_nte_index);
}

const char *
bfd_sym_unparse_storage_kind (enum bfd_sym_storage_kind kind)
{
  switch (kind)
    {
    case BFD_SYM_STORAGE_KIND_LOCAL: return "LOCAL";
    case BFD_SYM_STORAGE_KIND_VALUE: return "VALUE";
    case BFD_SYM_STORAGE_KIND_REFERENCE: return "REFERENCE";
    case BFD_SYM_STORAGE_KIND_WITH: return "WITH";
    default: return "[UNKNOWN]";
    }
}

const char *
bfd_sym_unparse_storage_class (enum bfd_sym_storage_class kind)
{
  switch (kind)
    {
    case BFD_SYM_STORAGE_CLASS_REGISTER: return "REGISTER";
    case BFD_SYM_STORAGE_CLASS_GLOBAL: return "GLOBAL";
    case BFD_SYM_STORAGE_CLASS_FRAME_RELATIVE: return "FRAME_RELATIVE";
    case BFD_SYM_STORAGE_CLASS_STACK_RELATIVE: return "STACK_RELATIVE";
    case BFD_SYM_STORAGE_CLASS_ABSOLUTE: return "ABSOLUTE";
    case BFD_SYM_STORAGE_CLASS_CONSTANT: return "CONSTANT";
    case BFD_SYM_STORAGE_CLASS_RESOURCE: return "RESOURCE";
    case BFD_SYM_STORAGE_CLASS_BIGCONSTANT: return "BIGCONSTANT";
    default: return "[UNKNOWN]";
    }
}

const char *
bfd_sym_unparse_module_kind (enum bfd_sym_module_kind kind)
{
  switch (kind)
    {
    case BFD_SYM_MODULE_KIND_NONE: return "NONE";
    case BFD_SYM_MODULE_KIND_PROGRAM: return "PROGRAM";
    case BFD_SYM_MODULE_KIND_UNIT: return "UNIT";
    case BFD_SYM_MODULE_KIND_PROCEDURE: return "PROCEDURE";
    case BFD_SYM_MODULE_KIND_FUNCTION: return "FUNCTION";
    case BFD_SYM_MODULE_KIND_DATA: return "DATA";
    case BFD_SYM_MODULE_KIND_BLOCK: return "BLOCK";
    default: return "[UNKNOWN]";
    }
}

const char *
bfd_sym_unparse_symbol_scope (enum bfd_sym_symbol_scope scope)
{
  switch (scope)
    {
    case BFD_SYM_SYMBOL_SCOPE_LOCAL: return "LOCAL";
    case BFD_SYM_SYMBOL_SCOPE_GLOBAL: return "GLOBAL";
    default:
      return "[UNKNOWN]";
    }
}

void
bfd_sym_print_file_reference (bfd *abfd,
			      FILE *f,
			      bfd_sym_file_reference *entry)
{
  bfd_sym_file_references_table_entry frtentry;
  int ret;

  ret = bfd_sym_fetch_file_references_table_entry (abfd, &frtentry,
						   entry->fref_frte_index);
  fprintf (f, "FILE ");

  if ((ret < 0) || (frtentry.generic.type != BFD_SYM_FILE_NAME_INDEX))
    fprintf (f, "[INVALID]");
  else
    fprintf (f, "\"%.*s\"",
	     bfd_sym_symbol_name (abfd, frtentry.filename.nte_index)[0],
	     &bfd_sym_symbol_name (abfd, frtentry.filename.nte_index)[1]);

  fprintf (f, " (FRTE %lu)", entry->fref_frte_index);
}

void
bfd_sym_print_resources_table_entry (bfd *abfd,
				     FILE *f,
				     bfd_sym_resources_table_entry *entry)
{
  fprintf (f, " \"%.*s\" (NTE %lu), type \"%.4s\", num %u, size %lu, MTE %lu -- %lu",
	   bfd_sym_symbol_name (abfd, entry->rte_nte_index)[0],
	   &bfd_sym_symbol_name (abfd, entry->rte_nte_index)[1],
	   entry->rte_nte_index, entry->rte_res_type, entry->rte_res_number,
	   entry->rte_res_size, entry->rte_mte_first, entry->rte_mte_last);
}

void
bfd_sym_print_modules_table_entry (bfd *abfd,
				   FILE *f,
				   bfd_sym_modules_table_entry *entry)
{
  fprintf (f, "\"%.*s\" (NTE %lu)",
	   bfd_sym_symbol_name (abfd, entry->mte_nte_index)[0],
	   &bfd_sym_symbol_name (abfd, entry->mte_nte_index)[1],
	   entry->mte_nte_index);

  fprintf (f, "\n            ");

  bfd_sym_print_file_reference (abfd, f, &entry->mte_imp_fref);
  fprintf (f, " range %lu -- %lu",
	   entry->mte_imp_fref.fref_offset, entry->mte_imp_end);

  fprintf (f, "\n            ");

  fprintf (f, "kind %s", bfd_sym_unparse_module_kind (entry->mte_kind));
  fprintf (f, ", scope %s", bfd_sym_unparse_symbol_scope (entry->mte_scope));

  fprintf (f, ", RTE %lu, offset %lu, size %lu",
	   entry->mte_rte_index, entry->mte_res_offset, entry->mte_size);

  fprintf (f, "\n            ");

  fprintf (f, "CMTE %lu, CVTE %lu, CLTE %lu, CTTE %lu, CSNTE1 %lu, CSNTE2 %lu",
	   entry->mte_cmte_index, entry->mte_cvte_index,
	   entry->mte_clte_index, entry->mte_ctte_index,
	   entry->mte_csnte_idx_1, entry->mte_csnte_idx_2);

  if (entry->mte_parent != 0)
    fprintf (f, ", parent %lu", entry->mte_parent);
  else
    fprintf (f, ", no parent");

  if (entry->mte_cmte_index != 0)
    fprintf (f, ", child %lu", entry->mte_cmte_index);
  else
    fprintf (f, ", no child");
}

void
bfd_sym_print_file_references_table_entry (bfd *abfd,
					   FILE *f,
					   bfd_sym_file_references_table_entry *entry)
{
  switch (entry->generic.type)
    {
    case BFD_SYM_FILE_NAME_INDEX:
      fprintf (f, "FILE \"%.*s\" (NTE %lu), modtime ",
	       bfd_sym_symbol_name (abfd, entry->filename.nte_index)[0],
	       &bfd_sym_symbol_name (abfd, entry->filename.nte_index)[1],
	       entry->filename.nte_index);

      fprintf (f, "[UNIMPLEMENTED]");
      /* printModDate (entry->filename.mod_date); */
      fprintf (f, " (0x%lx)", entry->filename.mod_date);
      break;

    case BFD_SYM_END_OF_LIST:
      fprintf (f, "END");
      break;

    default:
      fprintf (f, "\"%.*s\" (MTE %lu), offset %lu",
	       bfd_sym_module_name (abfd, entry->entry.mte_index)[0],
	       &bfd_sym_module_name (abfd, entry->entry.mte_index)[1],
	       entry->entry.mte_index,
	       entry->entry.file_offset);
      break;
    }
}

void
bfd_sym_print_contained_modules_table_entry (bfd *abfd,
					     FILE *f,
					     bfd_sym_contained_modules_table_entry *entry)
{
  switch (entry->generic.type)
    {
    case BFD_SYM_END_OF_LIST:
      fprintf (f, "END");
      break;

    default:
      fprintf (f, "\"%.*s\" (MTE %lu, NTE %lu)",
	       bfd_sym_module_name (abfd, entry->entry.mte_index)[0],
	       &bfd_sym_module_name (abfd, entry->entry.mte_index)[1],
	       entry->entry.mte_index,
	       entry->entry.nte_index);
      break;
    }
}

void
bfd_sym_print_contained_variables_table_entry (bfd *abfd,
					       FILE *f,
					       bfd_sym_contained_variables_table_entry *entry)
{
  if (entry->generic.type == BFD_SYM_END_OF_LIST)
    {
      fprintf (f, "END");
      return;
    }

  if (entry->generic.type == BFD_SYM_SOURCE_FILE_CHANGE)
    {
      bfd_sym_print_file_reference (abfd, f, &entry->file.fref);
      fprintf (f, " offset %lu", entry->file.fref.fref_offset);
      return;
    }

  fprintf (f, "\"%.*s\" (NTE %lu)",
	   bfd_sym_symbol_name (abfd, entry->entry.nte_index)[0],
	   &bfd_sym_symbol_name (abfd, entry->entry.nte_index)[1],
	   entry->entry.nte_index);

  fprintf (f, ", TTE %lu", entry->entry.tte_index);
  fprintf (f, ", offset %lu", entry->entry.file_delta);
  fprintf (f, ", scope %s", bfd_sym_unparse_symbol_scope (entry->entry.scope));

  if (entry->entry.la_size == BFD_SYM_CVTE_SCA)
    fprintf (f, ", latype %s, laclass %s, laoffset %lu",
	     bfd_sym_unparse_storage_kind (entry->entry.address.scstruct.sca_kind),
	     bfd_sym_unparse_storage_class (entry->entry.address.scstruct.sca_class),
	     entry->entry.address.scstruct.sca_offset);
  else if (entry->entry.la_size <= BFD_SYM_CVTE_LA_MAX_SIZE)
    {
      unsigned long i;

      fprintf (f, ", la [");
      for (i = 0; i < entry->entry.la_size; i++)
	fprintf (f, "0x%02x ", entry->entry.address.lastruct.la[i]);
      fprintf (f, "]");
    }
  else if (entry->entry.la_size == BFD_SYM_CVTE_BIG_LA)
    fprintf (f, ", bigla %lu, biglakind %u",
	     entry->entry.address.biglastruct.big_la,
	     entry->entry.address.biglastruct.big_la_kind);

  else
    fprintf (f, ", la [INVALID]");
}

void
bfd_sym_print_contained_statements_table_entry (bfd *abfd,
						FILE *f,
						bfd_sym_contained_statements_table_entry *entry)
{
  if (entry->generic.type == BFD_SYM_END_OF_LIST)
    {
      fprintf (f, "END");
      return;
    }

  if (entry->generic.type == BFD_SYM_SOURCE_FILE_CHANGE)
    {
      bfd_sym_print_file_reference (abfd, f, &entry->file.fref);
      fprintf (f, " offset %lu", entry->file.fref.fref_offset);
      return;
    }

  fprintf (f, "\"%.*s\" (MTE %lu), offset %lu, delta %lu",
	   bfd_sym_module_name (abfd, entry->entry.mte_index)[0],
	   &bfd_sym_module_name (abfd, entry->entry.mte_index)[1],
	   entry->entry.mte_index,
	   entry->entry.mte_offset,
	   entry->entry.file_delta);
}

void
bfd_sym_print_contained_labels_table_entry (bfd *abfd,
					    FILE *f,
					    bfd_sym_contained_labels_table_entry *entry)
{
  if (entry->generic.type == BFD_SYM_END_OF_LIST)
    {
      fprintf (f, "END");
      return;
    }

  if (entry->generic.type == BFD_SYM_SOURCE_FILE_CHANGE)
    {
      bfd_sym_print_file_reference (abfd, f, &entry->file.fref);
      fprintf (f, " offset %lu", entry->file.fref.fref_offset);
      return;
    }

  fprintf (f, "\"%.*s\" (MTE %lu), offset %lu, delta %lu, scope %s",
	   bfd_sym_module_name (abfd, entry->entry.mte_index)[0],
	   &bfd_sym_module_name (abfd, entry->entry.mte_index)[1],
	   entry->entry.mte_index,
	   entry->entry.mte_offset,
	   entry->entry.file_delta,
	   bfd_sym_unparse_symbol_scope (entry->entry.scope));
}

void
bfd_sym_print_contained_types_table_entry (bfd *abfd ATTRIBUTE_UNUSED,
					   FILE *f,
					   bfd_sym_contained_types_table_entry *entry ATTRIBUTE_UNUSED)
{
  fprintf (f, "[UNIMPLEMENTED]");
}

const char *
bfd_sym_type_operator_name (unsigned char num)
{
  switch (num)
    {
    case 1: return "TTE";
    case 2: return "PointerTo";
    case 3: return "ScalarOf";
    case 4: return "ConstantOf";
    case 5: return "EnumerationOf";
    case 6: return "VectorOf";
    case 7: return "RecordOf";
    case 8: return "UnionOf";
    case 9: return "SubRangeOf";
    case 10: return "SetOf";
    case 11: return "NamedTypeOf";
    case 12: return "ProcOf";
    case 13: return "ValueOf";
    case 14: return "ArrayOf";
    default: return "[UNKNOWN OPERATOR]";
    }
}

const char *
bfd_sym_type_basic_name (unsigned char num)
{
  switch (num)
    {
    case 0: return "void";
    case 1: return "pascal string";
    case 2: return "unsigned long";
    case 3: return "signed long";
    case 4: return "extended (10 bytes)";
    case 5: return "pascal boolean (1 byte)";
    case 6: return "unsigned byte";
    case 7: return "signed byte";
    case 8: return "character (1 byte)";
    case 9: return "wide character (2 bytes)";
    case 10: return "unsigned short";
    case 11: return "signed short";
    case 12: return "singled";
    case 13: return "double";
    case 14: return "extended (12 bytes)";
    case 15: return "computational (8 bytes)";
    case 16: return "c string";
    case 17: return "as-is string";
    default: return "[UNKNOWN BASIC TYPE]";
    }
}

int
bfd_sym_fetch_long (unsigned char *buf,
		    unsigned long len,
		    unsigned long offset,
		    unsigned long *offsetptr,
		    long *value)
{
  int ret;

  if (offset >= len)
    {
      *value = 0;
      offset += 0;
      ret = -1;
    }
  else if (! (buf[offset] & 0x80))
    {
      *value = buf[offset];
      offset += 1;
      ret = 0;
    }
  else if (buf[offset] == 0xc0)
    {
      if ((offset + 5) > len)
	{
	  *value = 0;
	  offset = len;
	  ret = -1;
	}
      else
	{
	  *value = bfd_getb32 (buf + offset + 1);
	  offset += 5;
	  ret = 0;
	}
    }
  else if ((buf[offset] & 0xc0) == 0xc0)
    {
      *value =  -(buf[offset] & 0x3f);
      offset += 1;
      ret = 0;
    }
  else if ((buf[offset] & 0xc0) == 0x80)
    {
      if ((offset + 2) > len)
	{
	  *value = 0;
	  offset = len;
	  ret = -1;
	}
      else
	{
	  *value = bfd_getb16 (buf + offset) & 0x3fff;
	  offset += 2;
	  ret = 0;
	}
    }
  else
    abort ();

  if (offsetptr != NULL)
    *offsetptr = offset;

  return ret;
}

void
bfd_sym_print_type_information (bfd *abfd,
				FILE *f,
				unsigned char *buf,
				unsigned long len,
				unsigned long offset,
				unsigned long *offsetptr)
{
  unsigned int type;

  if (offset >= len)
    {
      fprintf (f, "[NULL]");

      if (offsetptr != NULL)
	*offsetptr = offset;
      return;
  }

  type = buf[offset];
  offset++;

  if (! (type & 0x80))
    {
      fprintf (f, "[%s] (0x%x)", bfd_sym_type_basic_name (type & 0x7f), type);

      if (offsetptr != NULL)
	*offsetptr = offset;
      return;
    }

  if (type & 0x40)
    fprintf (f, "[packed ");
  else
    fprintf (f, "[");

  switch (type & 0x3f)
    {
    case 1:
      {
	long value;
	bfd_sym_type_information_table_entry tinfo;

	bfd_sym_fetch_long (buf, len, offset, &offset, &value);
	if (value <= 0)
	  fprintf (f, "[INVALID]");
	else
	  {
	    if (bfd_sym_fetch_type_table_information (abfd, &tinfo, value) < 0)
	      fprintf (f, "[INVALID]");
	    else
	      fprintf (f, "\"%.*s\"",
		       bfd_sym_symbol_name (abfd, tinfo.nte_index)[0],
		       &bfd_sym_symbol_name (abfd, tinfo.nte_index)[1]);
	  }
	fprintf (f, " (TTE %lu)", value);
	break;
      }

    case 2:
      fprintf (f, "pointer (0x%x) to ", type);
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      break;

    case 3:
      {
	long value;

	fprintf (f, "scalar (0x%x) of ", type);
	bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
	bfd_sym_fetch_long (buf, len, offset, &offset, &value);
	fprintf (f, " (%lu)", (unsigned long) value);
	break;
      }

    case 5:
      {
	long lower, upper, nelem;
	int i;

	fprintf (f, "enumeration (0x%x) of ", type);
	bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
	bfd_sym_fetch_long (buf, len, offset, &offset, &lower);
	bfd_sym_fetch_long (buf, len, offset, &offset, &upper);
	bfd_sym_fetch_long (buf, len, offset, &offset, &nelem);
	fprintf (f, " from %lu to %lu with %lu elements: ",
		 (unsigned long) lower, (unsigned long) upper,
		 (unsigned long) nelem);

	for (i = 0; i < nelem; i++)
	  {
	    fprintf (f, "\n                    ");
	    bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
	  }
	break;
      }

    case 6:
      fprintf (f, "vector (0x%x)", type);
      fprintf (f, "\n                index ");
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      fprintf (f, "\n                target ");
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      break;

    case 7:
    case 8:
      {
	long nrec, eloff, i;

	if ((type & 0x3f) == 7)
	  fprintf (f, "record (0x%x) of ", type);
	else
	  fprintf (f, "union (0x%x) of ", type);

	bfd_sym_fetch_long (buf, len, offset, &offset, &nrec);
	fprintf (f, "%lu elements: ", nrec);

	for (i = 0; i < nrec; i++)
	  {
	    bfd_sym_fetch_long (buf, len, offset, &offset, &eloff);
	    fprintf (f, "\n                ");
	    fprintf (f, "offset %lu: ", eloff);
	    bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
	  }
	break;
      }

    case 9:
      fprintf (f, "subrange (0x%x) of ", type);
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      fprintf (f, " lower ");
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      fprintf (f, " upper ");
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      break;

  case 11:
    {
      long value;

      fprintf (f, "named type (0x%x) ", type);
      bfd_sym_fetch_long (buf, len, offset, &offset, &value);
      if (value <= 0)
	fprintf (f, "[INVALID]");
      else
	fprintf (f, "\"%.*s\"",
		 bfd_sym_symbol_name (abfd, value)[0],
		 &bfd_sym_symbol_name (abfd, value)[1]);

      fprintf (f, " (NTE %lu) with type ", value);
      bfd_sym_print_type_information (abfd, f, buf, len, offset, &offset);
      break;
    }

  default:
    fprintf (f, "%s (0x%x)", bfd_sym_type_operator_name (type), type);
    break;
    }

  if (type == (0x40 | 0x6))
    {
      /* Vector.  */
      long n, width, m;
      long l;
      long i;

      bfd_sym_fetch_long (buf, len, offset, &offset, &n);
      bfd_sym_fetch_long (buf, len, offset, &offset, &width);
      bfd_sym_fetch_long (buf, len, offset, &offset, &m);
      /* fprintf (f, "\n                "); */
      fprintf (f, " N %ld, width %ld, M %ld, ", n, width, m);
      for (i = 0; i < m; i++)
	{
	  bfd_sym_fetch_long (buf, len, offset, &offset, &l);
	  if (i != 0)
	    fprintf (f, " ");
	  fprintf (f, "%ld", l);
	}
    }
  else  if (type & 0x40)
    {
      /* Other packed type.  */
      long msb, lsb;

      bfd_sym_fetch_long (buf, len, offset, &offset, &msb);
      bfd_sym_fetch_long (buf, len, offset, &offset, &lsb);
      /* fprintf (f, "\n                "); */
      fprintf (f, " msb %ld, lsb %ld", msb, lsb);
    }

  fprintf (f, "]");

  if (offsetptr != NULL)
    *offsetptr = offset;
}

void
bfd_sym_print_type_information_table_entry (bfd *abfd,
					    FILE *f,
					    bfd_sym_type_information_table_entry *entry)
{
  unsigned char *buf;
  unsigned long offset;
  unsigned int i;

  fprintf (f, "\"%.*s\" (NTE %lu), %lu bytes at %lu, logical size %lu",
	   bfd_sym_symbol_name (abfd, entry->nte_index)[0],
	   &bfd_sym_symbol_name (abfd, entry->nte_index)[1],
	   entry->nte_index,
	   entry->physical_size, entry->offset, entry->logical_size);

  fprintf (f, "\n            ");

  buf = alloca (entry->physical_size);
  if (buf == NULL)
    {
      fprintf (f, "[ERROR]\n");
      return;
    }
  if (bfd_seek (abfd, entry->offset, SEEK_SET) < 0)
    {
      fprintf (f, "[ERROR]\n");
      return;
    }
  if (bfd_bread (buf, entry->physical_size, abfd) != entry->physical_size)
    {
      fprintf (f, "[ERROR]\n");
      return;
    }

  fprintf (f, "[");
  for (i = 0; i < entry->physical_size; i++)
    {
      if (i == 0)
	fprintf (f, "0x%02x", buf[i]);
      else
	fprintf (f, " 0x%02x", buf[i]);
    }

  fprintf (f, "]");
  fprintf (f, "\n            ");

  bfd_sym_print_type_information (abfd, f, buf, entry->physical_size, 0, &offset);

  if (offset != entry->physical_size)
    fprintf (f, "\n            [parser used %lu bytes instead of %lu]", offset, entry->physical_size);
}

void
bfd_sym_print_file_references_index_table_entry (bfd *abfd ATTRIBUTE_UNUSED,
						 FILE *f,
						 bfd_sym_file_references_index_table_entry *entry ATTRIBUTE_UNUSED)
{
  fprintf (f, "[UNIMPLEMENTED]");
}

void
bfd_sym_print_constant_pool_entry (bfd *abfd ATTRIBUTE_UNUSED,
				   FILE *f,
				   bfd_sym_constant_pool_entry *entry ATTRIBUTE_UNUSED)
{
  fprintf (f, "[UNIMPLEMENTED]");
}

unsigned char *
bfd_sym_display_name_table_entry (bfd *abfd,
				  FILE *f,
				  unsigned char *entry)
{
  unsigned long index;
  unsigned long offset;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;
  index = (entry - sdata->name_table) / 2;

  if (sdata->version >= BFD_SYM_VERSION_3_4 && entry[0] == 255 && entry[1] == 0)
    {
      unsigned short length = bfd_getb16 (entry + 2);
      fprintf (f, "[%8lu] \"%.*s\"\n", index, length, entry + 4);
      offset = 2 + length + 1;
    }
  else
    {
      if (! (entry[0] == 0 || (entry[0] == 1 && entry[1] == '\0')))
	fprintf (f, "[%8lu] \"%.*s\"\n", index, entry[0], entry + 1);

      if (sdata->version >= BFD_SYM_VERSION_3_4)
	offset = entry[0] + 2;
      else
	offset = entry[0] + 1;
    }

  return (entry + offset + (offset % 2));
}

void
bfd_sym_display_name_table (bfd *abfd, FILE *f)
{
  unsigned long name_table_len;
  unsigned char *name_table, *name_table_end, *cur;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  name_table_len = sdata->header.dshb_nte.dti_page_count * sdata->header.dshb_page_size;
  name_table = sdata->name_table;
  name_table_end = name_table + name_table_len;

  fprintf (f, "name table (NTE) contains %lu bytes:\n\n", name_table_len);

  cur = name_table;
  for (;;)
    {
      cur = bfd_sym_display_name_table_entry (abfd, f, cur);
      if (cur >= name_table_end)
	break;
    }
}

void
bfd_sym_display_resources_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_resources_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "resource table (RTE) contains %lu objects:\n\n",
	   sdata->header.dshb_rte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_rte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_resources_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_resources_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_modules_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_modules_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "module table (MTE) contains %lu objects:\n\n",
	   sdata->header.dshb_mte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_mte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_modules_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_modules_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_file_references_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_file_references_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "file reference table (FRTE) contains %lu objects:\n\n",
	   sdata->header.dshb_frte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_frte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_file_references_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_file_references_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_contained_modules_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_contained_modules_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "contained modules table (CMTE) contains %lu objects:\n\n",
	   sdata->header.dshb_cmte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_cmte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_contained_modules_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_contained_modules_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_contained_variables_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_contained_variables_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "contained variables table (CVTE) contains %lu objects:\n\n",
	   sdata->header.dshb_cvte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_cvte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_contained_variables_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_contained_variables_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }

  fprintf (f, "\n");
}

void
bfd_sym_display_contained_statements_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_contained_statements_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "contained statements table (CSNTE) contains %lu objects:\n\n",
	   sdata->header.dshb_csnte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_csnte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_contained_statements_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_contained_statements_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_contained_labels_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_contained_labels_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "contained labels table (CLTE) contains %lu objects:\n\n",
	   sdata->header.dshb_clte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_clte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_contained_labels_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_contained_labels_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_contained_types_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_contained_types_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "contained types table (CTTE) contains %lu objects:\n\n",
	   sdata->header.dshb_ctte.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_ctte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_contained_types_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_contained_types_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_file_references_index_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_file_references_index_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "file references index table (FITE) contains %lu objects:\n\n",
	   sdata->header.dshb_fite.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_fite.dti_object_count; i++)
    {
      if (bfd_sym_fetch_file_references_index_table_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_file_references_index_table_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_constant_pool (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_constant_pool_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  fprintf (f, "constant pool (CONST) contains %lu objects:\n\n",
	   sdata->header.dshb_const.dti_object_count);

  for (i = 1; i <= sdata->header.dshb_const.dti_object_count; i++)
    {
      if (bfd_sym_fetch_constant_pool_entry (abfd, &entry, i) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] ", i);
	  bfd_sym_print_constant_pool_entry (abfd, f, &entry);
	  fprintf (f, "\n");
	}
    }
}

void
bfd_sym_display_type_information_table (bfd *abfd, FILE *f)
{
  unsigned long i;
  bfd_sym_type_table_entry index;
  bfd_sym_type_information_table_entry entry;
  bfd_sym_data_struct *sdata = NULL;

  BFD_ASSERT (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  if (sdata->header.dshb_tte.dti_object_count > 99)
    fprintf (f, "type table (TINFO) contains %lu objects:\n\n",
	     sdata->header.dshb_tte.dti_object_count - 99);
  else
    {
      fprintf (f, "type table (TINFO) contains [INVALID] objects:\n\n");
      return;
    }

  for (i = 100; i <= sdata->header.dshb_tte.dti_object_count; i++)
    {
      if (bfd_sym_fetch_type_table_entry (abfd, &index, i - 100) < 0)
	fprintf (f, " [%8lu] [INVALID]\n", i);
      else
	{
	  fprintf (f, " [%8lu] (TINFO %lu) ", i, index);

	  if (bfd_sym_fetch_type_information_table_entry (abfd, &entry, index) < 0)
	    fprintf (f, "[INVALID]");
	  else
	    bfd_sym_print_type_information_table_entry (abfd, f, &entry);

	  fprintf (f, "\n");
	}
    }
}

int
bfd_sym_scan (bfd *abfd, bfd_sym_version version, bfd_sym_data_struct *mdata)
{
  asection *bfdsec;
  const char *name = "symbols";

  mdata->name_table = 0;
  mdata->sbfd = abfd;
  mdata->version = version;

  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_sym_read_header (abfd, &mdata->header, mdata->version) != 0)
    return -1;

  mdata->name_table = bfd_sym_read_name_table (abfd, &mdata->header);
  if (mdata->name_table == NULL)
    return -1;

  bfdsec = bfd_make_section_anyway (abfd, name);
  if (bfdsec == NULL)
    return -1;

  bfdsec->vma = 0;
  bfdsec->lma = 0;
  bfdsec->size = 0;
  bfdsec->filepos = 0;
  bfdsec->alignment_power = 0;

  bfdsec->flags = SEC_HAS_CONTENTS;

  abfd->tdata.sym_data = mdata;

  return 0;
}

const bfd_target *
bfd_sym_object_p (bfd *abfd)
{
  struct bfd_preserve preserve;
  bfd_sym_version version = -1;

  preserve.marker = NULL;
  bfd_seek (abfd, 0, SEEK_SET);
  if (bfd_sym_read_version (abfd, &version) != 0)
    goto wrong;

  preserve.marker = bfd_alloc (abfd, sizeof (bfd_sym_data_struct));
  if (preserve.marker == NULL
      || ! bfd_preserve_save (abfd, &preserve))
    goto fail;

  if (bfd_sym_scan (abfd, version,
		    (bfd_sym_data_struct *) preserve.marker) != 0)
    goto wrong;

  bfd_preserve_finish (abfd, &preserve);
  return abfd->xvec;

 wrong:
  bfd_set_error (bfd_error_wrong_format);

 fail:
  if (preserve.marker != NULL)
    bfd_preserve_restore (abfd, &preserve);
  return NULL;
}

asymbol *
bfd_sym_make_empty_symbol (bfd *abfd)
{
  return bfd_alloc (abfd, sizeof (asymbol));
}

void
bfd_sym_get_symbol_info (bfd *abfd ATTRIBUTE_UNUSED, asymbol *symbol, symbol_info *ret)
{
  bfd_symbol_info (symbol, ret);
}

long
bfd_sym_get_symtab_upper_bound (bfd *abfd ATTRIBUTE_UNUSED)
{
  return 0;
}

long
bfd_sym_canonicalize_symtab (bfd *abfd ATTRIBUTE_UNUSED, asymbol **sym ATTRIBUTE_UNUSED)
{
  return 0;
}

int
bfd_sym_sizeof_headers (bfd *abfd ATTRIBUTE_UNUSED, bfd_boolean exec ATTRIBUTE_UNUSED)
{
  return 0;
}

const bfd_target sym_vec =
{
  "sym",			/* Name.  */
  bfd_target_sym_flavour,	/* Flavour.  */
  BFD_ENDIAN_BIG,		/* Byteorder.  */
  BFD_ENDIAN_BIG,		/* Header byteorder.  */
  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
  (SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_ROM | SEC_HAS_CONTENTS), /* Section_flags.  */
  0,				/* Symbol_leading_char.  */
  ' ',				/* AR_pad_char.  */
  16,				/* AR_max_namelen.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Data.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Hdrs.  */
  {				/* bfd_check_format.  */
    _bfd_dummy_target,
    bfd_sym_object_p,		/* bfd_check_format.  */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {				/* bfd_set_format.  */
    bfd_false,
    bfd_sym_mkobject,
    bfd_false,
    bfd_false,
  },
  {				/* bfd_write_contents.  */
    bfd_false,
    bfd_true,
    bfd_false,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (bfd_sym),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (bfd_sym),
  BFD_JUMP_TABLE_RELOCS (bfd_sym),
  BFD_JUMP_TABLE_WRITE (bfd_sym),
  BFD_JUMP_TABLE_LINK (bfd_sym),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};
