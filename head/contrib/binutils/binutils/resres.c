/* resres.c: read_res_file and write_res_file implementation for windres.
   Copyright 1998, 1999 Free Software Foundation, Inc.
   Written by Anders Norlander <anorland@hem2.passagen.se>.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* FIXME: This file does not work correctly in a cross configuration.
   It assumes that it can use fread and fwrite to read and write
   integers.  It does no swapping.  */

#include "bfd.h"
#include "bucomm.h"
#include "libiberty.h"
#include "windres.h"

#include <assert.h>
#include <time.h>

struct res_hdr
  {
    unsigned long data_size;
    unsigned long header_size;
  };

static void write_res_directory
  PARAMS ((const struct res_directory *,
	   const struct res_id *, const struct res_id *,
	   int *, int));
static void write_res_resource
  PARAMS ((const struct res_id *, const struct res_id *,
	   const struct res_resource *, int *));
static void write_res_bin
  PARAMS ((const struct res_resource *, const struct res_id *,
	   const struct res_id *, const struct res_res_info *));

static void write_res_id PARAMS ((const struct res_id *));
static void write_res_info PARAMS ((const struct res_res_info *));
static void write_res_data PARAMS ((const void *, size_t, int));
static void write_res_header
  PARAMS ((unsigned long, const struct res_id *, const struct res_id *,
	   const struct res_res_info *));

static int read_resource_entry PARAMS ((void));
static void read_res_data PARAMS ((void *, size_t, int));
static void read_res_id PARAMS ((struct res_id *));
static unichar *read_unistring PARAMS ((int *));
static void skip_null_resource PARAMS ((void));

static unsigned long get_id_size PARAMS ((const struct res_id *));
static void res_align_file PARAMS ((void));

static void
  res_add_resource
  PARAMS ((struct res_resource *, const struct res_id *,
	   const struct res_id *, int, int));

void
  res_append_resource
  PARAMS ((struct res_directory **, struct res_resource *,
	   int, const struct res_id *, int));

static struct res_directory *resources = NULL;

static FILE *fres;
static const char *filename;

extern char *program_name;

/* Read resource file */
struct res_directory *
read_res_file (fn)
     const char *fn;
{
  filename = fn;
  fres = fopen (filename, "rb");
  if (fres == NULL)
    fatal ("can't open `%s' for output: %s", filename, strerror (errno));

  skip_null_resource ();

  while (read_resource_entry ())
    ;

  fclose (fres);

  return resources;
}

/* Write resource file */
void
write_res_file (fn, resdir)
     const char *fn;
     const struct res_directory *resdir;
{
  int language;
  static const unsigned char sign[] =
  {0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
   0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  long fpos;

  filename = fn;

  fres = fopen (filename, "wb");
  if (fres == NULL)
    fatal ("can't open `%s' for output: %s", filename, strerror (errno));

  /* Write 32 bit resource signature */
  write_res_data (sign, sizeof (sign), 1);

  /* write resources */

  language = -1;
  write_res_directory (resdir, (const struct res_id *) NULL,
		       (const struct res_id *) NULL, &language, 1);

  /* end file on DWORD boundary */
  fpos = ftell (fres);
  if (fpos % 4)
    write_res_data (sign, fpos % 4, 1);

  fclose (fres);
}

/* Read a resource entry, returns 0 when all resources are read */
static int
read_resource_entry (void)
{
  struct res_id type;
  struct res_id name;
  struct res_res_info resinfo;
  struct res_hdr reshdr;
  long version;
  void *buff;

  struct res_resource *r;

  res_align_file ();

  /* Read header */
  if (fread (&reshdr, sizeof (reshdr), 1, fres) != 1)
    return 0;

  /* read resource type */
  read_res_id (&type);
  /* read resource id */
  read_res_id (&name);

  res_align_file ();

  /* Read additional resource header */
  read_res_data (&resinfo.version, sizeof (resinfo.version), 1);
  read_res_data (&resinfo.memflags, sizeof (resinfo.memflags), 1);
  read_res_data (&resinfo.language, sizeof (resinfo.language), 1);
  read_res_data (&version, sizeof (version), 1);
  read_res_data (&resinfo.characteristics, sizeof (resinfo.characteristics), 1);

  res_align_file ();

  /* Allocate buffer for data */
  buff = res_alloc (reshdr.data_size);
  /* Read data */
  read_res_data (buff, reshdr.data_size, 1);
  /* Convert binary data to resource */
  r = bin_to_res (type, buff, reshdr.data_size, 0);
  r->res_info = resinfo;
  /* Add resource to resource directory */
  res_add_resource (r, &type, &name, resinfo.language, 0);

  return 1;
}

/* write resource directory to binary resource file */
static void
write_res_directory (rd, type, name, language, level)
     const struct res_directory *rd;
     const struct res_id *type;
     const struct res_id *name;
     int *language;
     int level;
{
  const struct res_entry *re;

  for (re = rd->entries; re != NULL; re = re->next)
    {
      switch (level)
	{
	case 1:
	  /* If we're at level 1, the key of this resource is the
	     type.  This normally duplicates the information we have
	     stored with the resource itself, but we need to remember
	     the type if this is a user define resource type.  */
	  type = &re->id;
	  break;

	case 2:
	  /* If we're at level 2, the key of this resource is the name
	     we are going to use in the rc printout.  */
	  name = &re->id;
	  break;

	case 3:
	  /* If we're at level 3, then this key represents a language.
	     Use it to update the current language.  */
	  if (!re->id.named
	      && re->id.u.id != (unsigned long) *language
	      && (re->id.u.id & 0xffff) == re->id.u.id)
	    {
	      *language = re->id.u.id;
	    }
	  break;

	default:
	  break;
	}

      if (re->subdir)
	write_res_directory (re->u.dir, type, name, language, level + 1);
      else
	{
	  if (level == 3)
	    {
	      /* This is the normal case: the three levels are
	         TYPE/NAME/LANGUAGE.  NAME will have been set at level
	         2, and represents the name to use.  We probably just
	         set LANGUAGE, and it will probably match what the
	         resource itself records if anything.  */
	      write_res_resource (type, name, re->u.res, language);
	    }
	  else
	    {
	      fprintf (stderr, "// Resource at unexpected level %d\n", level);
	      write_res_resource (type, (struct res_id *) NULL, re->u.res,
				  language);
	    }
	}
    }

}

static void
write_res_resource (type, name, res, language)
     const struct res_id *type;
     const struct res_id *name;
     const struct res_resource *res;
     int *language ATTRIBUTE_UNUSED;
{
  int rt;

  switch (res->type)
    {
    default:
      abort ();

    case RES_TYPE_ACCELERATOR:
      rt = RT_ACCELERATOR;
      break;

    case RES_TYPE_BITMAP:
      rt = RT_BITMAP;
      break;

    case RES_TYPE_CURSOR:
      rt = RT_CURSOR;
      break;

    case RES_TYPE_GROUP_CURSOR:
      rt = RT_GROUP_CURSOR;
      break;

    case RES_TYPE_DIALOG:
      rt = RT_DIALOG;
      break;

    case RES_TYPE_FONT:
      rt = RT_FONT;
      break;

    case RES_TYPE_FONTDIR:
      rt = RT_FONTDIR;
      break;

    case RES_TYPE_ICON:
      rt = RT_ICON;
      break;

    case RES_TYPE_GROUP_ICON:
      rt = RT_GROUP_ICON;
      break;

    case RES_TYPE_MENU:
      rt = RT_MENU;
      break;

    case RES_TYPE_MESSAGETABLE:
      rt = RT_MESSAGETABLE;
      break;

    case RES_TYPE_RCDATA:
      rt = RT_RCDATA;
      break;

    case RES_TYPE_STRINGTABLE:
      rt = RT_STRING;
      break;

    case RES_TYPE_USERDATA:
      rt = 0;
      break;

    case RES_TYPE_VERSIONINFO:
      rt = RT_VERSION;
      break;
    }

  if (rt != 0
      && type != NULL
      && (type->named || type->u.id != (unsigned long) rt))
    {
      fprintf (stderr, "// Unexpected resource type mismatch: ");
      res_id_print (stderr, *type, 1);
      fprintf (stderr, " != %d", rt);
      abort ();
    }

  write_res_bin (res, type, name, &res->res_info);
  return;
}

/* Write a resource in binary resource format */
static void
write_res_bin (res, type, name, resinfo)
     const struct res_resource *res;
     const struct res_id *type;
     const struct res_id *name;
     const struct res_res_info *resinfo;
{
  unsigned long datasize = 0;
  const struct bindata *bin_rep, *data;

  bin_rep = res_to_bin (res, 0);
  for (data = bin_rep; data != NULL; data = data->next)
    datasize += data->length;

  write_res_header (datasize, type, name, resinfo);

  for (data = bin_rep; data != NULL; data = data->next)
    write_res_data (data->data, data->length, 1);
}

/* Get number of bytes needed to store an id in binary format */
static unsigned long
get_id_size (id)
     const struct res_id *id;
{
  if (id->named)
    return sizeof (unichar) * (id->u.n.length + 1);
  else
    return sizeof (unichar) * 2;
}

/* Write a resource header */
static void
write_res_header (datasize, type, name, resinfo)
     unsigned long datasize;
     const struct res_id *type;
     const struct res_id *name;
     const struct res_res_info *resinfo;
{
  struct res_hdr reshdr;
  reshdr.data_size = datasize;
  reshdr.header_size = 24 + get_id_size (type) + get_id_size (name);

  reshdr.header_size = (reshdr.header_size + 3) & ~3;

  res_align_file ();
  write_res_data (&reshdr, sizeof (reshdr), 1);
  write_res_id (type);
  write_res_id (name);

  res_align_file ();

  write_res_info (resinfo);
  res_align_file ();
}


/* Write data to file, abort on failure */
static void
write_res_data (data, size, count)
     const void *data;
     size_t size;
     int count;
{
  if (fwrite (data, size, count, fres) != (size_t) count)
    fatal ("%s: could not write to file", filename);
}

/* Read data from file, abort on failure */
static void
read_res_data (data, size, count)
     void *data;
     size_t size;
     int count;
{
  if (fread (data, size, count, fres) != (size_t) count)
    fatal ("%s: unexpected end of file", filename);
}

/* Write a resource id */
static void
write_res_id (id)
     const struct res_id *id;
{
  if (id->named)
    {
      unsigned long len = id->u.n.length;
      unichar null_term = 0;
      write_res_data (id->u.n.name, len * sizeof (unichar), 1);
      write_res_data (&null_term, sizeof (null_term), 1);
    }
  else
    {
      unsigned short i = 0xFFFF;
      write_res_data (&i, sizeof (i), 1);
      i = id->u.id;
      write_res_data (&i, sizeof (i), 1);
    }
}

/* Write resource info */
static void
write_res_info (info)
     const struct res_res_info *info;
{
  write_res_data (&info->version, sizeof (info->version), 1);
  write_res_data (&info->memflags, sizeof (info->memflags), 1);
  write_res_data (&info->language, sizeof (info->language), 1);
  write_res_data (&info->version, sizeof (info->version), 1);
  write_res_data (&info->characteristics, sizeof (info->characteristics), 1);
}

/* read a resource identifier */
void
read_res_id (id)
     struct res_id *id;
{
  unsigned short ord;
  unichar *id_s = NULL;
  int len;

  read_res_data (&ord, sizeof (ord), 1);
  if (ord == 0xFFFF)		/* an ordinal id */
    {
      read_res_data (&ord, sizeof (ord), 1);
      id->named = 0;
      id->u.id = ord;
    }
  else
    /* named id */
    {
      if (fseek (fres, -sizeof (ord), SEEK_CUR) != 0)
	fatal ("%s: %s: could not seek in file", program_name, filename);
      id_s = read_unistring (&len);
      id->named = 1;
      id->u.n.length = len;
      id->u.n.name = id_s;
    }
}

/* Read a null terminated UNICODE string */
static unichar *
read_unistring (len)
     int *len;
{
  unichar *s;
  unichar c;
  unichar *p;
  int l;

  *len = 0;
  l = 0;

  /* there are hardly any names longer than 256 characters */
  p = s = (unichar *) xmalloc (sizeof (unichar) * 256);
  do
    {
      read_res_data (&c, sizeof (c), 1);
      *p++ = c;
      if (c != 0)
	l++;
    }
  while (c != 0);
  *len = l;
  return s;
}

/* align file on DWORD boundary */
static void
res_align_file (void)
{
  int pos = ftell (fres);
  int skip = ((pos + 3) & ~3) - pos;
  if (fseek (fres, skip, SEEK_CUR) != 0)
    fatal ("%s: %s: unable to align file", program_name, filename);
}

/* Check if file is a win32 binary resource file, if so
   skip past the null resource. Returns 0 if successful, -1 on
   error.
 */
static void
skip_null_resource (void)
{
  struct res_hdr reshdr =
  {0, 0};
  read_res_data (&reshdr, sizeof (reshdr), 1);
  if ((reshdr.data_size != 0) || (reshdr.header_size != 0x20))
    goto skip_err;

  /* Subtract size of HeaderSize and DataSize */
  if (fseek (fres, reshdr.header_size - 8, SEEK_CUR) != 0)
    goto skip_err;

  return;

skip_err:
  fprintf (stderr, "%s: %s: Not a valid WIN32 resource file\n", program_name,
	   filename);
  xexit (1);
}

/* Add a resource to resource directory */
void
res_add_resource (r, type, id, language, dupok)
     struct res_resource *r;
     const struct res_id *type;
     const struct res_id *id;
     int language;
     int dupok;
{
  struct res_id a[3];

  a[0] = *type;
  a[1] = *id;
  a[2].named = 0;
  a[2].u.id = language;
  res_append_resource (&resources, r, 3, a, dupok);
}

/* Append a resource to resource directory.
   This is just copied from define_resource
   and modified to add an existing resource.
 */
void
res_append_resource (resources, resource, cids, ids, dupok)
     struct res_directory **resources;
     struct res_resource *resource;
     int cids;
     const struct res_id *ids;
     int dupok;
{
  struct res_entry *re = NULL;
  int i;

  assert (cids > 0);
  for (i = 0; i < cids; i++)
    {
      struct res_entry **pp;

      if (*resources == NULL)
	{
	  static unsigned long timeval;

	  /* Use the same timestamp for every resource created in a
	     single run.  */
	  if (timeval == 0)
	    timeval = time (NULL);

	  *resources = ((struct res_directory *)
			res_alloc (sizeof **resources));
	  (*resources)->characteristics = 0;
	  (*resources)->time = timeval;
	  (*resources)->major = 0;
	  (*resources)->minor = 0;
	  (*resources)->entries = NULL;
	}

      for (pp = &(*resources)->entries; *pp != NULL; pp = &(*pp)->next)
	if (res_id_cmp ((*pp)->id, ids[i]) == 0)
	  break;

      if (*pp != NULL)
	re = *pp;
      else
	{
	  re = (struct res_entry *) res_alloc (sizeof *re);
	  re->next = NULL;
	  re->id = ids[i];
	  if ((i + 1) < cids)
	    {
	      re->subdir = 1;
	      re->u.dir = NULL;
	    }
	  else
	    {
	      re->subdir = 0;
	      re->u.res = NULL;
	    }

	  *pp = re;
	}

      if ((i + 1) < cids)
	{
	  if (!re->subdir)
	    {
	      fprintf (stderr, "%s: ", program_name);
	      res_ids_print (stderr, i, ids);
	      fprintf (stderr, ": expected to be a directory\n");
	      xexit (1);
	    }

	  resources = &re->u.dir;
	}
    }

  if (re->subdir)
    {
      fprintf (stderr, "%s: ", program_name);
      res_ids_print (stderr, cids, ids);
      fprintf (stderr, ": expected to be a leaf\n");
      xexit (1);
    }

  if (re->u.res != NULL)
    {
      if (dupok)
	return;

      fprintf (stderr, "%s: warning: ", program_name);
      res_ids_print (stderr, cids, ids);
      fprintf (stderr, ": duplicate value\n");
    }

  re->u.res = resource;
}
