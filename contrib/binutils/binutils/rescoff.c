/* rescoff.c -- read and write resources in Windows COFF files.
   Copyright 1997, 1998, 1999, 2000, 2003
   Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support.

   This file is part of GNU Binutils.

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* This file contains function that read and write Windows resources
   in COFF files.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "windres.h"

#include <assert.h>

/* In order to use the address of a resource data entry, we need to
   get the image base of the file.  Right now we extract it from
   internal BFD information.  FIXME.  */

#include "coff/internal.h"
#include "libcoff.h"

/* Information we extract from the file.  */

struct coff_file_info
{
  /* File name.  */
  const char *filename;
  /* Data read from the file.  */
  const bfd_byte *data;
  /* End of data read from file.  */
  const bfd_byte *data_end;
  /* Address of the resource section minus the image base of the file.  */
  bfd_vma secaddr;
  /* Non-zero if the file is big endian.  */
  int big_endian;
};

/* A resource directory table in a COFF file.  */

struct extern_res_directory
{
  /* Characteristics.  */
  bfd_byte characteristics[4];
  /* Time stamp.  */
  bfd_byte time[4];
  /* Major version number.  */
  bfd_byte major[2];
  /* Minor version number.  */
  bfd_byte minor[2];
  /* Number of named directory entries.  */
  bfd_byte name_count[2];
  /* Number of directory entries with IDs.  */
  bfd_byte id_count[2];
};

/* A resource directory entry in a COFF file.  */

struct extern_res_entry
{
  /* Name or ID.  */
  bfd_byte name[4];
  /* Address of resource entry or subdirectory.  */
  bfd_byte rva[4];
};

/* A resource data entry in a COFF file.  */

struct extern_res_data
{
  /* Address of resource data.  This is apparently a file relative
     address, rather than a section offset.  */
  bfd_byte rva[4];
  /* Size of resource data.  */
  bfd_byte size[4];
  /* Code page.  */
  bfd_byte codepage[4];
  /* Reserved.  */
  bfd_byte reserved[4];
};

/* Macros to swap in values.  */

#define getfi_16(fi, s) ((fi)->big_endian ? bfd_getb16 (s) : bfd_getl16 (s))
#define getfi_32(fi, s) ((fi)->big_endian ? bfd_getb32 (s) : bfd_getl32 (s))

/* Local functions.  */

static void overrun (const struct coff_file_info *, const char *);
static struct res_directory *read_coff_res_dir
  (const bfd_byte *, const struct coff_file_info *,
   const struct res_id *, int);
static struct res_resource *read_coff_data_entry
  (const bfd_byte *, const struct coff_file_info *, const struct res_id *);

/* Read the resources in a COFF file.  */

struct res_directory *
read_coff_rsrc (const char *filename, const char *target)
{
  bfd *abfd;
  char **matching;
  asection *sec;
  bfd_size_type size;
  bfd_byte *data;
  struct coff_file_info finfo;

  if (filename == NULL)
    fatal (_("filename required for COFF input"));

  abfd = bfd_openr (filename, target);
  if (abfd == NULL)
    bfd_fatal (filename);

  if (! bfd_check_format_matches (abfd, bfd_object, &matching))
    {
      bfd_nonfatal (bfd_get_filename (abfd));
      if (bfd_get_error () == bfd_error_file_ambiguously_recognized)
	list_matching_formats (matching);
      xexit (1);
    }

  sec = bfd_get_section_by_name (abfd, ".rsrc");
  if (sec == NULL)
    {
      fatal (_("%s: no resource section"), filename);
    }

  size = bfd_section_size (abfd, sec);
  data = (bfd_byte *) res_alloc (size);

  if (! bfd_get_section_contents (abfd, sec, data, 0, size))
    bfd_fatal (_("can't read resource section"));

  finfo.filename = filename;
  finfo.data = data;
  finfo.data_end = data + size;
  finfo.secaddr = (bfd_get_section_vma (abfd, sec)
		   - pe_data (abfd)->pe_opthdr.ImageBase);
  finfo.big_endian = bfd_big_endian (abfd);

  bfd_close (abfd);

  /* Now just read in the top level resource directory.  Note that we
     don't free data, since we create resource entries that point into
     it.  If we ever want to free up the resource information we read,
     this will have to be cleaned up.  */

  return read_coff_res_dir (data, &finfo, (const struct res_id *) NULL, 0);
}

/* Give an error if we are out of bounds.  */

static void
overrun (const struct coff_file_info *finfo, const char *msg)
{
  fatal (_("%s: %s: address out of bounds"), finfo->filename, msg);
}

/* Read a resource directory.  */

static struct res_directory *
read_coff_res_dir (const bfd_byte *data, const struct coff_file_info *finfo,
		   const struct res_id *type, int level)
{
  const struct extern_res_directory *erd;
  struct res_directory *rd;
  int name_count, id_count, i;
  struct res_entry **pp;
  const struct extern_res_entry *ere;

  if ((size_t) (finfo->data_end - data) < sizeof (struct extern_res_directory))
    overrun (finfo, _("directory"));

  erd = (const struct extern_res_directory *) data;

  rd = (struct res_directory *) res_alloc (sizeof *rd);
  rd->characteristics = getfi_32 (finfo, erd->characteristics);
  rd->time = getfi_32 (finfo, erd->time);
  rd->major = getfi_16 (finfo, erd->major);
  rd->minor = getfi_16 (finfo, erd->minor);
  rd->entries = NULL;

  name_count = getfi_16 (finfo, erd->name_count);
  id_count = getfi_16 (finfo, erd->id_count);

  pp = &rd->entries;

  /* The resource directory entries immediately follow the directory
     table.  */
  ere = (const struct extern_res_entry *) (erd + 1);

  for (i = 0; i < name_count; i++, ere++)
    {
      unsigned long name, rva;
      struct res_entry *re;
      const bfd_byte *ers;
      int length, j;

      if ((const bfd_byte *) ere >= finfo->data_end)
	overrun (finfo, _("named directory entry"));

      name = getfi_32 (finfo, ere->name);
      rva = getfi_32 (finfo, ere->rva);

      /* For some reason the high bit in NAME is set.  */
      name &=~ 0x80000000;

      if (name > (size_t) (finfo->data_end - finfo->data))
	overrun (finfo, _("directory entry name"));

      ers = finfo->data + name;

      re = (struct res_entry *) res_alloc (sizeof *re);
      re->next = NULL;
      re->id.named = 1;
      length = getfi_16 (finfo, ers);
      re->id.u.n.length = length;
      re->id.u.n.name = (unichar *) res_alloc (length * sizeof (unichar));
      for (j = 0; j < length; j++)
	re->id.u.n.name[j] = getfi_16 (finfo, ers + j * 2 + 2);

      if (level == 0)
	type = &re->id;

      if ((rva & 0x80000000) != 0)
	{
	  rva &=~ 0x80000000;
	  if (rva >= (size_t) (finfo->data_end - finfo->data))
	    overrun (finfo, _("named subdirectory"));
	  re->subdir = 1;
	  re->u.dir = read_coff_res_dir (finfo->data + rva, finfo, type,
					 level + 1);
	}
      else
	{
	  if (rva >= (size_t) (finfo->data_end - finfo->data))
	    overrun (finfo, _("named resource"));
	  re->subdir = 0;
	  re->u.res = read_coff_data_entry (finfo->data + rva, finfo, type);
	}

      *pp = re;
      pp = &re->next;
    }

  for (i = 0; i < id_count; i++, ere++)
    {
      unsigned long name, rva;
      struct res_entry *re;

      if ((const bfd_byte *) ere >= finfo->data_end)
	overrun (finfo, _("ID directory entry"));

      name = getfi_32 (finfo, ere->name);
      rva = getfi_32 (finfo, ere->rva);

      re = (struct res_entry *) res_alloc (sizeof *re);
      re->next = NULL;
      re->id.named = 0;
      re->id.u.id = name;

      if (level == 0)
	type = &re->id;

      if ((rva & 0x80000000) != 0)
	{
	  rva &=~ 0x80000000;
	  if (rva >= (size_t) (finfo->data_end - finfo->data))
	    overrun (finfo, _("ID subdirectory"));
	  re->subdir = 1;
	  re->u.dir = read_coff_res_dir (finfo->data + rva, finfo, type,
					 level + 1);
	}
      else
	{
	  if (rva >= (size_t) (finfo->data_end - finfo->data))
	    overrun (finfo, _("ID resource"));
	  re->subdir = 0;
	  re->u.res = read_coff_data_entry (finfo->data + rva, finfo, type);
	}

      *pp = re;
      pp = &re->next;
    }

  return rd;
}

/* Read a resource data entry.  */

static struct res_resource *
read_coff_data_entry (const bfd_byte *data, const struct coff_file_info *finfo, const struct res_id *type)
{
  const struct extern_res_data *erd;
  struct res_resource *r;
  unsigned long size, rva;
  const bfd_byte *resdata;

  if (type == NULL)
    fatal (_("resource type unknown"));

  if ((size_t) (finfo->data_end - data) < sizeof (struct extern_res_data))
    overrun (finfo, _("data entry"));

  erd = (const struct extern_res_data *) data;

  size = getfi_32 (finfo, erd->size);
  rva = getfi_32 (finfo, erd->rva);
  if (rva < finfo->secaddr
      || rva - finfo->secaddr >= (size_t) (finfo->data_end - finfo->data))
    overrun (finfo, _("resource data"));

  resdata = finfo->data + (rva - finfo->secaddr);

  if (size > (size_t) (finfo->data_end - resdata))
    overrun (finfo, _("resource data size"));

  r = bin_to_res (*type, resdata, size, finfo->big_endian);

  memset (&r->res_info, 0, sizeof (struct res_res_info));
  r->coff_info.codepage = getfi_32 (finfo, erd->codepage);
  r->coff_info.reserved = getfi_32 (finfo, erd->reserved);

  return r;
}

/* This structure is used to build a list of bindata structures.  */

struct bindata_build
{
  /* The data.  */
  struct bindata *d;
  /* The last structure we have added to the list.  */
  struct bindata *last;
  /* The size of the list as a whole.  */
  unsigned long length;
};

/* This structure keeps track of information as we build the directory
   tree.  */

struct coff_write_info
{
  /* These fields are based on the BFD.  */
  /* The BFD itself.  */
  bfd *abfd;
  /* Non-zero if the file is big endian.  */
  int big_endian;
  /* Pointer to section symbol used to build RVA relocs.  */
  asymbol **sympp;

  /* These fields are computed initially, and then not changed.  */
  /* Length of directory tables and entries.  */
  unsigned long dirsize;
  /* Length of directory entry strings.  */
  unsigned long dirstrsize;
  /* Length of resource data entries.  */
  unsigned long dataentsize;

  /* These fields are updated as we add data.  */
  /* Directory tables and entries.  */
  struct bindata_build dirs;
  /* Directory entry strings.  */
  struct bindata_build dirstrs;
  /* Resource data entries.  */
  struct bindata_build dataents;
  /* Actual resource data.  */
  struct bindata_build resources;
  /* Relocations.  */
  arelent **relocs;
  /* Number of relocations.  */
  unsigned int reloc_count;
};

/* Macros to swap out values.  */

#define putcwi_16(cwi, v, s) \
  ((cwi->big_endian) ? bfd_putb16 ((v), (s)) : bfd_putl16 ((v), (s)))
#define putcwi_32(cwi, v, s) \
  ((cwi->big_endian) ? bfd_putb32 ((v), (s)) : bfd_putl32 ((v), (s)))

static void coff_bin_sizes
  (const struct res_directory *, struct coff_write_info *);
static unsigned char *coff_alloc (struct bindata_build *, size_t);
static void coff_to_bin
  (const struct res_directory *, struct coff_write_info *);
static void coff_res_to_bin
  (const struct res_resource *, struct coff_write_info *);

/* Write resources to a COFF file.  RESOURCES should already be
   sorted.

   Right now we always create a new file.  Someday we should also
   offer the ability to merge resources into an existing file.  This
   would require doing the basic work of objcopy, just modifying or
   adding the .rsrc section.  */

void
write_coff_file (const char *filename, const char *target,
		 const struct res_directory *resources)
{
  bfd *abfd;
  asection *sec;
  struct coff_write_info cwi;
  struct bindata *d;
  unsigned long length, offset;

  if (filename == NULL)
    fatal (_("filename required for COFF output"));

  abfd = bfd_openw (filename, target);
  if (abfd == NULL)
    bfd_fatal (filename);

  if (! bfd_set_format (abfd, bfd_object))
    bfd_fatal ("bfd_set_format");

#if defined DLLTOOL_SH
  if (! bfd_set_arch_mach (abfd, bfd_arch_sh, 0))
    bfd_fatal ("bfd_set_arch_mach(sh)");
#elif defined DLLTOOL_MIPS
  if (! bfd_set_arch_mach (abfd, bfd_arch_mips, 0))
    bfd_fatal ("bfd_set_arch_mach(mips)");
#elif defined DLLTOOL_ARM
  if (! bfd_set_arch_mach (abfd, bfd_arch_arm, 0))
    bfd_fatal ("bfd_set_arch_mach(arm)");
#else
  /* FIXME: This is obviously i386 specific.  */
  if (! bfd_set_arch_mach (abfd, bfd_arch_i386, 0))
    bfd_fatal ("bfd_set_arch_mach(i386)");
#endif

  if (! bfd_set_file_flags (abfd, HAS_SYMS | HAS_RELOC))
    bfd_fatal ("bfd_set_file_flags");

  sec = bfd_make_section (abfd, ".rsrc");
  if (sec == NULL)
    bfd_fatal ("bfd_make_section");

  if (! bfd_set_section_flags (abfd, sec,
			       (SEC_HAS_CONTENTS | SEC_ALLOC
				| SEC_LOAD | SEC_DATA)))
    bfd_fatal ("bfd_set_section_flags");

  if (! bfd_set_symtab (abfd, sec->symbol_ptr_ptr, 1))
    bfd_fatal ("bfd_set_symtab");

  /* Requiring this is probably a bug in BFD.  */
  sec->output_section = sec;

  /* The order of data in the .rsrc section is
       resource directory tables and entries
       resource directory strings
       resource data entries
       actual resource data

     We build these different types of data in different lists.  */

  cwi.abfd = abfd;
  cwi.big_endian = bfd_big_endian (abfd);
  cwi.sympp = sec->symbol_ptr_ptr;
  cwi.dirsize = 0;
  cwi.dirstrsize = 0;
  cwi.dataentsize = 0;
  cwi.dirs.d = NULL;
  cwi.dirs.last = NULL;
  cwi.dirs.length = 0;
  cwi.dirstrs.d = NULL;
  cwi.dirstrs.last = NULL;
  cwi.dirstrs.length = 0;
  cwi.dataents.d = NULL;
  cwi.dataents.last = NULL;
  cwi.dataents.length = 0;
  cwi.resources.d = NULL;
  cwi.resources.last = NULL;
  cwi.resources.length = 0;
  cwi.relocs = NULL;
  cwi.reloc_count = 0;

  /* Work out the sizes of the resource directory entries, so that we
     know the various offsets we will need.  */
  coff_bin_sizes (resources, &cwi);

  /* Force the directory strings to be 32 bit aligned.  Every other
     structure is 32 bit aligned anyhow.  */
  cwi.dirstrsize = (cwi.dirstrsize + 3) &~ 3;

  /* Actually convert the resources to binary.  */
  coff_to_bin (resources, &cwi);

  /* Add another 2 bytes to the directory strings if needed for
     alignment.  */
  if ((cwi.dirstrs.length & 3) != 0)
    {
      unsigned char *ex;

      ex = coff_alloc (&cwi.dirstrs, 2);
      ex[0] = 0;
      ex[1] = 0;
    }

  /* Make sure that the data we built came out to the same size as we
     calculated initially.  */
  assert (cwi.dirs.length == cwi.dirsize);
  assert (cwi.dirstrs.length == cwi.dirstrsize);
  assert (cwi.dataents.length == cwi.dataentsize);

  length = (cwi.dirsize
	    + cwi.dirstrsize
	    + cwi.dataentsize
	    + cwi.resources.length);

  if (! bfd_set_section_size (abfd, sec, length))
    bfd_fatal ("bfd_set_section_size");

  bfd_set_reloc (abfd, sec, cwi.relocs, cwi.reloc_count);

  offset = 0;
  for (d = cwi.dirs.d; d != NULL; d = d->next)
    {
      if (! bfd_set_section_contents (abfd, sec, d->data, offset, d->length))
	bfd_fatal ("bfd_set_section_contents");
      offset += d->length;
    }
  for (d = cwi.dirstrs.d; d != NULL; d = d->next)
    {
      if (! bfd_set_section_contents (abfd, sec, d->data, offset, d->length))
	bfd_fatal ("bfd_set_section_contents");
      offset += d->length;
    }
  for (d = cwi.dataents.d; d != NULL; d = d->next)
    {
      if (! bfd_set_section_contents (abfd, sec, d->data, offset, d->length))
	bfd_fatal ("bfd_set_section_contents");
      offset += d->length;
    }
  for (d = cwi.resources.d; d != NULL; d = d->next)
    {
      if (! bfd_set_section_contents (abfd, sec, d->data, offset, d->length))
	bfd_fatal ("bfd_set_section_contents");
      offset += d->length;
    }

  assert (offset == length);

  if (! bfd_close (abfd))
    bfd_fatal ("bfd_close");

  /* We allocated the relocs array using malloc.  */
  free (cwi.relocs);
}

/* Work out the sizes of the various fixed size resource directory
   entries.  This updates fields in CWI.  */

static void
coff_bin_sizes (const struct res_directory *resdir,
		struct coff_write_info *cwi)
{
  const struct res_entry *re;

  cwi->dirsize += sizeof (struct extern_res_directory);

  for (re = resdir->entries; re != NULL; re = re->next)
    {
      cwi->dirsize += sizeof (struct extern_res_entry);

      if (re->id.named)
	cwi->dirstrsize += re->id.u.n.length * 2 + 2;

      if (re->subdir)
	coff_bin_sizes (re->u.dir, cwi);
      else
	cwi->dataentsize += sizeof (struct extern_res_data);
    }
}

/* Allocate data for a particular list.  */

static unsigned char *
coff_alloc (struct bindata_build *bb, size_t size)
{
  struct bindata *d;

  d = (struct bindata *) reswr_alloc (sizeof *d);

  d->next = NULL;
  d->data = (unsigned char *) reswr_alloc (size);
  d->length = size;

  if (bb->d == NULL)
    bb->d = d;
  else
    bb->last->next = d;
  bb->last = d;
  bb->length += size;

  return d->data;
}

/* Convert the resource directory RESDIR to binary.  */

static void
coff_to_bin (const struct res_directory *resdir, struct coff_write_info *cwi)
{
  struct extern_res_directory *erd;
  int ci, cn;
  const struct res_entry *e;
  struct extern_res_entry *ere;

  /* Write out the directory table.  */

  erd = ((struct extern_res_directory *)
	 coff_alloc (&cwi->dirs, sizeof (*erd)));

  putcwi_32 (cwi, resdir->characteristics, erd->characteristics);
  putcwi_32 (cwi, resdir->time, erd->time);
  putcwi_16 (cwi, resdir->major, erd->major);
  putcwi_16 (cwi, resdir->minor, erd->minor);

  ci = 0;
  cn = 0;
  for (e = resdir->entries; e != NULL; e = e->next)
    {
      if (e->id.named)
	++cn;
      else
	++ci;
    }

  putcwi_16 (cwi, cn, erd->name_count);
  putcwi_16 (cwi, ci, erd->id_count);

  /* Write out the data entries.  Note that we allocate space for all
     the entries before writing them out.  That permits a recursive
     call to work correctly when writing out subdirectories.  */

  ere = ((struct extern_res_entry *)
	 coff_alloc (&cwi->dirs, (ci + cn) * sizeof (*ere)));
  for (e = resdir->entries; e != NULL; e = e->next, ere++)
    {
      if (! e->id.named)
	putcwi_32 (cwi, e->id.u.id, ere->name);
      else
	{
	  unsigned char *str;
	  int i;

	  /* For some reason existing files seem to have the high bit
             set on the address of the name, although that is not
             documented.  */
	  putcwi_32 (cwi,
		     0x80000000 | (cwi->dirsize + cwi->dirstrs.length),
		     ere->name);

	  str = coff_alloc (&cwi->dirstrs, e->id.u.n.length * 2 + 2);
	  putcwi_16 (cwi, e->id.u.n.length, str);
	  for (i = 0; i < e->id.u.n.length; i++)
	    putcwi_16 (cwi, e->id.u.n.name[i], str + i * 2 + 2);
	}

      if (e->subdir)
	{
	  putcwi_32 (cwi, 0x80000000 | cwi->dirs.length, ere->rva);
	  coff_to_bin (e->u.dir, cwi);
	}
      else
	{
	  putcwi_32 (cwi,
		     cwi->dirsize + cwi->dirstrsize + cwi->dataents.length,
		     ere->rva);

	  coff_res_to_bin (e->u.res, cwi);
	}
    }
}

/* Convert the resource RES to binary.  */

static void
coff_res_to_bin (const struct res_resource *res, struct coff_write_info *cwi)
{
  arelent *r;
  struct extern_res_data *erd;
  struct bindata *d;
  unsigned long length;

  /* For some reason, although every other address is a section
     offset, the address of the resource data itself is an RVA.  That
     means that we need to generate a relocation for it.  We allocate
     the relocs array using malloc so that we can use realloc.  FIXME:
     This relocation handling is correct for the i386, but probably
     not for any other target.  */

  r = (arelent *) reswr_alloc (sizeof (arelent));
  r->sym_ptr_ptr = cwi->sympp;
  r->address = cwi->dirsize + cwi->dirstrsize + cwi->dataents.length;
  r->addend = 0;
  r->howto = bfd_reloc_type_lookup (cwi->abfd, BFD_RELOC_RVA);
  if (r->howto == NULL)
    bfd_fatal (_("can't get BFD_RELOC_RVA relocation type"));

  cwi->relocs = xrealloc (cwi->relocs,
			  (cwi->reloc_count + 2) * sizeof (arelent *));
  cwi->relocs[cwi->reloc_count] = r;
  cwi->relocs[cwi->reloc_count + 1] = NULL;
  ++cwi->reloc_count;

  erd = (struct extern_res_data *) coff_alloc (&cwi->dataents, sizeof (*erd));

  putcwi_32 (cwi,
	     (cwi->dirsize
	      + cwi->dirstrsize
	      + cwi->dataentsize
	      + cwi->resources.length),
	     erd->rva);
  putcwi_32 (cwi, res->coff_info.codepage, erd->codepage);
  putcwi_32 (cwi, res->coff_info.reserved, erd->reserved);

  d = res_to_bin (res, cwi->big_endian);

  if (cwi->resources.d == NULL)
    cwi->resources.d = d;
  else
    cwi->resources.last->next = d;

  length = 0;
  for (; d->next != NULL; d = d->next)
    length += d->length;
  length += d->length;
  cwi->resources.last = d;
  cwi->resources.length += length;

  putcwi_32 (cwi, length, erd->size);

  /* Force the next resource to have 32 bit alignment.  */

  if ((length & 3) != 0)
    {
      int add;
      unsigned char *ex;

      add = 4 - (length & 3);

      ex = coff_alloc (&cwi->resources, add);
      memset (ex, 0, add);
    }
}
