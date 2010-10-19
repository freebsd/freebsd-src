/* vms-misc.c -- Miscellaneous functions for VAX (openVMS/VAX) and
   EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
   Free Software Foundation, Inc.

   Written by Klaus K"ampf (kkaempf@rmi.de)

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

#if __STDC__
#include <stdarg.h>
#endif

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"

#include "vms.h"

#if VMS_DEBUG
/* Debug functions.  */

/* Debug function for all vms extensions
   evaluates environment variable VMS_DEBUG for a
   numerical value on the first call
   all error levels below this value are printed

   levels:
   1	toplevel bfd calls (functions from the bfd vector)
   2	functions called by bfd calls
   ...
   9	almost everything

   level is also indentation level. Indentation is performed
   if level > 0.  */

void
_bfd_vms_debug (int level, char *format, ...)
{
  static int min_level = -1;
  static FILE *output = NULL;
  char *eptr;
  va_list args;
  int abslvl = (level > 0) ? level : - level;

  if (min_level == -1)
    {
      if ((eptr = getenv ("VMS_DEBUG")) != NULL)
	{
	  min_level = atoi (eptr);
	  output = stderr;
	}
      else
	min_level = 0;
    }
  if (output == NULL)
    return;
  if (abslvl > min_level)
    return;

  while (--level>0)
    fprintf (output, " ");
  va_start (args, format);
  vfprintf (output, format, args);
  fflush (output);
  va_end (args);
}

/* A debug function
   hex dump 'size' bytes starting at 'ptr'.  */

void
_bfd_hexdump (int level,
	      unsigned char *ptr,
	      int size,
	      int offset)
{
  unsigned char *lptr = ptr;
  int count = 0;
  long start = offset;

  while (size-- > 0)
    {
      if ((count%16) == 0)
	vms_debug (level, "%08lx:", start);
      vms_debug (-level, " %02x", *ptr++);
      count++;
      start++;
      if (size == 0)
	{
	  while ((count%16) != 0)
	    {
	      vms_debug (-level, "   ");
	      count++;
	    }
	}
      if ((count%16) == 0)
	{
	  vms_debug (-level, " ");
	  while (lptr < ptr)
	    {
	      vms_debug (-level, "%c", (*lptr < 32)?'.':*lptr);
	      lptr++;
	    }
	  vms_debug (-level, "\n");
	}
    }
  if ((count%16) != 0)
    vms_debug (-level, "\n");
}
#endif

/* Hash functions

   These are needed when reading an object file.  */

/* Allocate new vms_hash_entry
   keep the symbol name and a pointer to the bfd symbol in the table.  */

struct bfd_hash_entry *
_bfd_vms_hash_newfunc (struct bfd_hash_entry *entry,
		       struct bfd_hash_table *table,
		       const char *string)
{
  vms_symbol_entry *ret;

#if VMS_DEBUG
  vms_debug (5, "_bfd_vms_hash_newfunc (%p, %p, %s)\n", entry, table, string);
#endif

  if (entry == NULL)
    {
      ret = (vms_symbol_entry *)
	      bfd_hash_allocate (table, sizeof (vms_symbol_entry));
      if (ret ==  NULL)
	{
	  bfd_set_error (bfd_error_no_memory);
	  return NULL;
	}
      entry = (struct bfd_hash_entry *) ret;
    }

  /* Call the allocation method of the base class.  */
  ret = (vms_symbol_entry *) bfd_hash_newfunc (entry, table, string);
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_hash_newfunc ret %p\n", ret);
#endif

  ret->symbol = NULL;

  return (struct bfd_hash_entry *)ret;
}

/* Object file input functions.  */

/* Return type and length from record header (buf) on Alpha.  */

void
_bfd_vms_get_header_values (bfd * abfd ATTRIBUTE_UNUSED,
			    unsigned char *buf,
			    int *type,
			    int *length)
{
  if (type != 0)
    *type = bfd_getl16 (buf);
  buf += 2;
  if (length != 0)
    *length = bfd_getl16 (buf);

#if VMS_DEBUG
  vms_debug (10, "_bfd_vms_get_header_values type %x, length %x\n", (type?*type:0), (length?*length:0));
#endif
}

/* Get next record from object file to vms_buf.
   Set PRIV(buf_size) and return it

   This is a little tricky since it should be portable.

   The openVMS object file has 'variable length' which means that
   read() returns data in chunks of (hopefully) correct and expected
   size. The linker (and other tools on vms) depend on that. Unix doesn't
   know about 'formatted' files, so reading and writing such an object
   file in a unix environment is not trivial.

   With the tool 'file' (available on all vms ftp sites), one
   can view and change the attributes of a file. Changing from
   'variable length' to 'fixed length, 512 bytes' reveals the
   record length at the first 2 bytes of every record. The same
   happens during the transfer of object files from vms to unix,
   at least with ucx, dec's implementation of tcp/ip.

   The vms format repeats the length at bytes 2 & 3 of every record.

   On the first call (file_format == FF_UNKNOWN) we check if
   the first and the third byte pair (!) of the record match.
   If they do it's an object file in an unix environment or with
   wrong attributes (FF_FOREIGN), else we should be in a vms
   environment where read() returns the record size (FF_NATIVE).

   Reading is always done in 2 steps.
   First just the record header is read and the length extracted
   by get_header_values,
   then the read buffer is adjusted and the remaining bytes are
   read in.

   All file i/o is always done on even file positions.  */

int
_bfd_vms_get_record (bfd * abfd)
{
  int test_len, test_start, remaining;
  unsigned char *vms_buf;

#if VMS_DEBUG
  vms_debug (8, "_bfd_vms_get_record\n");
#endif

  /* Minimum is 6 bytes on Alpha
     (2 bytes length, 2 bytes record id, 2 bytes length repeated)

     On the VAX there's no length information in the record
     so start with OBJ_S_C_MAXRECSIZ.   */

  if (PRIV (buf_size) == 0)
    {
      bfd_size_type amt;

      if (PRIV (is_vax))
	{
	  amt = OBJ_S_C_MAXRECSIZ;
	  PRIV (file_format) = FF_VAX;
	}
      else
	amt = 6;
      PRIV (vms_buf) = bfd_malloc (amt);
      PRIV (buf_size) = amt;
    }

  vms_buf = PRIV (vms_buf);

  if (vms_buf == 0)
    return -1;

  switch (PRIV (file_format))
    {
    case FF_UNKNOWN:
    case FF_FOREIGN:
      test_len = 6;			/* Probe 6 bytes.  */
      test_start = 2;			/* Where the record starts.  */
      break;

    case FF_NATIVE:
      test_len = 4;
      test_start = 0;
      break;

    default:
    case FF_VAX:
      test_len = 0;
      test_start = 0;
      break;
    }

  /* Skip odd alignment byte.  */

  if (bfd_tell (abfd) & 1)
    {
      if (bfd_bread (PRIV (vms_buf), (bfd_size_type) 1, abfd) != 1)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return 0;
	}
    }

  /* Read the record header on Alpha.  */
  if ((test_len != 0)
      && (bfd_bread (PRIV (vms_buf), (bfd_size_type) test_len, abfd)
	  != (bfd_size_type) test_len))
    {
      bfd_set_error (bfd_error_file_truncated);
      return 0;
    }

  /* Check file format on first call.  */
  if (PRIV (file_format) == FF_UNKNOWN)
    {						/* Record length repeats ?  */
      if (vms_buf[0] == vms_buf[4]
	  && vms_buf[1] == vms_buf[5])
	{
	  PRIV (file_format) = FF_FOREIGN;	/* Y: foreign environment.  */
	  test_start = 2;
	}
      else
	{
	  PRIV (file_format) = FF_NATIVE;	/* N: native environment.  */
	  test_start = 0;
	}
    }

  if (PRIV (is_vax))
    {
      PRIV (rec_length) = bfd_bread (vms_buf, (bfd_size_type) PRIV (buf_size),
				     abfd);
      if (PRIV (rec_length) <= 0)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return 0;
	}
      PRIV (vms_rec) = vms_buf;
    }
  else
    {
      /* Alpha.   */
      /* Extract vms record length.  */

      _bfd_vms_get_header_values (abfd, vms_buf + test_start, NULL,
				  & PRIV (rec_length));

      if (PRIV (rec_length) <= 0)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return 0;
	}

      /* That's what the linker manual says.  */

      if (PRIV (rec_length) > EOBJ_S_C_MAXRECSIZ)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return 0;
	}

      /* Adjust the buffer.  */

      if (PRIV (rec_length) > PRIV (buf_size))
	{
	  PRIV (vms_buf) = bfd_realloc (vms_buf,
					(bfd_size_type) PRIV (rec_length));
	  vms_buf = PRIV (vms_buf);
	  if (vms_buf == 0)
	    return -1;
	  PRIV (buf_size) = PRIV (rec_length);
	}

      /* Read the remaining record.  */
      remaining = PRIV (rec_length) - test_len + test_start;

#if VMS_DEBUG
      vms_debug (10, "bfd_bread remaining %d\n", remaining);
#endif
      if (bfd_bread (vms_buf + test_len, (bfd_size_type) remaining, abfd) !=
	  (bfd_size_type) remaining)
	{
	  bfd_set_error (bfd_error_file_truncated);
	  return 0;
	}
      PRIV (vms_rec) = vms_buf + test_start;
    }

#if VMS_DEBUG
  vms_debug (11, "bfd_bread rec_length %d\n", PRIV (rec_length));
#endif

  return PRIV (rec_length);
}

/* Get next vms record from file
   update vms_rec and rec_length to new (remaining) values.  */

int
_bfd_vms_next_record (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (8, "_bfd_vms_next_record (len %d, size %d)\n",
	      PRIV (rec_length), PRIV (rec_size));
#endif

  if (PRIV (rec_length) > 0)
    PRIV (vms_rec) += PRIV (rec_size);
  else
    {
      if (_bfd_vms_get_record (abfd) <= 0)
	return -1;
    }

  if (!PRIV (vms_rec) || !PRIV (vms_buf)
      || PRIV (vms_rec) >= (PRIV (vms_buf) + PRIV (buf_size)))
    return -1;

  if (PRIV (is_vax))
    {
      PRIV (rec_type) = *(PRIV (vms_rec));
      PRIV (rec_size) = PRIV (rec_length);
    }
  else
    _bfd_vms_get_header_values (abfd, PRIV (vms_rec), &PRIV (rec_type),
				&PRIV (rec_size));

  PRIV (rec_length) -= PRIV (rec_size);

#if VMS_DEBUG
  vms_debug (8, "_bfd_vms_next_record: rec %p, size %d, length %d, type %d\n",
	      PRIV (vms_rec), PRIV (rec_size), PRIV (rec_length),
	      PRIV (rec_type));
#endif

  return PRIV (rec_type);
}

/* Copy sized string (string with fixed length) to new allocated area
   size is string length (size of record)  */

char *
_bfd_vms_save_sized_string (unsigned char *str, int size)
{
  char *newstr = bfd_malloc ((bfd_size_type) size + 1);

  if (newstr == NULL)
    return NULL;
  strncpy (newstr, (char *) str, (size_t) size);
  newstr[size] = 0;

  return newstr;
}

/* Copy counted string (string with length at first byte) to new allocated area
   ptr points to length byte on entry  */

char *
_bfd_vms_save_counted_string (unsigned char *ptr)
{
  int len = *ptr++;

  return _bfd_vms_save_sized_string (ptr, len);
}

/* Stack routines for vms ETIR commands.  */

/* Push value and section index.  */

void
_bfd_vms_push (bfd * abfd, uquad val, int psect)
{
  static int last_psect;

#if VMS_DEBUG
  vms_debug (4, "<push %016lx (%d) at %d>\n", val, psect, PRIV (stackptr));
#endif

  if (psect >= 0)
    last_psect = psect;

  PRIV (stack[PRIV (stackptr)]).value = val;
  PRIV (stack[PRIV (stackptr)]).psect = last_psect;
  PRIV (stackptr)++;
  if (PRIV (stackptr) >= STACKSIZE)
    {
      bfd_set_error (bfd_error_bad_value);
      (*_bfd_error_handler) (_("Stack overflow (%d) in _bfd_vms_push"), PRIV (stackptr));
      exit (1);
    }
}

/* Pop value and section index.  */

uquad
_bfd_vms_pop (bfd * abfd, int *psect)
{
  uquad value;

  if (PRIV (stackptr) == 0)
    {
      bfd_set_error (bfd_error_bad_value);
      (*_bfd_error_handler) (_("Stack underflow in _bfd_vms_pop"));
      exit (1);
    }
  PRIV (stackptr)--;
  value = PRIV (stack[PRIV (stackptr)]).value;
  if ((psect != NULL) && (PRIV (stack[PRIV (stackptr)]).psect >= 0))
    *psect = PRIV (stack[PRIV (stackptr)]).psect;

#if VMS_DEBUG
  vms_debug (4, "<pop %016lx(%d)>\n", value, PRIV (stack[PRIV (stackptr)]).psect);
#endif

  return value;
}

/* Object file output functions.  */

/* GAS tends to write sections in little chunks (bfd_set_section_contents)
   which we can't use directly. So we save the little chunks in linked
   lists (one per section) and write them later.  */

/* Add a new vms_section structure to vms_section_table
   - forward chaining -.  */

static vms_section *
add_new_contents (bfd * abfd, sec_ptr section)
{
  vms_section *sptr, *newptr;

  sptr = PRIV (vms_section_table)[section->index];
  if (sptr != NULL)
    return sptr;

  newptr = bfd_alloc (abfd, (bfd_size_type) sizeof (vms_section));
  if (newptr == NULL)
    return NULL;
  newptr->contents = bfd_alloc (abfd, section->size);
  if (newptr->contents == NULL)
    return NULL;
  newptr->offset = 0;
  newptr->size = section->size;
  newptr->next = 0;
  PRIV (vms_section_table)[section->index] = newptr;
  return newptr;
}

/* Save section data & offset to a vms_section structure
   vms_section_table[] holds the vms_section chain.  */

bfd_boolean
_bfd_save_vms_section (bfd * abfd,
		       sec_ptr section,
		       const void * data,
		       file_ptr offset,
		       bfd_size_type count)
{
  vms_section *sptr;

  if (section->index >= VMS_SECTION_COUNT)
    {
      bfd_set_error (bfd_error_nonrepresentable_section);
      return FALSE;
    }
  if (count == (bfd_size_type)0)
    return TRUE;
  sptr = add_new_contents (abfd, section);
  if (sptr == NULL)
    return FALSE;
  memcpy (sptr->contents + offset, data, (size_t) count);

  return TRUE;
}

/* Get vms_section pointer to saved contents for section # index  */

vms_section *
_bfd_get_vms_section (bfd * abfd, int index)
{
  if (index >=  VMS_SECTION_COUNT)
    {
      bfd_set_error (bfd_error_nonrepresentable_section);
      return NULL;
    }
  return PRIV (vms_section_table)[index];
}

/* Object output routines.   */

/* Begin new record or record header
   write 2 bytes rectype
   write 2 bytes record length (filled in at flush)
   write 2 bytes header type (ommitted if rechead == -1).   */

void
_bfd_vms_output_begin (bfd * abfd, int rectype, int rechead)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_begin (type %d, head %d)\n", rectype,
	      rechead);
#endif

  _bfd_vms_output_short (abfd, (unsigned int) rectype);

  /* Save current output position to fill in length later.   */

  if (PRIV (push_level) > 0)
    PRIV (length_pos) = PRIV (output_size);

#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_begin: length_pos = %d\n",
	      PRIV (length_pos));
#endif

  /* Placeholder for length.  */
  _bfd_vms_output_short (abfd, 0);

  if (rechead != -1)
    _bfd_vms_output_short (abfd, (unsigned int) rechead);
}

/* Set record/subrecord alignment.   */

void
_bfd_vms_output_alignment (bfd * abfd, int alignto)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_alignment (%d)\n", alignto);
#endif

  PRIV (output_alignment) = alignto;
}

/* Prepare for subrecord fields.  */

void
_bfd_vms_output_push (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (6, "vms_output_push (pushed_size = %d)\n", PRIV (output_size));
#endif

  PRIV (push_level)++;
  PRIV (pushed_size) = PRIV (output_size);
}

/* End of subrecord fields.   */

void
_bfd_vms_output_pop (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (6, "vms_output_pop (pushed_size = %d)\n", PRIV (pushed_size));
#endif

  _bfd_vms_output_flush (abfd);
  PRIV (length_pos) = 2;

#if VMS_DEBUG
  vms_debug (6, "vms_output_pop: length_pos = %d\n", PRIV (length_pos));
#endif

  PRIV (pushed_size) = 0;
  PRIV (push_level)--;
}

/* Flush unwritten output, ends current record.  */

void
_bfd_vms_output_flush (bfd * abfd)
{
  int real_size = PRIV (output_size);
  int aligncount;
  int length;

#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_flush (real_size = %d, pushed_size %d at lenpos %d)\n",
	      real_size, PRIV (pushed_size), PRIV (length_pos));
#endif

  if (PRIV (push_level) > 0)
    length = real_size - PRIV (pushed_size);
  else
    length = real_size;

  if (length == 0)
    return;
  aligncount = (PRIV (output_alignment)
		- (length % PRIV (output_alignment))) % PRIV (output_alignment);

#if VMS_DEBUG
  vms_debug (6, "align: adding %d bytes\n", aligncount);
#endif

  while (aligncount-- > 0)
    {
      PRIV (output_buf)[real_size++] = 0;
      length++;
    }

  /* Put length to buffer.  */
  PRIV (output_size) = PRIV (length_pos);
  _bfd_vms_output_short (abfd, (unsigned int) length);

  if (PRIV (push_level) == 0)
    {
#ifndef VMS
	/* Write length first, see FF_FOREIGN in the input routines.  */
      fwrite (PRIV (output_buf) + 2, 2, 1, (FILE *) abfd->iostream);
#endif
      fwrite (PRIV (output_buf), (size_t) real_size, 1,
	      (FILE *) abfd->iostream);

      PRIV (output_size) = 0;
    }
  else
    {
      PRIV (output_size) = real_size;
      PRIV (pushed_size) = PRIV (output_size);
    }
}

/* End record output.   */

void
_bfd_vms_output_end (bfd * abfd)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_end\n");
#endif

  _bfd_vms_output_flush (abfd);
}

/* Check remaining buffer size

   Return what's left.  */

int
_bfd_vms_output_check (bfd * abfd, int size)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_check (%d)\n", size);
#endif

  return (MAX_OUTREC_SIZE - (PRIV (output_size) + size + MIN_OUTREC_LUFT));
}

/* Output byte (8 bit) value.  */

void
_bfd_vms_output_byte (bfd * abfd, unsigned int value)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_byte (%02x)\n", value);
#endif

  bfd_put_8 (abfd, value & 0xff, PRIV (output_buf) + PRIV (output_size));
  PRIV (output_size) += 1;
}

/* Output short (16 bit) value.  */

void
_bfd_vms_output_short (bfd * abfd, unsigned int value)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_short (%04x)\n", value);
#endif

  bfd_put_16 (abfd, (bfd_vma) value & 0xffff,
	      PRIV (output_buf) + PRIV (output_size));
  PRIV (output_size) += 2;
}

/* Output long (32 bit) value.  */

void
_bfd_vms_output_long (bfd * abfd, unsigned long value)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_long (%08lx)\n", value);
#endif

  bfd_put_32 (abfd, (bfd_vma) value, PRIV (output_buf) + PRIV (output_size));
  PRIV (output_size) += 4;
}

/* Output quad (64 bit) value.  */

void
_bfd_vms_output_quad (bfd * abfd, uquad value)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_quad (%016lx)\n", value);
#endif

  bfd_put_64(abfd, value, PRIV (output_buf) + PRIV (output_size));
  PRIV (output_size) += 8;
}

/* Output c-string as counted string.  */

void
_bfd_vms_output_counted (bfd * abfd, char *value)
{
  int len;

#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_counted (%s)\n", value);
#endif

  len = strlen (value);
  if (len == 0)
    {
      (*_bfd_error_handler) (_("_bfd_vms_output_counted called with zero bytes"));
      return;
    }
  if (len > 255)
    {
      (*_bfd_error_handler) (_("_bfd_vms_output_counted called with too many bytes"));
      return;
    }
  _bfd_vms_output_byte (abfd, (unsigned int) len & 0xff);
  _bfd_vms_output_dump (abfd, (unsigned char *) value, len);
}

/* Output character area.  */

void
_bfd_vms_output_dump (bfd * abfd,
		      unsigned char *data,
		      int length)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_dump (%d)\n", length);
#endif

  if (length == 0)
    return;

  memcpy (PRIV (output_buf) + PRIV (output_size), data, (size_t) length);
  PRIV (output_size) += length;
}

/* Output count bytes of value.  */

void
_bfd_vms_output_fill (bfd * abfd,
		      int value,
		      int count)
{
#if VMS_DEBUG
  vms_debug (6, "_bfd_vms_output_fill (val %02x times %d)\n", value, count);
#endif

  if (count == 0)
    return;
  memset (PRIV (output_buf) + PRIV (output_size), value, (size_t) count);
  PRIV (output_size) += count;
}

/* This hash routine borrowed from GNU-EMACS, and strengthened slightly.  ERY.  */

static int
hash_string (const char *ptr)
{
  const unsigned char *p = (unsigned char *) ptr;
  const unsigned char *end = p + strlen (ptr);
  unsigned char c;
  int hash = 0;

  while (p != end)
    {
      c = *p++;
      hash = ((hash << 3) + (hash << 15) + (hash >> 28) + c);
    }
  return hash;
}

/* Generate a length-hashed VMS symbol name (limited to maxlen chars).  */

char *
_bfd_vms_length_hash_symbol (bfd * abfd, const char *in, int maxlen)
{
  long int result;
  int in_len;
  char *new_name;
  const char *old_name;
  int i;
  static char outbuf[EOBJ_S_C_SYMSIZ+1];
  char *out = outbuf;

#if VMS_DEBUG
  vms_debug (4, "_bfd_vms_length_hash_symbol \"%s\"\n", in);
#endif

  if (maxlen > EOBJ_S_C_SYMSIZ)
    maxlen = EOBJ_S_C_SYMSIZ;

  /* Save this for later.  */
  new_name = out;

  /* We may need to truncate the symbol, save the hash for later.  */
  in_len = strlen (in);

  result = (in_len > maxlen) ? hash_string (in) : 0;

  old_name = in;

  /* Do the length checking.  */
  if (in_len <= maxlen)
    i = in_len;
  else
    {
      if (PRIV (flag_hash_long_names))
	i = maxlen-9;
      else
	i = maxlen;
    }

  strncpy (out, in, (size_t) i);
  in += i;
  out += i;

  if ((in_len > maxlen)
      && PRIV (flag_hash_long_names))
    sprintf (out, "_%08lx", result);
  else
    *out = 0;

#if VMS_DEBUG
  vms_debug (4, "--> [%d]\"%s\"\n", strlen (outbuf), outbuf);
#endif

  if (in_len > maxlen
	&& PRIV (flag_hash_long_names)
	&& PRIV (flag_show_after_trunc))
    printf (_("Symbol %s replaced by %s\n"), old_name, new_name);

  return outbuf;
}

/* Allocate and initialize a new symbol.  */

static asymbol *
new_symbol (bfd * abfd, char *name)
{
  asymbol *symbol;

#if VMS_DEBUG
  _bfd_vms_debug (7,  "new_symbol %s\n", name);
#endif

  symbol = bfd_make_empty_symbol (abfd);
  if (symbol == 0)
    return symbol;
  symbol->name = name;
  symbol->section = bfd_make_section (abfd, BFD_UND_SECTION_NAME);

  return symbol;
}

/* Allocate and enter a new private symbol.  */

vms_symbol_entry *
_bfd_vms_enter_symbol (bfd * abfd, char *name)
{
  vms_symbol_entry *entry;

#if VMS_DEBUG
  _bfd_vms_debug (6,  "_bfd_vms_enter_symbol %s\n", name);
#endif

  entry = (vms_symbol_entry *)
	  bfd_hash_lookup (PRIV (vms_symbol_table), name, FALSE, FALSE);
  if (entry == 0)
    {
#if VMS_DEBUG
      _bfd_vms_debug (8,  "creating hash entry for %s\n", name);
#endif
      entry = (vms_symbol_entry *) bfd_hash_lookup (PRIV (vms_symbol_table),
						    name, TRUE, FALSE);
      if (entry != 0)
	{
	  asymbol *symbol;
	  symbol = new_symbol (abfd, name);
	  if (symbol != 0)
	    {
	      entry->symbol = symbol;
	      PRIV (gsd_sym_count)++;
	      abfd->symcount++;
	    }
	  else
	    entry = 0;
	}
      else
	(*_bfd_error_handler) (_("failed to enter %s"), name);
    }
  else
    {
#if VMS_DEBUG
      _bfd_vms_debug (8,  "found hash entry for %s\n", name);
#endif
    }

#if VMS_DEBUG
  _bfd_vms_debug (7, "-> entry %p, entry->symbol %p\n", entry, entry->symbol);
#endif
  return entry;
}
