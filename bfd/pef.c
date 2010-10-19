/* PEF support for BFD.
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

#include "safe-ctype.h"
#include "pef.h"
#include "pef-traceback.h"
#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"

#ifndef BFD_IO_FUNCS
#define BFD_IO_FUNCS 0
#endif

#define bfd_pef_close_and_cleanup                   _bfd_generic_close_and_cleanup
#define bfd_pef_bfd_free_cached_info                _bfd_generic_bfd_free_cached_info
#define bfd_pef_new_section_hook                    _bfd_generic_new_section_hook
#define bfd_pef_bfd_is_local_label_name             bfd_generic_is_local_label_name
#define bfd_pef_bfd_is_target_special_symbol ((bfd_boolean (*) (bfd *, asymbol *)) bfd_false)
#define bfd_pef_get_lineno                          _bfd_nosymbols_get_lineno
#define bfd_pef_find_nearest_line                   _bfd_nosymbols_find_nearest_line
#define bfd_pef_find_inliner_info                   _bfd_nosymbols_find_inliner_info
#define bfd_pef_bfd_make_debug_symbol               _bfd_nosymbols_bfd_make_debug_symbol
#define bfd_pef_read_minisymbols                    _bfd_generic_read_minisymbols
#define bfd_pef_minisymbol_to_symbol                _bfd_generic_minisymbol_to_symbol
#define bfd_pef_get_reloc_upper_bound               _bfd_norelocs_get_reloc_upper_bound
#define bfd_pef_canonicalize_reloc                  _bfd_norelocs_canonicalize_reloc
#define bfd_pef_bfd_reloc_type_lookup               _bfd_norelocs_bfd_reloc_type_lookup
#define bfd_pef_set_arch_mach                       _bfd_generic_set_arch_mach
#define bfd_pef_get_section_contents                _bfd_generic_get_section_contents
#define bfd_pef_set_section_contents                _bfd_generic_set_section_contents
#define bfd_pef_bfd_get_relocated_section_contents  bfd_generic_get_relocated_section_contents
#define bfd_pef_bfd_relax_section                   bfd_generic_relax_section
#define bfd_pef_bfd_gc_sections                     bfd_generic_gc_sections
#define bfd_pef_bfd_merge_sections                  bfd_generic_merge_sections
#define bfd_pef_bfd_is_group_section		    bfd_generic_is_group_section
#define bfd_pef_bfd_discard_group                   bfd_generic_discard_group
#define bfd_pef_section_already_linked	            _bfd_generic_section_already_linked
#define bfd_pef_bfd_link_hash_table_create          _bfd_generic_link_hash_table_create
#define bfd_pef_bfd_link_hash_table_free            _bfd_generic_link_hash_table_free
#define bfd_pef_bfd_link_add_symbols                _bfd_generic_link_add_symbols
#define bfd_pef_bfd_link_just_syms                  _bfd_generic_link_just_syms
#define bfd_pef_bfd_final_link                      _bfd_generic_final_link
#define bfd_pef_bfd_link_split_section              _bfd_generic_link_split_section
#define bfd_pef_get_section_contents_in_window      _bfd_generic_get_section_contents_in_window

static int
bfd_pef_parse_traceback_table (bfd *abfd,
			       asection *section,
			       unsigned char *buf,
			       size_t len,
			       size_t pos,
			       asymbol *sym,
			       FILE *file)
{
  struct traceback_table table;
  size_t offset;
  const char *s;
  asymbol tmpsymbol;

  if (sym == NULL)
    sym = & tmpsymbol;

  sym->name = NULL;
  sym->value = 0;
  sym->the_bfd = abfd;
  sym->section = section;
  sym->flags = 0;
  sym->udata.i = 0;

  /* memcpy is fine since all fields are unsigned char.  */
  if ((pos + 8) > len)
    return -1;
  memcpy (&table, buf + pos, 8);

  /* Calling code relies on returned symbols having a name and
     correct offset.  */
  if ((table.lang != TB_C) && (table.lang != TB_CPLUSPLUS))
    return -1;

  if (! (table.flags2 & TB_NAME_PRESENT))
    return -1;

  if (! table.flags1 & TB_HAS_TBOFF)
    return -1;

  offset = 8;

  if ((table.flags5 & TB_FLOATPARAMS) || (table.fixedparams))
    offset += 4;

  if (table.flags1 & TB_HAS_TBOFF)
    {
      struct traceback_table_tboff off;

      if ((pos + offset + 4) > len)
	return -1;
      off.tb_offset = bfd_getb32 (buf + pos + offset);
      offset += 4;

      /* Need to subtract 4 because the offset includes the 0x0L
	 preceding the table.  */
      if (file != NULL)
	fprintf (file, " [offset = 0x%lx]", off.tb_offset);

      if ((file == NULL) && ((off.tb_offset + 4) > (pos + offset)))
	return -1;

      sym->value = pos - off.tb_offset - 4;
    }

  if (table.flags2 & TB_INT_HNDL)
    offset += 4;

  if (table.flags1 & TB_HAS_CTL)
    {
      struct traceback_table_anchors anchors;

      if ((pos + offset + 4) > len)
	return -1;
      anchors.ctl_info = bfd_getb32 (buf + pos + offset);
      offset += 4;

      if (anchors.ctl_info > 1024)
	return -1;

      offset += anchors.ctl_info * 4;
    }

  if (table.flags2 & TB_NAME_PRESENT)
    {
      struct traceback_table_routine name;
      char *namebuf;

      if ((pos + offset + 2) > len)
	return -1;
      name.name_len = bfd_getb16 (buf + pos + offset);
      offset += 2;

      if (name.name_len > 4096)
	return -1;

      if ((pos + offset + name.name_len) > len)
	return -1;

      namebuf = bfd_alloc (abfd, name.name_len + 1);
      if (namebuf == NULL)
	return -1;

      memcpy (namebuf, buf + pos + offset, name.name_len);
      namebuf[name.name_len] = '\0';

      /* Strip leading period inserted by compiler.  */
      if (namebuf[0] == '.')
	memmove (namebuf, namebuf + 1, name.name_len + 1);

      sym->name = namebuf;

      for (s = sym->name; (*s != '\0'); s++)
	if (! ISPRINT (*s))
	  return -1;

      offset += name.name_len;
    }

  if (table.flags2 & TB_USES_ALLOCA)
    offset += 4;

  if (table.flags4 & TB_HAS_VEC_INFO)
    offset += 4;

  if (file != NULL)
    fprintf (file, " [length = 0x%lx]", (long) offset);

  return offset;
}

static void
bfd_pef_print_symbol (bfd *abfd,
		      void * afile,
		      asymbol *symbol,
		      bfd_print_symbol_type how)
{
  FILE *file = (FILE *) afile;

  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    default:
      bfd_print_symbol_vandf (abfd, (void *) file, symbol);
      fprintf (file, " %-5s %s", symbol->section->name, symbol->name);
      if (strncmp (symbol->name, "__traceback_", strlen ("__traceback_")) == 0)
	{
	  unsigned char *buf = alloca (symbol->udata.i);
	  size_t offset = symbol->value + 4;
	  size_t len = symbol->udata.i;
	  int ret;

	  bfd_get_section_contents (abfd, symbol->section, buf, offset, len);
	  ret = bfd_pef_parse_traceback_table (abfd, symbol->section, buf,
					       len, 0, NULL, file);
	  if (ret < 0)
	    fprintf (file, " [ERROR]");
	}
    }
}

static void
bfd_pef_convert_architecture (unsigned long architecture,
			      enum bfd_architecture *type,
			      unsigned long *subtype)
{
  const unsigned long ARCH_POWERPC = 0x70777063; /* 'pwpc'.  */
  const unsigned long ARCH_M68K = 0x6d36386b; /* 'm68k'.  */

  *subtype = bfd_arch_unknown;
  *type = bfd_arch_unknown;

  if (architecture == ARCH_POWERPC)
    *type = bfd_arch_powerpc;
  else if (architecture == ARCH_M68K)
    *type = bfd_arch_m68k;
}

static bfd_boolean
bfd_pef_mkobject (bfd *abfd ATTRIBUTE_UNUSED)
{
  return TRUE;
}

static const char *bfd_pef_section_name (bfd_pef_section *section)
{
  switch (section->section_kind)
    {
    case BFD_PEF_SECTION_CODE: return "code";
    case BFD_PEF_SECTION_UNPACKED_DATA: return "unpacked-data";
    case BFD_PEF_SECTION_PACKED_DATA: return "packed-data";
    case BFD_PEF_SECTION_CONSTANT: return "constant";
    case BFD_PEF_SECTION_LOADER: return "loader";
    case BFD_PEF_SECTION_DEBUG: return "debug";
    case BFD_PEF_SECTION_EXEC_DATA: return "exec-data";
    case BFD_PEF_SECTION_EXCEPTION: return "exception";
    case BFD_PEF_SECTION_TRACEBACK: return "traceback";
    default: return "unknown";
    }
}

static unsigned long bfd_pef_section_flags (bfd_pef_section *section)
{
  switch (section->section_kind)
    {
    case BFD_PEF_SECTION_CODE:
      return SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC | SEC_CODE;
    case BFD_PEF_SECTION_UNPACKED_DATA:
    case BFD_PEF_SECTION_PACKED_DATA:
    case BFD_PEF_SECTION_CONSTANT:
    case BFD_PEF_SECTION_LOADER:
    case BFD_PEF_SECTION_DEBUG:
    case BFD_PEF_SECTION_EXEC_DATA:
    case BFD_PEF_SECTION_EXCEPTION:
    case BFD_PEF_SECTION_TRACEBACK:
    default:
      return SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC;
    }
}

static asection *
bfd_pef_make_bfd_section (bfd *abfd, bfd_pef_section *section)
{
  asection *bfdsec;
  const char *name = bfd_pef_section_name (section);

  bfdsec = bfd_make_section_anyway (abfd, name);
  if (bfdsec == NULL)
    return NULL;

  bfdsec->vma = section->default_address + section->container_offset;
  bfdsec->lma = section->default_address + section->container_offset;
  bfdsec->size = section->container_length;
  bfdsec->filepos = section->container_offset;
  bfdsec->alignment_power = section->alignment;

  bfdsec->flags = bfd_pef_section_flags (section);

  return bfdsec;
}

int
bfd_pef_parse_loader_header (bfd *abfd ATTRIBUTE_UNUSED,
			     unsigned char *buf,
			     size_t len,
			     bfd_pef_loader_header *header)
{
  BFD_ASSERT (len == 56);

  header->main_section = bfd_getb32 (buf);
  header->main_offset = bfd_getb32 (buf + 4);
  header->init_section = bfd_getb32 (buf + 8);
  header->init_offset = bfd_getb32 (buf + 12);
  header->term_section = bfd_getb32 (buf + 16);
  header->term_offset = bfd_getb32 (buf + 20);
  header->imported_library_count = bfd_getb32 (buf + 24);
  header->total_imported_symbol_count = bfd_getb32 (buf + 28);
  header->reloc_section_count = bfd_getb32 (buf + 32);
  header->reloc_instr_offset = bfd_getb32 (buf + 36);
  header->loader_strings_offset = bfd_getb32 (buf + 40);
  header->export_hash_offset = bfd_getb32 (buf + 44);
  header->export_hash_table_power = bfd_getb32 (buf + 48);
  header->exported_symbol_count = bfd_getb32 (buf + 52);

  return 0;
}

int
bfd_pef_parse_imported_library (bfd *abfd ATTRIBUTE_UNUSED,
				unsigned char *buf,
				size_t len,
				bfd_pef_imported_library *header)
{
  BFD_ASSERT (len == 24);

  header->name_offset = bfd_getb32 (buf);
  header->old_implementation_version = bfd_getb32 (buf + 4);
  header->current_version = bfd_getb32 (buf + 8);
  header->imported_symbol_count = bfd_getb32 (buf + 12);
  header->first_imported_symbol = bfd_getb32 (buf + 16);
  header->options = buf[20];
  header->reserved_a = buf[21];
  header->reserved_b = bfd_getb16 (buf + 22);

  return 0;
}

int
bfd_pef_parse_imported_symbol (bfd *abfd ATTRIBUTE_UNUSED,
			       unsigned char *buf,
			       size_t len,
			       bfd_pef_imported_symbol *symbol)
{
  unsigned long value;

  BFD_ASSERT (len == 4);

  value = bfd_getb32 (buf);
  symbol->class = value >> 24;
  symbol->name = value & 0x00ffffff;

  return 0;
}

int
bfd_pef_scan_section (bfd *abfd, bfd_pef_section *section)
{
  unsigned char buf[28];

  bfd_seek (abfd, section->header_offset, SEEK_SET);
  if (bfd_bread ((void *) buf, 28, abfd) != 28)
    return -1;

  section->name_offset = bfd_h_get_32 (abfd, buf);
  section->default_address = bfd_h_get_32 (abfd, buf + 4);
  section->total_length = bfd_h_get_32 (abfd, buf + 8);
  section->unpacked_length = bfd_h_get_32 (abfd, buf + 12);
  section->container_length = bfd_h_get_32 (abfd, buf + 16);
  section->container_offset = bfd_h_get_32 (abfd, buf + 20);
  section->section_kind = buf[24];
  section->share_kind = buf[25];
  section->alignment = buf[26];
  section->reserved = buf[27];

  section->bfd_section = bfd_pef_make_bfd_section (abfd, section);
  if (section->bfd_section == NULL)
    return -1;

  return 0;
}

void
bfd_pef_print_loader_header (bfd *abfd ATTRIBUTE_UNUSED,
			     bfd_pef_loader_header *header,
			     FILE *file)
{
  fprintf (file, "main_section: %ld\n", header->main_section);
  fprintf (file, "main_offset: %lu\n", header->main_offset);
  fprintf (file, "init_section: %ld\n", header->init_section);
  fprintf (file, "init_offset: %lu\n", header->init_offset);
  fprintf (file, "term_section: %ld\n", header->term_section);
  fprintf (file, "term_offset: %lu\n", header->term_offset);
  fprintf (file, "imported_library_count: %lu\n",
	   header->imported_library_count);
  fprintf (file, "total_imported_symbol_count: %lu\n",
	   header->total_imported_symbol_count);
  fprintf (file, "reloc_section_count: %lu\n", header->reloc_section_count);
  fprintf (file, "reloc_instr_offset: %lu\n", header->reloc_instr_offset);
  fprintf (file, "loader_strings_offset: %lu\n",
	   header->loader_strings_offset);
  fprintf (file, "export_hash_offset: %lu\n", header->export_hash_offset);
  fprintf (file, "export_hash_table_power: %lu\n",
	   header->export_hash_table_power);
  fprintf (file, "exported_symbol_count: %lu\n",
	   header->exported_symbol_count);
}

int
bfd_pef_print_loader_section (bfd *abfd, FILE *file)
{
  bfd_pef_loader_header header;
  asection *loadersec = NULL;
  unsigned char *loaderbuf = NULL;
  size_t loaderlen = 0;

  loadersec = bfd_get_section_by_name (abfd, "loader");
  if (loadersec == NULL)
    return -1;

  loaderlen = loadersec->size;
  loaderbuf = bfd_malloc (loaderlen);

  if (bfd_seek (abfd, loadersec->filepos, SEEK_SET) < 0
      || bfd_bread ((void *) loaderbuf, loaderlen, abfd) != loaderlen
      || loaderlen < 56
      || bfd_pef_parse_loader_header (abfd, loaderbuf, 56, &header) < 0)
    {
      free (loaderbuf);
      return -1;
    }

  bfd_pef_print_loader_header (abfd, &header, file);
  return 0;
}

int
bfd_pef_scan_start_address (bfd *abfd)
{
  bfd_pef_loader_header header;
  asection *section;

  asection *loadersec = NULL;
  unsigned char *loaderbuf = NULL;
  size_t loaderlen = 0;
  int ret;

  loadersec = bfd_get_section_by_name (abfd, "loader");
  if (loadersec == NULL)
    goto end;

  loaderlen = loadersec->size;
  loaderbuf = bfd_malloc (loaderlen);
  if (bfd_seek (abfd, loadersec->filepos, SEEK_SET) < 0)
    goto error;
  if (bfd_bread ((void *) loaderbuf, loaderlen, abfd) != loaderlen)
    goto error;

  if (loaderlen < 56)
    goto error;
  ret = bfd_pef_parse_loader_header (abfd, loaderbuf, 56, &header);
  if (ret < 0)
    goto error;

  if (header.main_section < 0)
    goto end;

  for (section = abfd->sections; section != NULL; section = section->next)
    if ((section->index + 1) == header.main_section)
      break;

  if (section == NULL)
    goto error;

  abfd->start_address = section->vma + header.main_offset;

 end:
  if (loaderbuf != NULL)
    free (loaderbuf);
  return 0;

 error:
  if (loaderbuf != NULL)
    free (loaderbuf);
  return -1;
}

int
bfd_pef_scan (abfd, header, mdata)
     bfd *abfd;
     bfd_pef_header *header;
     bfd_pef_data_struct *mdata;
{
  unsigned int i;
  enum bfd_architecture cputype;
  unsigned long cpusubtype;

  mdata->header = *header;

  bfd_pef_convert_architecture (header->architecture, &cputype, &cpusubtype);
  if (cputype == bfd_arch_unknown)
    {
      fprintf (stderr, "bfd_pef_scan: unknown architecture 0x%lx\n",
	       header->architecture);
      return -1;
    }
  bfd_set_arch_mach (abfd, cputype, cpusubtype);

  mdata->header = *header;

  abfd->flags = (abfd->xvec->object_flags
		 | (abfd->flags & (BFD_IN_MEMORY | BFD_IO_FUNCS)));

  if (header->section_count != 0)
    {
      mdata->sections = bfd_alloc (abfd, header->section_count * sizeof (bfd_pef_section));

      if (mdata->sections == NULL)
	return -1;

      for (i = 0; i < header->section_count; i++)
	{
	  bfd_pef_section *cur = &mdata->sections[i];
	  cur->header_offset = 40 + (i * 28);
	  if (bfd_pef_scan_section (abfd, cur) < 0)
	    return -1;
	}
    }

  if (bfd_pef_scan_start_address (abfd) < 0)
    return -1;

  abfd->tdata.pef_data = mdata;

  return 0;
}

static int
bfd_pef_read_header (bfd *abfd, bfd_pef_header *header)
{
  unsigned char buf[40];

  bfd_seek (abfd, 0, SEEK_SET);

  if (bfd_bread ((void *) buf, 40, abfd) != 40)
    return -1;

  header->tag1 = bfd_getb32 (buf);
  header->tag2 = bfd_getb32 (buf + 4);
  header->architecture = bfd_getb32 (buf + 8);
  header->format_version = bfd_getb32 (buf + 12);
  header->timestamp = bfd_getb32 (buf + 16);
  header->old_definition_version = bfd_getb32 (buf + 20);
  header->old_implementation_version = bfd_getb32 (buf + 24);
  header->current_version = bfd_getb32 (buf + 28);
  header->section_count = bfd_getb32 (buf + 32) + 1;
  header->instantiated_section_count = bfd_getb32 (buf + 34);
  header->reserved = bfd_getb32 (buf + 36);

  return 0;
}

static const bfd_target *
bfd_pef_object_p (bfd *abfd)
{
  struct bfd_preserve preserve;
  bfd_pef_header header;

  preserve.marker = NULL;
  if (bfd_pef_read_header (abfd, &header) != 0)
    goto wrong;

  if (header.tag1 != BFD_PEF_TAG1 || header.tag2 != BFD_PEF_TAG2)
    goto wrong;

  preserve.marker = bfd_zalloc (abfd, sizeof (bfd_pef_data_struct));
  if (preserve.marker == NULL
      || !bfd_preserve_save (abfd, &preserve))
    goto fail;

  if (bfd_pef_scan (abfd, &header,
		    (bfd_pef_data_struct *) preserve.marker) != 0)
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

static int
bfd_pef_parse_traceback_tables (bfd *abfd,
				asection *sec,
				unsigned char *buf,
				size_t len,
				long *nsym,
				asymbol **csym)
{
  char *name;

  asymbol function;
  asymbol traceback;

  const char *const tbprefix = "__traceback_";
  size_t tbnamelen;

  size_t pos = 0;
  unsigned long count = 0;
  int ret;

  for (;;)
    {
      /* We're reading symbols two at a time.  */
      if (csym && ((csym[count] == NULL) || (csym[count + 1] == NULL)))
	break;

      pos += 3;
      pos -= (pos % 4);

      while ((pos + 4) <= len)
	{
	  if (bfd_getb32 (buf + pos) == 0)
	    break;
	  pos += 4;
	}

      if ((pos + 4) > len)
	break;

      ret = bfd_pef_parse_traceback_table (abfd, sec, buf, len, pos + 4,
					   &function, 0);
      if (ret < 0)
	{
	  /* Skip over 0x0L to advance to next possible traceback table.  */
	  pos += 4;
	  continue;
	}

      BFD_ASSERT (function.name != NULL);

      /* Don't bother to compute the name if we are just
	 counting symbols.  */
      if (csym)
	{
	  tbnamelen = strlen (tbprefix) + strlen (function.name);
	  name = bfd_alloc (abfd, tbnamelen + 1);
	  if (name == NULL)
	    {
	      bfd_release (abfd, (void *) function.name);
	      function.name = NULL;
	      break;
	    }
	  snprintf (name, tbnamelen + 1, "%s%s", tbprefix, function.name);
	  traceback.name = name;
	  traceback.value = pos;
	  traceback.the_bfd = abfd;
	  traceback.section = sec;
	  traceback.flags = 0;
	  traceback.udata.i = ret;

	  *(csym[count]) = function;
	  *(csym[count + 1]) = traceback;
	}

      pos += ret;
      count += 2;
    }

  *nsym = count;
  return 0;
}

static int
bfd_pef_parse_function_stub (bfd *abfd ATTRIBUTE_UNUSED,
			     unsigned char *buf,
			     size_t len,
			     unsigned long *offset)
{
  BFD_ASSERT (len == 24);

  if ((bfd_getb32 (buf) & 0xffff0000) != 0x81820000)
    return -1;
  if (bfd_getb32 (buf + 4) != 0x90410014)
    return -1;
  if (bfd_getb32 (buf + 8) != 0x800c0000)
    return -1;
  if (bfd_getb32 (buf + 12) != 0x804c0004)
    return -1;
  if (bfd_getb32 (buf + 16) != 0x7c0903a6)
    return -1;
  if (bfd_getb32 (buf + 20) != 0x4e800420)
    return -1;

  if (offset != NULL)
    *offset = (bfd_getb32 (buf) & 0x0000ffff) / 4;

  return 0;
}

static int
bfd_pef_parse_function_stubs (bfd *abfd,
			      asection *codesec,
			      unsigned char *codebuf,
			      size_t codelen,
			      unsigned char *loaderbuf,
			      size_t loaderlen,
			      unsigned long *nsym,
			      asymbol **csym)
{
  const char *const sprefix = "__stub_";

  size_t codepos = 0;
  unsigned long count = 0;

  bfd_pef_loader_header header;
  bfd_pef_imported_library *libraries = NULL;
  bfd_pef_imported_symbol *imports = NULL;

  unsigned long i;
  int ret;

  if (loaderlen < 56)
    goto error;

  ret = bfd_pef_parse_loader_header (abfd, loaderbuf, 56, &header);
  if (ret < 0)
    goto error;

  libraries = bfd_malloc
    (header.imported_library_count * sizeof (bfd_pef_imported_library));
  imports = bfd_malloc
    (header.total_imported_symbol_count * sizeof (bfd_pef_imported_symbol));

  if (loaderlen < (56 + (header.imported_library_count * 24)))
    goto error;
  for (i = 0; i < header.imported_library_count; i++)
    {
      ret = bfd_pef_parse_imported_library
	(abfd, loaderbuf + 56 + (i * 24), 24, &libraries[i]);
      if (ret < 0)
	goto error;
    }

  if (loaderlen < (56 + (header.imported_library_count * 24)
		   + (header.total_imported_symbol_count * 4)))
    goto error;
  for (i = 0; i < header.total_imported_symbol_count; i++)
    {
      ret = (bfd_pef_parse_imported_symbol
	     (abfd,
	      loaderbuf + 56 + (header.imported_library_count * 24) + (i * 4),
	      4, &imports[i]));
      if (ret < 0)
	goto error;
    }

  codepos = 0;

  for (;;)
    {
      asymbol sym;
      const char *symname;
      char *name;
      unsigned long index;
      int ret;

      if (csym && (csym[count] == NULL))
	break;

      codepos += 3;
      codepos -= (codepos % 4);

      while ((codepos + 4) <= codelen)
	{
	  if ((bfd_getb32 (codebuf + codepos) & 0xffff0000) == 0x81820000)
	    break;
	  codepos += 4;
	}

      if ((codepos + 4) > codelen)
	break;

      ret = bfd_pef_parse_function_stub (abfd, codebuf + codepos, 24, &index);
      if (ret < 0)
	{
	  codepos += 24;
	  continue;
	}

      if (index >= header.total_imported_symbol_count)
	{
	  codepos += 24;
	  continue;
	}

      {
	size_t max, namelen;
	const char *s;

	if (loaderlen < (header.loader_strings_offset + imports[index].name))
	  goto error;

	max = loaderlen - (header.loader_strings_offset + imports[index].name);
	symname = (char *) loaderbuf;
	symname += header.loader_strings_offset + imports[index].name;
	namelen = 0;
	for (s = symname; s < (symname + max); s++)
	  {
	    if (*s == '\0')
	      break;
	    if (! ISPRINT (*s))
	      goto error;
	    namelen++;
	  }
	if (*s != '\0')
	  goto error;

	name = bfd_alloc (abfd, strlen (sprefix) + namelen + 1);
	if (name == NULL)
	  break;

	snprintf (name, strlen (sprefix) + namelen + 1, "%s%s",
		  sprefix, symname);
	sym.name = name;
      }

      sym.value = codepos;
      sym.the_bfd = abfd;
      sym.section = codesec;
      sym.flags = 0;
      sym.udata.i = 0;

      codepos += 24;

      if (csym != NULL)
	*(csym[count]) = sym;

      count++;
    }

  goto end;

 end:
  if (libraries != NULL)
    free (libraries);
  if (imports != NULL)
    free (imports);
  *nsym = count;
  return 0;

 error:
  if (libraries != NULL)
    free (libraries);
  if (imports != NULL)
    free (imports);
  *nsym = count;
  return -1;
}

static long
bfd_pef_parse_symbols (bfd *abfd, asymbol **csym)
{
  unsigned long count = 0;

  asection *codesec = NULL;
  unsigned char *codebuf = NULL;
  size_t codelen = 0;

  asection *loadersec = NULL;
  unsigned char *loaderbuf = NULL;
  size_t loaderlen = 0;

  codesec = bfd_get_section_by_name (abfd, "code");
  if (codesec != NULL)
    {
      codelen = codesec->size;
      codebuf = bfd_malloc (codelen);
      if (bfd_seek (abfd, codesec->filepos, SEEK_SET) < 0)
	goto end;
      if (bfd_bread ((void *) codebuf, codelen, abfd) != codelen)
	goto end;
    }

  loadersec = bfd_get_section_by_name (abfd, "loader");
  if (loadersec != NULL)
    {
      loaderlen = loadersec->size;
      loaderbuf = bfd_malloc (loaderlen);
      if (bfd_seek (abfd, loadersec->filepos, SEEK_SET) < 0)
	goto end;
      if (bfd_bread ((void *) loaderbuf, loaderlen, abfd) != loaderlen)
	goto end;
    }

  count = 0;
  if (codesec != NULL)
    {
      long ncount = 0;
      bfd_pef_parse_traceback_tables (abfd, codesec, codebuf, codelen,
				      &ncount, csym);
      count += ncount;
    }

  if ((codesec != NULL) && (loadersec != NULL))
    {
      unsigned long ncount = 0;
      bfd_pef_parse_function_stubs
	(abfd, codesec, codebuf, codelen, loaderbuf, loaderlen, &ncount,
	 (csym != NULL) ? (csym + count) : NULL);
      count += ncount;
    }

  if (csym != NULL)
    csym[count] = NULL;

 end:
  if (codebuf != NULL)
    free (codebuf);

  if (loaderbuf != NULL)
    free (loaderbuf);

  return count;
}

static long
bfd_pef_count_symbols (bfd *abfd)
{
  return bfd_pef_parse_symbols (abfd, NULL);
}

static long
bfd_pef_get_symtab_upper_bound (bfd *abfd)
{
  long nsyms = bfd_pef_count_symbols (abfd);

  if (nsyms < 0)
    return nsyms;
  return ((nsyms + 1) * sizeof (asymbol *));
}

static long
bfd_pef_canonicalize_symtab (bfd *abfd, asymbol **alocation)
{
  long i;
  asymbol *syms;
  long ret;
  long nsyms = bfd_pef_count_symbols (abfd);

  if (nsyms < 0)
    return nsyms;

  syms = bfd_alloc (abfd, nsyms * sizeof (asymbol));
  if (syms == NULL)
    return -1;

  for (i = 0; i < nsyms; i++)
    alocation[i] = &syms[i];

  alocation[nsyms] = NULL;

  ret = bfd_pef_parse_symbols (abfd, alocation);
  if (ret != nsyms)
    return 0;

  return ret;
}

static asymbol *
bfd_pef_make_empty_symbol (bfd *abfd)
{
  return bfd_alloc (abfd, sizeof (asymbol));
}

static void
bfd_pef_get_symbol_info (bfd *abfd ATTRIBUTE_UNUSED,
			 asymbol *symbol,
			 symbol_info *ret)
{
  bfd_symbol_info (symbol, ret);
}

static int
bfd_pef_sizeof_headers (bfd *abfd ATTRIBUTE_UNUSED, bfd_boolean exec ATTRIBUTE_UNUSED)
{
  return 0;
}

const bfd_target pef_vec =
{
  "pef",			/* Name.  */
  bfd_target_pef_flavour,	/* Flavour.  */
  BFD_ENDIAN_BIG,		/* Byteorder.  */
  BFD_ENDIAN_BIG,		/* Header_byteorder.  */
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
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Headers.  */
  {				/* bfd_check_format.  */
    _bfd_dummy_target,
    bfd_pef_object_p,		/* bfd_check_format.  */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {				/* bfd_set_format.  */
    bfd_false,
    bfd_pef_mkobject,
    bfd_false,
    bfd_false,
  },
  {				/* bfd_write_contents.  */
    bfd_false,
    bfd_true,
    bfd_false,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (bfd_pef),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (bfd_pef),
  BFD_JUMP_TABLE_RELOCS (bfd_pef),
  BFD_JUMP_TABLE_WRITE (bfd_pef),
  BFD_JUMP_TABLE_LINK (bfd_pef),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};

#define bfd_pef_xlib_close_and_cleanup              _bfd_generic_close_and_cleanup
#define bfd_pef_xlib_bfd_free_cached_info           _bfd_generic_bfd_free_cached_info
#define bfd_pef_xlib_new_section_hook               _bfd_generic_new_section_hook
#define bfd_pef_xlib_get_section_contents           _bfd_generic_get_section_contents
#define bfd_pef_xlib_set_section_contents           _bfd_generic_set_section_contents
#define bfd_pef_xlib_get_section_contents_in_window _bfd_generic_get_section_contents_in_window
#define bfd_pef_xlib_set_section_contents_in_window _bfd_generic_set_section_contents_in_window

static int
bfd_pef_xlib_read_header (bfd *abfd, bfd_pef_xlib_header *header)
{
  unsigned char buf[76];

  bfd_seek (abfd, 0, SEEK_SET);

  if (bfd_bread ((void *) buf, 76, abfd) != 76)
    return -1;

  header->tag1 = bfd_getb32 (buf);
  header->tag2 = bfd_getb32 (buf + 4);
  header->current_format = bfd_getb32 (buf + 8);
  header->container_strings_offset = bfd_getb32 (buf + 12);
  header->export_hash_offset = bfd_getb32 (buf + 16);
  header->export_key_offset = bfd_getb32 (buf + 20);
  header->export_symbol_offset = bfd_getb32 (buf + 24);
  header->export_names_offset = bfd_getb32 (buf + 28);
  header->export_hash_table_power = bfd_getb32 (buf + 32);
  header->exported_symbol_count = bfd_getb32 (buf + 36);
  header->frag_name_offset = bfd_getb32 (buf + 40);
  header->frag_name_length = bfd_getb32 (buf + 44);
  header->dylib_path_offset = bfd_getb32 (buf + 48);
  header->dylib_path_length = bfd_getb32 (buf + 52);
  header->cpu_family = bfd_getb32 (buf + 56);
  header->cpu_model = bfd_getb32 (buf + 60);
  header->date_time_stamp = bfd_getb32 (buf + 64);
  header->current_version = bfd_getb32 (buf + 68);
  header->old_definition_version = bfd_getb32 (buf + 72);
  header->old_implementation_version = bfd_getb32 (buf + 76);

  return 0;
}

static int
bfd_pef_xlib_scan (bfd *abfd, bfd_pef_xlib_header *header)
{
  bfd_pef_xlib_data_struct *mdata = NULL;

  mdata = bfd_alloc (abfd, sizeof (* mdata));
  if (mdata == NULL)
    return -1;

  mdata->header = *header;

  abfd->flags = (abfd->xvec->object_flags
		 | (abfd->flags & (BFD_IN_MEMORY | BFD_IO_FUNCS)));

  abfd->tdata.pef_xlib_data = mdata;

  return 0;
}

static const bfd_target *
bfd_pef_xlib_object_p (bfd *abfd)
{
  struct bfd_preserve preserve;
  bfd_pef_xlib_header header;

  if (bfd_pef_xlib_read_header (abfd, &header) != 0)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if ((header.tag1 != BFD_PEF_XLIB_TAG1)
      || ((header.tag2 != BFD_PEF_VLIB_TAG2)
	  && (header.tag2 != BFD_PEF_BLIB_TAG2)))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (! bfd_preserve_save (abfd, &preserve))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (bfd_pef_xlib_scan (abfd, &header) != 0)
    {
      bfd_preserve_restore (abfd, &preserve);
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  bfd_preserve_finish (abfd, &preserve);
  return abfd->xvec;
}

const bfd_target pef_xlib_vec =
{
  "pef-xlib",			/* Name.  */
  bfd_target_pef_xlib_flavour,	/* Flavour.  */
  BFD_ENDIAN_BIG,		/* Byteorder */
  BFD_ENDIAN_BIG,		/* Header_byteorder.  */
  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | DYNAMIC | WP_TEXT | D_PAGED),
  (SEC_ALLOC | SEC_LOAD | SEC_READONLY | SEC_CODE | SEC_DATA
   | SEC_ROM | SEC_HAS_CONTENTS),/* Section_flags.  */
  0,				/* Symbol_leading_char.  */
  ' ',				/* AR_pad_char.  */
  16,				/* AR_max_namelen.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Data.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Headers.  */
  {				/* bfd_check_format.  */
    _bfd_dummy_target,
    bfd_pef_xlib_object_p,	/* bfd_check_format.  */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {				/* bfd_set_format.  */
    bfd_false,
    bfd_pef_mkobject,
    bfd_false,
    bfd_false,
  },
  {				/* bfd_write_contents.  */
    bfd_false,
    bfd_true,
    bfd_false,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (bfd_pef_xlib),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
  BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
  BFD_JUMP_TABLE_WRITE (_bfd_nowrite),
  BFD_JUMP_TABLE_LINK (_bfd_nolink),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};
