/* BFD back-end for s-record objects.
   Copyright 1990, 91, 92, 93, 94, 95, 96, 97, 98, 1999
   Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support <sac@cygnus.com>.

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

/*
SUBSECTION
	S-Record handling

DESCRIPTION
	
	Ordinary S-Records cannot hold anything but addresses and
	data, so that's all that we implement.

	The only interesting thing is that S-Records may come out of
	order and there is no header, so an initial scan is required
	to discover the minimum and maximum addresses used to create
	the vma and size of the only section we create.  We
	arbitrarily call this section ".text".

	When bfd_get_section_contents is called the file is read
	again, and this time the data is placed into a bfd_alloc'd
	area.

	Any number of sections may be created for output, we save them
	up and output them when it's time to close the bfd.

	An s record looks like:
	
EXAMPLE
	S<type><length><address><data><checksum>
	
DESCRIPTION
	Where
	o length
	is the number of bytes following upto the checksum. Note that
	this is not the number of chars following, since it takes two
	chars to represent a byte.
	o type
	is one of:
	0) header record
	1) two byte address data record
	2) three byte address data record
	3) four byte address data record
	7) four byte address termination record
	8) three byte address termination record
	9) two byte address termination record
	
	o address
	is the start address of the data following, or in the case of
	a termination record, the start address of the image
	o data
	is the data.
	o checksum
	is the sum of all the raw byte data in the record, from the length
	upwards, modulo 256 and subtracted from 255.


SUBSECTION
	Symbol S-Record handling

DESCRIPTION
	Some ICE equipment understands an addition to the standard
	S-Record format; symbols and their addresses can be sent
	before the data.

	The format of this is:
	($$ <modulename>
		(<space> <symbol> <address>)*)
	$$

	so a short symbol table could look like:

EXAMPLE
	$$ flash.x
	$$ flash.c
	  _port6 $0
	  _delay $4
	  _start $14
	  _etext $8036
	  _edata $8036
 	  _end $8036
	$$

DESCRIPTION
	We allow symbols to be anywhere in the data stream - the module names
	are always ignored.
		
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libiberty.h"
#include <ctype.h>

static void srec_get_symbol_info PARAMS ((bfd *, asymbol *, symbol_info *));
static void srec_print_symbol
 PARAMS ((bfd *, PTR, asymbol *, bfd_print_symbol_type));
static void srec_init PARAMS ((void));
static boolean srec_mkobject PARAMS ((bfd *));
static int srec_get_byte PARAMS ((bfd *, boolean *));
static void srec_bad_byte PARAMS ((bfd *, unsigned int, int, boolean));
static boolean srec_scan PARAMS ((bfd *));
static const bfd_target *srec_object_p PARAMS ((bfd *));
static const bfd_target *symbolsrec_object_p PARAMS ((bfd *));
static boolean srec_read_section PARAMS ((bfd *, asection *, bfd_byte *));

static boolean srec_write_record PARAMS ((bfd *, int, bfd_vma,
					  const bfd_byte *,
					  const bfd_byte *));
static boolean srec_write_header PARAMS ((bfd *));
static boolean srec_write_symbols PARAMS ((bfd *));
static boolean srec_new_symbol PARAMS ((bfd *, const char *, bfd_vma));
static boolean srec_get_section_contents
  PARAMS ((bfd *, asection *, PTR, file_ptr, bfd_size_type));
static boolean srec_set_arch_mach
  PARAMS ((bfd *, enum bfd_architecture, unsigned long));
static boolean srec_set_section_contents
  PARAMS ((bfd *, sec_ptr, PTR, file_ptr, bfd_size_type));
static boolean internal_srec_write_object_contents PARAMS ((bfd *, int));
static boolean srec_write_object_contents PARAMS ((bfd *));
static boolean symbolsrec_write_object_contents PARAMS ((bfd *));
static int srec_sizeof_headers PARAMS ((bfd *, boolean));
static asymbol *srec_make_empty_symbol PARAMS ((bfd *));
static long srec_get_symtab_upper_bound PARAMS ((bfd *));
static long srec_get_symtab PARAMS ((bfd *, asymbol **));

/* Macros for converting between hex and binary. */

static CONST char digs[] = "0123456789ABCDEF";

#define NIBBLE(x) hex_value(x)
#define HEX(buffer) ((NIBBLE((buffer)[0])<<4) + NIBBLE((buffer)[1]))
#define TOHEX(d, x, ch) \
	d[1] = digs[(x) & 0xf]; \
	d[0] = digs[((x)>>4)&0xf]; \
	ch += ((x) & 0xff);
#define	ISHEX(x)  hex_p(x)

/* Initialize by filling in the hex conversion array. */

static void
srec_init ()
{
  static boolean inited = false;

  if (inited == false)
    {
      inited = true;
      hex_init ();
    }
}

/* The maximum number of bytes on a line is FF */
#define MAXCHUNK 0xff
/* The number of bytes we fit onto a line on output */
#define CHUNK 16

/* When writing an S-record file, the S-records can not be output as
   they are seen.  This structure is used to hold them in memory.  */

struct srec_data_list_struct
{
  struct srec_data_list_struct *next;
  bfd_byte *data;
  bfd_vma where;
  bfd_size_type size;
};

typedef struct srec_data_list_struct srec_data_list_type;

/* When scanning the S-record file, a linked list of srec_symbol
   structures is built to represent the symbol table (if there is
   one).  */

struct srec_symbol
{
  struct srec_symbol *next;
  const char *name;
  bfd_vma val;
};

/* The S-record tdata information.  */

typedef struct srec_data_struct
  {
    srec_data_list_type *head;
    srec_data_list_type *tail;
    unsigned int type;
    struct srec_symbol *symbols;
    struct srec_symbol *symtail;
    asymbol *csymbols;
  }
tdata_type;

static boolean srec_write_section PARAMS ((bfd *, tdata_type *,
					   srec_data_list_type *));
static boolean srec_write_terminator PARAMS ((bfd *, tdata_type *));

/* Set up the S-record tdata information.  */

static boolean
srec_mkobject (abfd)
     bfd *abfd;
{
  srec_init ();

  if (abfd->tdata.srec_data == NULL)
    {
      tdata_type *tdata = (tdata_type *) bfd_alloc (abfd, sizeof (tdata_type));
      if (tdata == NULL)
	return false;
      abfd->tdata.srec_data = tdata;
      tdata->type = 1;
      tdata->head = NULL;
      tdata->tail = NULL;
      tdata->symbols = NULL;
      tdata->symtail = NULL;
      tdata->csymbols = NULL;
    }

  return true;
}

/* Read a byte from an S record file.  Set *ERRORPTR if an error
   occurred.  Return EOF on error or end of file.  */

static int
srec_get_byte (abfd, errorptr)
     bfd *abfd;
     boolean *errorptr;
{
  bfd_byte c;

  if (bfd_read (&c, 1, 1, abfd) != 1)
    {
      if (bfd_get_error () != bfd_error_file_truncated)
	*errorptr = true;
      return EOF;
    }

  return (int) (c & 0xff);
}

/* Report a problem in an S record file.  FIXME: This probably should
   not call fprintf, but we really do need some mechanism for printing
   error messages.  */

static void
srec_bad_byte (abfd, lineno, c, error)
     bfd *abfd;
     unsigned int lineno;
     int c;
     boolean error;
{
  if (c == EOF)
    {
      if (! error)
	bfd_set_error (bfd_error_file_truncated);
    }
  else
    {
      char buf[10];

      if (! isprint (c))
	sprintf (buf, "\\%03o", (unsigned int) c);
      else
	{
	  buf[0] = c;
	  buf[1] = '\0';
	}
      (*_bfd_error_handler)
	(_("%s:%d: Unexpected character `%s' in S-record file\n"),
	 bfd_get_filename (abfd), lineno, buf);
      bfd_set_error (bfd_error_bad_value);
    }
}

/* Add a new symbol found in an S-record file.  */

static boolean
srec_new_symbol (abfd, name, val)
     bfd *abfd;
     const char *name;
     bfd_vma val;
{
  struct srec_symbol *n;

  n = (struct srec_symbol *) bfd_alloc (abfd, sizeof (struct srec_symbol));
  if (n == NULL)
    return false;

  n->name = name;
  n->val = val;

  if (abfd->tdata.srec_data->symbols == NULL)
    abfd->tdata.srec_data->symbols = n;
  else
    abfd->tdata.srec_data->symtail->next = n;
  abfd->tdata.srec_data->symtail = n;
  n->next = NULL;

  ++abfd->symcount;

  return true;
}

/* Read the S record file and turn it into sections.  We create a new
   section for each contiguous set of bytes.  */

static boolean
srec_scan (abfd)
     bfd *abfd;
{
  int c;
  unsigned int lineno = 1;
  boolean error = false;
  bfd_byte *buf = NULL;
  size_t bufsize = 0;
  asection *sec = NULL;
  char *symbuf = NULL;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
    goto error_return;

  while ((c = srec_get_byte (abfd, &error)) != EOF)
    {
      /* We only build sections from contiguous S-records, so if this
         is not an S-record, then stop building a section.  */
      if (c != 'S' && c != '\r' && c != '\n')
	sec = NULL;

      switch (c)
	{
	default:
	  srec_bad_byte (abfd, lineno, c, error);
	  goto error_return;

	case '\n':
	  ++lineno;
	  break;

	case '\r':
	  break;

	case '$':
	  /* Starting a module name, which we ignore.  */
	  while ((c = srec_get_byte (abfd, &error)) != '\n'
		 && c != EOF)
	    ;
	  if (c == EOF)
	    {
	      srec_bad_byte (abfd, lineno, c, error);
	      goto error_return;
	    }

	  ++lineno;

	  break;

	case ' ':
	  do
	    {
	      unsigned int alc;
	      char *p, *symname;
	      bfd_vma symval;

	      /* Starting a symbol definition.  */
	      while ((c = srec_get_byte (abfd, &error)) != EOF
		     && (c == ' ' || c == '\t'))
		;

	      if (c == '\n' || c == '\r')
		break;

	      if (c == EOF)
		{
		  srec_bad_byte (abfd, lineno, c, error);
		  goto error_return;
		}

	      alc = 10;
	      symbuf = (char *) bfd_malloc (alc + 1);
	      if (symbuf == NULL)
		goto error_return;

	      p = symbuf;

	      *p++ = c;
	      while ((c = srec_get_byte (abfd, &error)) != EOF
		     && ! isspace (c))
		{
		  if ((unsigned int) (p - symbuf) >= alc)
		    {
		      char *n;

		      alc *= 2;
		      n = (char *) bfd_realloc (symbuf, alc + 1);
		      if (n == NULL)
			goto error_return;
		      p = n + (p - symbuf);
		      symbuf = n;
		    }

		  *p++ = c;
		}

	      if (c == EOF)
		{
		  srec_bad_byte (abfd, lineno, c, error);
		  goto error_return;
		}

	      *p++ = '\0';
	      symname = bfd_alloc (abfd, p - symbuf);
	      if (symname == NULL)
		goto error_return;
	      strcpy (symname, symbuf);
	      free (symbuf);
	      symbuf = NULL;

	      while ((c = srec_get_byte (abfd, &error)) != EOF
		     && (c == ' ' || c == '\t'))
		;
	      if (c == EOF)
		{
		  srec_bad_byte (abfd, lineno, c, error);
		  goto error_return;
		}

	      /* Skip a dollar sign before the hex value.  */
	      if (c == '$')
		{
		  c = srec_get_byte (abfd, &error);
		  if (c == EOF)
		    {
		      srec_bad_byte (abfd, lineno, c, error);
		      goto error_return;
		    }
		}

	      symval = 0;
	      while (ISHEX (c))
		{
		  symval <<= 4;
		  symval += NIBBLE (c);
		  c = srec_get_byte (abfd, &error);
		}

	      if (! srec_new_symbol (abfd, symname, symval))
		goto error_return;
	    }
	  while (c == ' ' || c == '\t');

	  if (c == '\n')
	    ++lineno;
	  else if (c != '\r')
	    {
	      srec_bad_byte (abfd, lineno, c, error);
	      goto error_return;
	    }

	  break;
    
	case 'S':
	  {
	    file_ptr pos;
	    char hdr[3];
	    unsigned int bytes;
	    bfd_vma address;
	    bfd_byte *data;

	    /* Starting an S-record.  */

	    pos = bfd_tell (abfd) - 1;

	    if (bfd_read (hdr, 1, 3, abfd) != 3)
	      goto error_return;

	    if (! ISHEX (hdr[1]) || ! ISHEX (hdr[2]))
	      {
		if (! ISHEX (hdr[1]))
		  c = hdr[1];
		else
		  c = hdr[2];
		srec_bad_byte (abfd, lineno, c, error);
		goto error_return;
	      }

	    bytes = HEX (hdr + 1);
	    if (bytes * 2 > bufsize)
	      {
		if (buf != NULL)
		  free (buf);
		buf = (bfd_byte *) bfd_malloc (bytes * 2);
		if (buf == NULL)
		  goto error_return;
		bufsize = bytes * 2;
	      }

	    if (bfd_read (buf, 1, bytes * 2, abfd) != bytes * 2)
	      goto error_return;

	    /* Ignore the checksum byte.  */
	    --bytes;

	    address = 0;
	    data = buf;
	    switch (hdr[0])
	      {
	      case '0':
	      case '5':
		/* Prologue--ignore the file name, but stop building a
                   section at this point.  */
		sec = NULL;
		break;

	      case '3':
		address = HEX (data);
		data += 2;
		--bytes;
		/* Fall through.  */
	      case '2':
		address = (address << 8) | HEX (data);
		data += 2;
		--bytes;
		/* Fall through.  */
	      case '1':
		address = (address << 8) | HEX (data);
		data += 2;
		address = (address << 8) | HEX (data);
		data += 2;
		bytes -= 2;

		if (sec != NULL
		    && sec->vma + sec->_raw_size == address)
		  {
		    /* This data goes at the end of the section we are
                       currently building.  */
		    sec->_raw_size += bytes;
		  }
		else
		  {
		    char secbuf[20];
		    char *secname;

		    sprintf (secbuf, ".sec%d", bfd_count_sections (abfd) + 1);
		    secname = (char *) bfd_alloc (abfd, strlen (secbuf) + 1);
		    strcpy (secname, secbuf);
		    sec = bfd_make_section (abfd, secname);
		    if (sec == NULL)
		      goto error_return;
		    sec->flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC;
		    sec->vma = address;
		    sec->lma = address;
		    sec->_raw_size = bytes;
		    sec->filepos = pos;
		  }

		break;

	      case '7':
		address = HEX (data);
		data += 2;
		/* Fall through.  */
	      case '8':
		address = (address << 8) | HEX (data);
		data += 2;
		/* Fall through.  */
	      case '9':
		address = (address << 8) | HEX (data);
		data += 2;
		address = (address << 8) | HEX (data);
		data += 2;

		/* This is a termination record.  */
		abfd->start_address = address;

		if (buf != NULL)
		  free (buf);

		return true;
	      }
	  }
	  break;
	}
    }

  if (error)
    goto error_return;

  if (buf != NULL)
    free (buf);

  return true;

 error_return:
  if (symbuf != NULL)
    free (symbuf);
  if (buf != NULL)
    free (buf);
  return false;
}

/* Check whether an existing file is an S-record file.  */

static const bfd_target *
srec_object_p (abfd)
     bfd *abfd;
{
  bfd_byte b[4];

  srec_init ();

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || bfd_read (b, 1, 4, abfd) != 4)
    return NULL;

  if (b[0] != 'S' || !ISHEX (b[1]) || !ISHEX (b[2]) || !ISHEX (b[3]))
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (! srec_mkobject (abfd)
      || ! srec_scan (abfd))
    return NULL;

  if (abfd->symcount > 0)
    abfd->flags |= HAS_SYMS;

  return abfd->xvec;
}

/* Check whether an existing file is an S-record file with symbols.  */

static const bfd_target *
symbolsrec_object_p (abfd)
     bfd *abfd;
{
  char b[2];

  srec_init ();

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0
      || bfd_read (b, 1, 2, abfd) != 2)
    return NULL;

  if (b[0] != '$' || b[1] != '$')
    {
      bfd_set_error (bfd_error_wrong_format);
      return NULL;
    }

  if (! srec_mkobject (abfd)
      || ! srec_scan (abfd))
    return NULL;

  if (abfd->symcount > 0)
    abfd->flags |= HAS_SYMS;

  return abfd->xvec;
}

/* Read in the contents of a section in an S-record file.  */

static boolean
srec_read_section (abfd, section, contents)
     bfd *abfd;
     asection *section;
     bfd_byte *contents;
{
  int c;
  bfd_size_type sofar = 0;
  boolean error = false;
  bfd_byte *buf = NULL;
  size_t bufsize = 0;

  if (bfd_seek (abfd, section->filepos, SEEK_SET) != 0)
    goto error_return;

  while ((c = srec_get_byte (abfd, &error)) != EOF)
    {
      bfd_byte hdr[3];
      unsigned int bytes;
      bfd_vma address;
      bfd_byte *data;

      if (c == '\r' || c == '\n')
	continue;

      /* This is called after srec_scan has already been called, so we
         ought to know the exact format.  */
      BFD_ASSERT (c == 'S');

      if (bfd_read (hdr, 1, 3, abfd) != 3)
	goto error_return;

      BFD_ASSERT (ISHEX (hdr[1]) && ISHEX (hdr[2]));

      bytes = HEX (hdr + 1);

      if (bytes * 2 > bufsize)
	{
	  if (buf != NULL)
	    free (buf);
	  buf = (bfd_byte *) bfd_malloc (bytes * 2);
	  if (buf == NULL)
	    goto error_return;
	  bufsize = bytes * 2;
	}

      if (bfd_read (buf, 1, bytes * 2, abfd) != bytes * 2)
	goto error_return;

      address = 0;
      data = buf;
      switch (hdr[0])
	{
	default:
	  BFD_ASSERT (sofar == section->_raw_size);
	  if (buf != NULL)
	    free (buf);
	  return true;

	case '3':
	  address = HEX (data);
	  data += 2;
	  --bytes;
	  /* Fall through.  */
	case '2':
	  address = (address << 8) | HEX (data);
	  data += 2;
	  --bytes;
	  /* Fall through.  */
	case '1':
	  address = (address << 8) | HEX (data);
	  data += 2;
	  address = (address << 8) | HEX (data);
	  data += 2;
	  bytes -= 2;

	  if (address != section->vma + sofar)
	    {
	      /* We've come to the end of this section.  */
	      BFD_ASSERT (sofar == section->_raw_size);
	      if (buf != NULL)
		free (buf);
	      return true;
	    }

	  /* Don't consider checksum.  */
	  --bytes;

	  while (bytes-- != 0)
	    {
	      contents[sofar] = HEX (data);
	      data += 2;
	      ++sofar;
	    }

	  break;
	}
    }

  if (error)
    goto error_return;

  BFD_ASSERT (sofar == section->_raw_size);

  if (buf != NULL)
    free (buf);

  return true;

 error_return:
  if (buf != NULL)
    free (buf);
  return false;
}

/* Get the contents of a section in an S-record file.  */

static boolean
srec_get_section_contents (abfd, section, location, offset, count)
     bfd *abfd;
     asection *section;
     PTR location;
     file_ptr offset;
     bfd_size_type count;
{
  if (section->used_by_bfd == NULL)
    {
      section->used_by_bfd = bfd_alloc (abfd, section->_raw_size);
      if (section->used_by_bfd == NULL
	  && section->_raw_size != 0)
	return false;

      if (! srec_read_section (abfd, section, section->used_by_bfd))
	return false;
    }

  memcpy (location, (bfd_byte *) section->used_by_bfd + offset,
	  (size_t) count);

  return true;
}

/* Set the architecture.  We accept an unknown architecture here.  */

static boolean
srec_set_arch_mach (abfd, arch, mach)
     bfd *abfd;
     enum bfd_architecture arch;
     unsigned long mach;
{
  if (arch == bfd_arch_unknown)
    {
      abfd->arch_info = &bfd_default_arch_struct;
      return true;
    }
  return bfd_default_set_arch_mach (abfd, arch, mach);
}

/* we have to save up all the Srecords for a splurge before output */

static boolean
srec_set_section_contents (abfd, section, location, offset, bytes_to_do)
     bfd *abfd;
     sec_ptr section;
     PTR location;
     file_ptr offset;
     bfd_size_type bytes_to_do;
{
  tdata_type *tdata = abfd->tdata.srec_data;
  register srec_data_list_type *entry;

  entry = ((srec_data_list_type *)
	   bfd_alloc (abfd, sizeof (srec_data_list_type)));
  if (entry == NULL)
    return false;

  if (bytes_to_do
      && (section->flags & SEC_ALLOC)
      && (section->flags & SEC_LOAD))
    {
      bfd_byte *data = (bfd_byte *) bfd_alloc (abfd, bytes_to_do);
      if (data == NULL)
	return false;
      memcpy ((PTR) data, location, (size_t) bytes_to_do);

      if ((section->lma + offset + bytes_to_do - 1) <= 0xffff)
	{

	}
      else if ((section->lma + offset + bytes_to_do - 1) <= 0xffffff
	       && tdata->type <= 2)
	{
	  tdata->type = 2;
	}
      else
	{
	  tdata->type = 3;
	}

      entry->data = data;
      entry->where = section->lma + offset;
      entry->size = bytes_to_do;

      /* Sort the records by address.  Optimize for the common case of
         adding a record to the end of the list.  */
      if (tdata->tail != NULL
	  && entry->where >= tdata->tail->where)
	{
	  tdata->tail->next = entry;
	  entry->next = NULL;
	  tdata->tail = entry;
	}
      else
	{
	  register srec_data_list_type **look;

	  for (look = &tdata->head;
	       *look != NULL && (*look)->where < entry->where;
	       look = &(*look)->next)
	    ;
	  entry->next = *look;
	  *look = entry;
	  if (entry->next == NULL)
	    tdata->tail = entry;
	}
    }
  return true;
}

/* Write a record of type, of the supplied number of bytes. The
   supplied bytes and length don't have a checksum. That's worked out
   here
*/
static boolean
srec_write_record (abfd, type, address, data, end)
     bfd *abfd;
     int type;
     bfd_vma address;
     const bfd_byte *data;
     const bfd_byte *end;
{
  char buffer[MAXCHUNK];
  unsigned int check_sum = 0;
  CONST bfd_byte *src = data;
  char *dst = buffer;
  char *length;
  bfd_size_type wrlen;

  *dst++ = 'S';
  *dst++ = '0' + type;

  length = dst;
  dst += 2;			/* leave room for dst*/

  switch (type)
    {
    case 3:
    case 7:
      TOHEX (dst, (address >> 24), check_sum);
      dst += 2;
    case 8:
    case 2:
      TOHEX (dst, (address >> 16), check_sum);
      dst += 2;
    case 9:
    case 1:
    case 0:
      TOHEX (dst, (address >> 8), check_sum);
      dst += 2;
      TOHEX (dst, (address), check_sum);
      dst += 2;
      break;

    }
  for (src = data; src < end; src++)
    {
      TOHEX (dst, *src, check_sum);
      dst += 2;
    }

  /* Fill in the length */
  TOHEX (length, (dst - length) / 2, check_sum);
  check_sum &= 0xff;
  check_sum = 255 - check_sum;
  TOHEX (dst, check_sum, check_sum);
  dst += 2;

  *dst++ = '\r';
  *dst++ = '\n';
  wrlen = dst - buffer;
  if (bfd_write ((PTR) buffer, 1, wrlen, abfd) != wrlen)
    return false;
  return true;
}



static boolean
srec_write_header (abfd)
     bfd *abfd;
{
  bfd_byte buffer[MAXCHUNK];
  bfd_byte *dst = buffer;
  unsigned int i;

  /* I'll put an arbitary 40 char limit on header size */
  for (i = 0; i < 40 && abfd->filename[i]; i++)
    {
      *dst++ = abfd->filename[i];
    }
  return srec_write_record (abfd, 0, 0, buffer, dst);
}

static boolean
srec_write_section (abfd, tdata, list)
     bfd *abfd;
     tdata_type *tdata;
     srec_data_list_type *list;
{
  unsigned int octets_written = 0;
  bfd_byte *location = list->data;

  while (octets_written < list->size)
    {
      bfd_vma address;
      unsigned int octets_this_chunk = list->size - octets_written;

      if (octets_this_chunk > CHUNK)
	octets_this_chunk = CHUNK;

      address = list->where + octets_written / bfd_octets_per_byte (abfd);

      if (! srec_write_record (abfd,
			       tdata->type,
			       address,
			       location,
			       location + octets_this_chunk))
	return false;

      octets_written += octets_this_chunk;
      location += octets_this_chunk;
    }

  return true;
}

static boolean
srec_write_terminator (abfd, tdata)
     bfd *abfd;
     tdata_type *tdata;
{
  bfd_byte buffer[2];

  return srec_write_record (abfd, 10 - tdata->type,
			    abfd->start_address, buffer, buffer);
}



static boolean
srec_write_symbols (abfd)
     bfd *abfd;
{
  char buffer[MAXCHUNK];
  /* Dump out the symbols of a bfd */
  int i;
  int count = bfd_get_symcount (abfd);

  if (count)
    {
      size_t len;
      asymbol **table = bfd_get_outsymbols (abfd);
      sprintf (buffer, "$$ %s\r\n", abfd->filename);

      len = strlen (buffer);
      if (bfd_write (buffer, len, 1, abfd) != len)
	return false;

      for (i = 0; i < count; i++)
	{
	  asymbol *s = table[i];
	  if (! bfd_is_local_label (abfd, s)
	      && (s->flags & BSF_DEBUGGING) == 0)
	    {
	      /* Just dump out non debug symbols */
	      bfd_size_type l;
	      char buf2[40], *p;

	      sprintf_vma (buf2,
			   s->value + s->section->output_section->lma
			   + s->section->output_offset);
	      p = buf2;
	      while (p[0] == '0' && p[1] != 0)
		p++;
	      sprintf (buffer, "  %s $%s\r\n", s->name, p);
	      l = strlen (buffer);
	      if (bfd_write (buffer, l, 1, abfd) != l)
		return false;
	    }
	}
      sprintf (buffer, "$$ \r\n");
      len = strlen (buffer);
      if (bfd_write (buffer, len, 1, abfd) != len)
	return false;
    }

  return true;
}

static boolean
internal_srec_write_object_contents (abfd, symbols)
     bfd *abfd;
     int symbols;
{
  tdata_type *tdata = abfd->tdata.srec_data;
  srec_data_list_type *list;

  if (symbols)
    {
      if (! srec_write_symbols (abfd))
	return false;
    }

  if (! srec_write_header (abfd))
    return false;

  /* Now wander though all the sections provided and output them */
  list = tdata->head;

  while (list != (srec_data_list_type *) NULL)
    {
      if (! srec_write_section (abfd, tdata, list))
	return false;
      list = list->next;
    }
  return srec_write_terminator (abfd, tdata);
}

static boolean
srec_write_object_contents (abfd)
     bfd *abfd;
{
  return internal_srec_write_object_contents (abfd, 0);
}

static boolean
symbolsrec_write_object_contents (abfd)
     bfd *abfd;
{
  return internal_srec_write_object_contents (abfd, 1);
}

/*ARGSUSED*/
static int
srec_sizeof_headers (abfd, exec)
     bfd *abfd ATTRIBUTE_UNUSED;
     boolean exec ATTRIBUTE_UNUSED;
{
  return 0;
}

static asymbol *
srec_make_empty_symbol (abfd)
     bfd *abfd;
{
  asymbol *new = (asymbol *) bfd_zalloc (abfd, sizeof (asymbol));
  if (new)
    new->the_bfd = abfd;
  return new;
}

/* Return the amount of memory needed to read the symbol table.  */

static long
srec_get_symtab_upper_bound (abfd)
     bfd *abfd;
{
  return (bfd_get_symcount (abfd) + 1) * sizeof (asymbol *);
}

/* Return the symbol table.  */

static long
srec_get_symtab (abfd, alocation)
     bfd *abfd;
     asymbol **alocation;
{
  unsigned int symcount = bfd_get_symcount (abfd);
  asymbol *csymbols;
  unsigned int i;

  csymbols = abfd->tdata.srec_data->csymbols;
  if (csymbols == NULL)
    {
      asymbol *c;
      struct srec_symbol *s;

      csymbols = (asymbol *) bfd_alloc (abfd, symcount * sizeof (asymbol));
      if (csymbols == NULL && symcount != 0)
	return false;
      abfd->tdata.srec_data->csymbols = csymbols;

      for (s = abfd->tdata.srec_data->symbols, c = csymbols;
	   s != NULL;
	   s = s->next, ++c)
	{
	  c->the_bfd = abfd;
	  c->name = s->name;
	  c->value = s->val;
	  c->flags = BSF_GLOBAL;
	  c->section = bfd_abs_section_ptr;
	  c->udata.p = NULL;
	}
    }
	
  for (i = 0; i < symcount; i++)
    *alocation++ = csymbols++;
  *alocation = NULL;

  return symcount;
}

/*ARGSUSED*/
static void
srec_get_symbol_info (ignore_abfd, symbol, ret)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     asymbol *symbol;
     symbol_info *ret;
{
  bfd_symbol_info (symbol, ret);
}

/*ARGSUSED*/
static void
srec_print_symbol (ignore_abfd, afile, symbol, how)
     bfd *ignore_abfd ATTRIBUTE_UNUSED;
     PTR afile;
     asymbol *symbol;
     bfd_print_symbol_type how;
{
  FILE *file = (FILE *) afile;
  switch (how)
    {
    case bfd_print_symbol_name:
      fprintf (file, "%s", symbol->name);
      break;
    default:
      bfd_print_symbol_vandf ((PTR) file, symbol);
      fprintf (file, " %-5s %s",
	       symbol->section->name,
	       symbol->name);

    }
}

#define	srec_close_and_cleanup _bfd_generic_close_and_cleanup
#define srec_bfd_free_cached_info _bfd_generic_bfd_free_cached_info
#define srec_new_section_hook _bfd_generic_new_section_hook

#define srec_bfd_is_local_label_name bfd_generic_is_local_label_name
#define srec_get_lineno _bfd_nosymbols_get_lineno
#define srec_find_nearest_line _bfd_nosymbols_find_nearest_line
#define srec_bfd_make_debug_symbol _bfd_nosymbols_bfd_make_debug_symbol
#define srec_read_minisymbols _bfd_generic_read_minisymbols
#define srec_minisymbol_to_symbol _bfd_generic_minisymbol_to_symbol

#define srec_get_reloc_upper_bound \
  ((long (*) PARAMS ((bfd *, asection *))) bfd_0l)
#define srec_canonicalize_reloc \
  ((long (*) PARAMS ((bfd *, asection *, arelent **, asymbol **))) bfd_0l)
#define srec_bfd_reloc_type_lookup _bfd_norelocs_bfd_reloc_type_lookup

#define srec_get_section_contents_in_window \
  _bfd_generic_get_section_contents_in_window

#define srec_bfd_get_relocated_section_contents \
  bfd_generic_get_relocated_section_contents
#define srec_bfd_relax_section bfd_generic_relax_section
#define srec_bfd_gc_sections bfd_generic_gc_sections
#define srec_bfd_link_hash_table_create _bfd_generic_link_hash_table_create
#define srec_bfd_link_add_symbols _bfd_generic_link_add_symbols
#define srec_bfd_final_link _bfd_generic_final_link
#define srec_bfd_link_split_section _bfd_generic_link_split_section

const bfd_target srec_vec =
{
  "srec",			/* name */
  bfd_target_srec_flavour,
  BFD_ENDIAN_UNKNOWN,		/* target byte order */
  BFD_ENDIAN_UNKNOWN,		/* target headers byte order */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {
    _bfd_dummy_target,
    srec_object_p,		/* bfd_check_format */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    srec_mkobject,
    _bfd_generic_mkarchive,
    bfd_false,
  },
  {				/* bfd_write_contents */
    bfd_false,
    srec_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (srec),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (srec),
  BFD_JUMP_TABLE_RELOCS (srec),
  BFD_JUMP_TABLE_WRITE (srec),
  BFD_JUMP_TABLE_LINK (srec),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,
  
  (PTR) 0
};



const bfd_target symbolsrec_vec =
{
  "symbolsrec",			/* name */
  bfd_target_srec_flavour,
  BFD_ENDIAN_UNKNOWN,		/* target byte order */
  BFD_ENDIAN_UNKNOWN,		/* target headers byte order */
  (HAS_RELOC | EXEC_P |		/* object flags */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* section flags */
  0,				/* leading underscore */
  ' ',				/* ar_pad_char */
  16,				/* ar_max_namelen */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* data */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* hdrs */

  {
    _bfd_dummy_target,
    symbolsrec_object_p,	/* bfd_check_format */
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    srec_mkobject,
    _bfd_generic_mkarchive,
    bfd_false,
  },
  {				/* bfd_write_contents */
    bfd_false,
    symbolsrec_write_object_contents,
    _bfd_write_archive_contents,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (srec),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (srec),
  BFD_JUMP_TABLE_RELOCS (srec),
  BFD_JUMP_TABLE_WRITE (srec),
  BFD_JUMP_TABLE_LINK (srec),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,
  
  (PTR) 0
};
