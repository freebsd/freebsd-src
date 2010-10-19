/* BFD back-end for VERSAdos-E objects.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support <sac@cygnus.com>.

   Versados is a Motorola trademark.

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

/*
   SUBSECTION
   VERSAdos-E relocatable object file format

   DESCRIPTION

   This module supports reading of VERSAdos relocatable
   object files.

   A VERSAdos file looks like contains

   o Identification Record
   o External Symbol Definition Record
   o Object Text Record
   o End Record.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"


#define VHEADER '1'
#define VESTDEF '2'
#define VOTR '3'
#define VEND '4'

#define ES_BASE 17		/* First symbol has esdid 17.  */

/* Per file target dependent information.  */

/* One for each section.  */
struct esdid
{
  asection *section;		/* Ptr to bfd version.  */
  unsigned char *contents;	/* Used to build image.  */
  int pc;
  int relocs;			/* Reloc count, valid end of pass 1.  */
  int donerel;			/* Have relocs been translated.  */
};

typedef struct versados_data_struct
{
  int es_done;			/* Count of symbol index, starts at ES_BASE.  */
  asymbol *symbols;		/* Pointer to local symbols.  */
  char *strings;		/* Strings of all the above.  */
  int stringlen;		/* Len of string table (valid end of pass1).  */
  int nsecsyms;			/* Number of sections.  */

  int ndefs;			/* Number of exported symbols (they dont get esdids).  */
  int nrefs;			/* Number of imported symbols  (valid end of pass1).  */

  int ref_idx;			/* Current processed value of the above.  */
  int def_idx;

  int pass_2_done;

  struct esdid e[16];		/* Per section info.  */
  int alert;			/* To see if we're trampling.  */
  asymbol *rest[256 - 16];	/* Per symbol info.  */
}
tdata_type;

#define VDATA(abfd)       (abfd->tdata.versados_data)
#define EDATA(abfd, n)    (abfd->tdata.versados_data->e[n])
#define RDATA(abfd, n)    (abfd->tdata.versados_data->rest[n])

struct ext_otr
{
  unsigned char size;
  char type;
  unsigned char map[4];
  unsigned char esdid;
  unsigned char data[200];
};

struct ext_vheader
{
  unsigned char size;
  char type;			/* Record type.  */
  char name[10];		/* Module name.  */
  char rev;			/* Module rev number.  */
  char lang;
  char vol[4];
  char user[2];
  char cat[8];
  char fname[8];
  char ext[2];
  char time[3];
  char date[3];
  char rest[211];
};

struct ext_esd
{
  unsigned char size;
  char type;
  unsigned char esd_entries[1];
};

#define ESD_ABS 	  0
#define ESD_COMMON 	  1
#define ESD_STD_REL_SEC   2
#define ESD_SHRT_REL_SEC  3
#define ESD_XDEF_IN_SEC   4
#define ESD_XDEF_IN_ABS   5
#define ESD_XREF_SEC	  6
#define ESD_XREF_SYM      7

union ext_any
{
  unsigned char size;
  struct ext_vheader header;
  struct ext_esd esd;
  struct ext_otr otr;
};

/* Initialize by filling in the hex conversion array.  */

/* Set up the tdata information.  */

static bfd_boolean
versados_mkobject (bfd *abfd)
{
  if (abfd->tdata.versados_data == NULL)
    {
      bfd_size_type amt = sizeof (tdata_type);
      tdata_type *tdata = bfd_alloc (abfd, amt);

      if (tdata == NULL)
	return FALSE;
      abfd->tdata.versados_data = tdata;
      tdata->symbols = NULL;
      VDATA (abfd)->alert = 0x12345678;
    }

  bfd_default_set_arch_mach (abfd, bfd_arch_m68k, 0);
  return TRUE;
}

/* Report a problem in an S record file.  FIXME: This probably should
   not call fprintf, but we really do need some mechanism for printing
   error messages.  */

static asymbol *
versados_new_symbol (bfd *abfd,
		     int snum,
		     const char *name,
		     bfd_vma val,
		     asection *sec)
{
  asymbol *n = VDATA (abfd)->symbols + snum;
  n->name = name;
  n->value = val;
  n->section = sec;
  n->the_bfd = abfd;
  n->flags = 0;
  return n;
}

static int
get_record (bfd *abfd, union ext_any *ptr)
{
  if (bfd_bread (&ptr->size, (bfd_size_type) 1, abfd) != 1
      || (bfd_bread ((char *) ptr + 1, (bfd_size_type) ptr->size, abfd)
	  != ptr->size))
    return 0;
  return 1;
}

static int
get_4 (unsigned char **pp)
{
  unsigned char *p = *pp;

  *pp += 4;
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
}

static void
get_10 (unsigned char **pp, char *name)
{
  char *p = (char *) *pp;
  int len = 10;

  *pp += len;
  while (*p != ' ' && len)
    {
      *name++ = *p++;
      len--;
    }
  *name = 0;
}

static char *
new_symbol_string (bfd *abfd, const char *name)
{
  char *n = VDATA (abfd)->strings;

  strcpy (VDATA (abfd)->strings, name);
  VDATA (abfd)->strings += strlen (VDATA (abfd)->strings) + 1;
  return n;
}

static void
process_esd (bfd *abfd, struct ext_esd *esd, int pass)
{
  /* Read through the ext def for the est entries.  */
  int togo = esd->size - 2;
  bfd_vma size;
  bfd_vma start;
  asection *sec;
  char name[11];
  unsigned char *ptr = esd->esd_entries;
  unsigned char *end = ptr + togo;

  while (ptr < end)
    {
      int scn = *ptr & 0xf;
      int typ = (*ptr >> 4) & 0xf;

      /* Declare this section.  */
      sprintf (name, "%d", scn);
      sec = bfd_make_section_old_way (abfd, strdup (name));
      sec->target_index = scn;
      EDATA (abfd, scn).section = sec;
      ptr++;

      switch (typ)
	{
	default:
	  abort ();
	case ESD_XREF_SEC:
	case ESD_XREF_SYM:
	  {
	    int snum = VDATA (abfd)->ref_idx++;
	    get_10 (&ptr, name);
	    if (pass == 1)
	      VDATA (abfd)->stringlen += strlen (name) + 1;
	    else
	      {
		int esidx;
		asymbol *s;
		char *n = new_symbol_string (abfd, name);

		s = versados_new_symbol (abfd, snum, n, (bfd_vma) 0,
					 bfd_und_section_ptr);
		esidx = VDATA (abfd)->es_done++;
		RDATA (abfd, esidx - ES_BASE) = s;
	      }
	  }
	  break;

	case ESD_ABS:
	  size = get_4 (&ptr);
	  start = get_4 (&ptr);
	  break;
	case ESD_STD_REL_SEC:
	case ESD_SHRT_REL_SEC:
	  sec->size = get_4 (&ptr);
	  sec->flags |= SEC_ALLOC;
	  break;
	case ESD_XDEF_IN_ABS:
	  sec = (asection *) & bfd_abs_section;
	case ESD_XDEF_IN_SEC:
	  {
	    int snum = VDATA (abfd)->def_idx++;
	    bfd_vma val;

	    get_10 (&ptr, name);
	    val = get_4 (&ptr);
	    if (pass == 1)
	      /* Just remember the symbol.  */
	      VDATA (abfd)->stringlen += strlen (name) + 1;
	    else
	      {
		asymbol *s;
		char *n = new_symbol_string (abfd, name);

		s = versados_new_symbol (abfd, snum + VDATA (abfd)->nrefs, n,
					 val, sec);
		s->flags |= BSF_GLOBAL;
	      }
	  }
	  break;
	}
    }
}

#define R_RELWORD     1
#define R_RELLONG     2
#define R_RELWORD_NEG 3
#define R_RELLONG_NEG 4

reloc_howto_type versados_howto_table[] =
{
  HOWTO (R_RELWORD, 0, 1, 16, FALSE,
	 0, complain_overflow_dont, 0,
	 "+v16", TRUE, 0x0000ffff, 0x0000ffff, FALSE),
  HOWTO (R_RELLONG, 0, 2, 32, FALSE,
	 0, complain_overflow_dont, 0,
	 "+v32", TRUE, 0xffffffff, 0xffffffff, FALSE),

  HOWTO (R_RELWORD_NEG, 0, -1, 16, FALSE,
	 0, complain_overflow_dont, 0,
	 "-v16", TRUE, 0x0000ffff, 0x0000ffff, FALSE),
  HOWTO (R_RELLONG_NEG, 0, -2, 32, FALSE,
	 0, complain_overflow_dont, 0,
	 "-v32", TRUE, 0xffffffff, 0xffffffff, FALSE),
};

static int
get_offset (int len, unsigned char *ptr)
{
  int val = 0;

  if (len)
    {
      int i;

      val = *ptr++;
      if (val & 0x80)
	val |= ~0xff;
      for (i = 1; i < len; i++)
	val = (val << 8) | *ptr++;
    }

  return val;
}

static void
process_otr (bfd *abfd, struct ext_otr *otr, int pass)
{
  unsigned long shift;
  unsigned char *srcp = otr->data;
  unsigned char *endp = (unsigned char *) otr + otr->size;
  unsigned int bits = (otr->map[0] << 24)
  | (otr->map[1] << 16)
  | (otr->map[2] << 8)
  | (otr->map[3] << 0);

  struct esdid *esdid = &EDATA (abfd, otr->esdid - 1);
  unsigned char *contents = esdid->contents;
  int need_contents = 0;
  unsigned int dst_idx = esdid->pc;

  for (shift = ((unsigned long) 1 << 31); shift && srcp < endp; shift >>= 1)
    {
      if (bits & shift)
	{
	  int flag = *srcp++;
	  int esdids = (flag >> 5) & 0x7;
	  int sizeinwords = ((flag >> 3) & 1) ? 2 : 1;
	  int offsetlen = flag & 0x7;
	  int j;

	  if (esdids == 0)
	    {
	      /* A zero esdid means the new pc is the offset given.  */
	      dst_idx += get_offset (offsetlen, srcp);
	      srcp += offsetlen;
	    }
	  else
	    {
	      int val = get_offset (offsetlen, srcp + esdids);

	      if (pass == 1)
		need_contents = 1;
	      else
		for (j = 0; j < sizeinwords * 2; j++)
		  {
		    contents[dst_idx + (sizeinwords * 2) - j - 1] = val;
		    val >>= 8;
		  }

	      for (j = 0; j < esdids; j++)
		{
		  int esdid = *srcp++;

		  if (esdid)
		    {
		      int rn = EDATA (abfd, otr->esdid - 1).relocs++;

		      if (pass == 1)
			{
			  /* This is the first pass over the data,
			     just remember that we need a reloc.  */
			}
		      else
			{
			  arelent *n =
			  EDATA (abfd, otr->esdid - 1).section->relocation + rn;
			  n->address = dst_idx;

			  n->sym_ptr_ptr = (asymbol **) (size_t) esdid;
			  n->addend = 0;
			  n->howto = versados_howto_table + ((j & 1) * 2) + (sizeinwords - 1);
			}
		    }
		}
	      srcp += offsetlen;
	      dst_idx += sizeinwords * 2;
	    }
	}
      else
	{
	  need_contents = 1;
	  if (dst_idx < esdid->section->size)
	    if (pass == 2)
	      {
		/* Absolute code, comes in 16 bit lumps.  */
		contents[dst_idx] = srcp[0];
		contents[dst_idx + 1] = srcp[1];
	      }
	  dst_idx += 2;
	  srcp += 2;
	}
    }
  EDATA (abfd, otr->esdid - 1).pc = dst_idx;

  if (!contents && need_contents)
    {
      bfd_size_type size = esdid->section->size;
      esdid->contents = bfd_alloc (abfd, size);
    }
}

static bfd_boolean
versados_scan (bfd *abfd)
{
  int loop = 1;
  int i;
  int j;
  int nsecs = 0;
  bfd_size_type amt;

  VDATA (abfd)->stringlen = 0;
  VDATA (abfd)->nrefs = 0;
  VDATA (abfd)->ndefs = 0;
  VDATA (abfd)->ref_idx = 0;
  VDATA (abfd)->def_idx = 0;
  VDATA (abfd)->pass_2_done = 0;

  while (loop)
    {
      union ext_any any;

      if (!get_record (abfd, &any))
	return TRUE;
      switch (any.header.type)
	{
	case VHEADER:
	  break;
	case VEND:
	  loop = 0;
	  break;
	case VESTDEF:
	  process_esd (abfd, &any.esd, 1);
	  break;
	case VOTR:
	  process_otr (abfd, &any.otr, 1);
	  break;
	}
    }

  /* Now allocate space for the relocs and sections.  */
  VDATA (abfd)->nrefs = VDATA (abfd)->ref_idx;
  VDATA (abfd)->ndefs = VDATA (abfd)->def_idx;
  VDATA (abfd)->ref_idx = 0;
  VDATA (abfd)->def_idx = 0;

  abfd->symcount = VDATA (abfd)->nrefs + VDATA (abfd)->ndefs;

  for (i = 0; i < 16; i++)
    {
      struct esdid *esdid = &EDATA (abfd, i);

      if (esdid->section)
	{
	  amt = (bfd_size_type) esdid->relocs * sizeof (arelent);
	  esdid->section->relocation = bfd_alloc (abfd, amt);

	  esdid->pc = 0;

	  if (esdid->contents)
	    esdid->section->flags |= SEC_HAS_CONTENTS | SEC_LOAD;

	  esdid->section->reloc_count = esdid->relocs;
	  if (esdid->relocs)
	    esdid->section->flags |= SEC_RELOC;

	  esdid->relocs = 0;

	  /* Add an entry into the symbol table for it.  */
	  nsecs++;
	  VDATA (abfd)->stringlen += strlen (esdid->section->name) + 1;
	}
    }

  abfd->symcount += nsecs;

  amt = abfd->symcount;
  amt *= sizeof (asymbol);
  VDATA (abfd)->symbols = bfd_alloc (abfd, amt);

  amt = VDATA (abfd)->stringlen;
  VDATA (abfd)->strings = bfd_alloc (abfd, amt);

  if ((VDATA (abfd)->symbols == NULL && abfd->symcount > 0)
      || (VDATA (abfd)->strings == NULL && VDATA (abfd)->stringlen > 0))
    return FALSE;

  /* Actually fill in the section symbols,
     we stick them at the end of the table.  */
  for (j = VDATA (abfd)->nrefs + VDATA (abfd)->ndefs, i = 0; i < 16; i++)
    {
      struct esdid *esdid = &EDATA (abfd, i);
      asection *sec = esdid->section;

      if (sec)
	{
	  asymbol *s = VDATA (abfd)->symbols + j;
	  s->name = new_symbol_string (abfd, sec->name);
	  s->section = sec;
	  s->flags = BSF_LOCAL;
	  s->value = 0;
	  s->the_bfd = abfd;
	  j++;
	}
    }

  if (abfd->symcount)
    abfd->flags |= HAS_SYMS;

  /* Set this to nsecs - since we've already planted the section
     symbols.  */
  VDATA (abfd)->nsecsyms = nsecs;

  VDATA (abfd)->ref_idx = 0;

  return 1;
}

/* Check whether an existing file is a versados  file.  */

static const bfd_target *
versados_object_p (bfd *abfd)
{
  struct ext_vheader ext;
  unsigned char len;
  tdata_type *tdata_save;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return NULL;

  if (bfd_bread (&len, (bfd_size_type) 1, abfd) != 1)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (bfd_bread (&ext.type, (bfd_size_type) len, abfd) != len)
    {
      if (bfd_get_error () != bfd_error_system_call)
	bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* We guess that the language field will never be larger than 10.
     In sample files, it is always either 0 or 1.  Checking for this
     prevents confusion with Intel Hex files.  */
  if (ext.type != VHEADER
      || ext.lang > 10)
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  /* OK, looks like a record, build the tdata and read in.  */
  tdata_save = abfd->tdata.versados_data;
  if (!versados_mkobject (abfd) || !versados_scan (abfd))
    {
      abfd->tdata.versados_data = tdata_save;
      return NULL;
    }

  return abfd->xvec;
}

static bfd_boolean
versados_pass_2 (bfd *abfd)
{
  union ext_any any;

  if (VDATA (abfd)->pass_2_done)
    return 1;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    return 0;

  VDATA (abfd)->es_done = ES_BASE;

  /* Read records till we get to where we want to be.  */
  while (1)
    {
      get_record (abfd, &any);
      switch (any.header.type)
	{
	case VEND:
	  VDATA (abfd)->pass_2_done = 1;
	  return 1;
	case VESTDEF:
	  process_esd (abfd, &any.esd, 2);
	  break;
	case VOTR:
	  process_otr (abfd, &any.otr, 2);
	  break;
	}
    }
}

static bfd_boolean
versados_get_section_contents (bfd *abfd,
			       asection *section,
			       void * location,
			       file_ptr offset,
			       bfd_size_type count)
{
  if (!versados_pass_2 (abfd))
    return FALSE;

  memcpy (location,
	  EDATA (abfd, section->target_index).contents + offset,
	  (size_t) count);

  return TRUE;
}

#define versados_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

static bfd_boolean
versados_set_section_contents (bfd *abfd ATTRIBUTE_UNUSED,
			       sec_ptr section ATTRIBUTE_UNUSED,
			       const void * location ATTRIBUTE_UNUSED,
			       file_ptr offset ATTRIBUTE_UNUSED,
			       bfd_size_type bytes_to_do ATTRIBUTE_UNUSED)
{
  return FALSE;
}

static int
versados_sizeof_headers (bfd *abfd ATTRIBUTE_UNUSED,
			 bfd_boolean exec ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Return the amount of memory needed to read the symbol table.  */

static long
versados_get_symtab_upper_bound (bfd *abfd)
{
  return (bfd_get_symcount (abfd) + 1) * sizeof (asymbol *);
}

/* Return the symbol table.  */

static long
versados_canonicalize_symtab (bfd *abfd, asymbol **alocation)
{
  unsigned int symcount = bfd_get_symcount (abfd);
  unsigned int i;
  asymbol *s;

  versados_pass_2 (abfd);

  for (i = 0, s = VDATA (abfd)->symbols;
       i < symcount;
       s++, i++)
    *alocation++ = s;

  *alocation = NULL;

  return symcount;
}

static void
versados_get_symbol_info (bfd *abfd ATTRIBUTE_UNUSED,
			  asymbol *symbol,
			  symbol_info *ret)
{
  bfd_symbol_info (symbol, ret);
}

static void
versados_print_symbol (bfd *abfd,
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
      fprintf (file, " %-5s %s",
	       symbol->section->name,
	       symbol->name);
    }
}

static long
versados_get_reloc_upper_bound (bfd *abfd ATTRIBUTE_UNUSED,
				sec_ptr asect)
{
  return (asect->reloc_count + 1) * sizeof (arelent *);
}

static long
versados_canonicalize_reloc (bfd *abfd,
			     sec_ptr section,
			     arelent **relptr,
			     asymbol **symbols)
{
  unsigned int count;
  arelent *src;

  versados_pass_2 (abfd);
  src = section->relocation;
  if (!EDATA (abfd, section->target_index).donerel)
    {
      EDATA (abfd, section->target_index).donerel = 1;
      /* Translate from indexes to symptr ptrs.  */
      for (count = 0; count < section->reloc_count; count++)
	{
	  int esdid = (int) (size_t) src[count].sym_ptr_ptr;

	  if (esdid == 0)
	    src[count].sym_ptr_ptr = bfd_abs_section.symbol_ptr_ptr;
	  else if (esdid < ES_BASE)
	    {
	      /* Section relative thing.  */
	      struct esdid *e = &EDATA (abfd, esdid - 1);

	      src[count].sym_ptr_ptr = e->section->symbol_ptr_ptr;
	    }
	  else
	    src[count].sym_ptr_ptr = symbols + esdid - ES_BASE;
	}
    }

  for (count = 0; count < section->reloc_count; count++)
    *relptr++ = src++;

  *relptr = 0;
  return section->reloc_count;
}

#define	versados_close_and_cleanup                    _bfd_generic_close_and_cleanup
#define versados_bfd_free_cached_info                 _bfd_generic_bfd_free_cached_info
#define versados_new_section_hook                     _bfd_generic_new_section_hook
#define versados_bfd_is_target_special_symbol   ((bfd_boolean (*) (bfd *, asymbol *)) bfd_false)
#define versados_bfd_is_local_label_name              bfd_generic_is_local_label_name
#define versados_get_lineno                           _bfd_nosymbols_get_lineno
#define versados_find_nearest_line                    _bfd_nosymbols_find_nearest_line
#define versados_find_inliner_info                    _bfd_nosymbols_find_inliner_info
#define versados_make_empty_symbol                    _bfd_generic_make_empty_symbol
#define versados_bfd_make_debug_symbol                _bfd_nosymbols_bfd_make_debug_symbol
#define versados_read_minisymbols                     _bfd_generic_read_minisymbols
#define versados_minisymbol_to_symbol                 _bfd_generic_minisymbol_to_symbol
#define versados_bfd_reloc_type_lookup                _bfd_norelocs_bfd_reloc_type_lookup
#define versados_set_arch_mach                        bfd_default_set_arch_mach
#define versados_bfd_get_relocated_section_contents   bfd_generic_get_relocated_section_contents
#define versados_bfd_relax_section                    bfd_generic_relax_section
#define versados_bfd_gc_sections                      bfd_generic_gc_sections
#define versados_bfd_merge_sections                   bfd_generic_merge_sections
#define versados_bfd_is_group_section                 bfd_generic_is_group_section
#define versados_bfd_discard_group                    bfd_generic_discard_group
#define versados_section_already_linked               _bfd_generic_section_already_linked
#define versados_bfd_link_hash_table_create           _bfd_generic_link_hash_table_create
#define versados_bfd_link_hash_table_free             _bfd_generic_link_hash_table_free
#define versados_bfd_link_add_symbols                 _bfd_generic_link_add_symbols
#define versados_bfd_link_just_syms                   _bfd_generic_link_just_syms
#define versados_bfd_final_link                       _bfd_generic_final_link
#define versados_bfd_link_split_section               _bfd_generic_link_split_section

const bfd_target versados_vec =
{
  "versados",			/* Name.  */
  bfd_target_versados_flavour,
  BFD_ENDIAN_BIG,		/* Target byte order.  */
  BFD_ENDIAN_BIG,		/* Target headers byte order.  */
  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),		/* Section flags.  */
  0,				/* Leading underscore.  */
  ' ',				/* AR_pad_char.  */
  16,				/* AR_max_namelen.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Data.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Headers.  */

  {
    _bfd_dummy_target,
    versados_object_p,		/* bfd_check_format.  */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    versados_mkobject,
    _bfd_generic_mkarchive,
    bfd_false,
  },
  {				/* bfd_write_contents.  */
    bfd_false,
    bfd_false,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (versados),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (versados),
  BFD_JUMP_TABLE_RELOCS (versados),
  BFD_JUMP_TABLE_WRITE (versados),
  BFD_JUMP_TABLE_LINK (versados),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};
