/* S-record download support for GDB, the GNU debugger.
   Copyright 1995, 1996 Free Software Foundation, Inc.

This file is part of GDB.

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

#include "defs.h"
#include "serial.h"
#include "srec.h"

extern int remote_debug;

static int make_srec PARAMS ((char *srec, CORE_ADDR targ_addr, bfd *abfd,
			      asection *sect, int sectoff, int *maxrecsize,
			      int flags));

/*  Download an executable by converting it to S records.  DESC is a serial_t
    to send the data to.  FILE is the name of the file to be loaded.
    MAXRECSIZE is the length in chars of the largest S-record the host can
    accomodate.  This is measured from the starting `S' to the last char of the
    checksum.  FLAGS is various random flags, and HASHMARK is non-zero to cause
    a `#' to be printed out for each record loaded.  */

void
load_srec (desc, file, maxrecsize, flags, hashmark)
     serial_t desc;
     const char *file;
     int maxrecsize;
     int flags;
     int hashmark;
{
  bfd *abfd;
  asection *s;
  char *srec;
  int i;
  int reclen;

  srec = (char *)alloca (maxrecsize + 1);

  abfd = bfd_openr (file, 0);
  if (!abfd)
    {
      printf_filtered ("Unable to open file %s\n", file);
      return;
    }

  if (bfd_check_format (abfd, bfd_object) == 0)
    {
      printf_filtered ("File is not an object file\n");
      return;
    }
  
  for (s = abfd->sections; s; s = s->next)
    if (s->flags & SEC_LOAD)
      {
	int numbytes;

	printf_filtered ("%s\t: 0x%4x .. 0x%4x  ", s->name, s->vma,
			 s->vma + s->_raw_size);
	gdb_flush (gdb_stdout);

	for (i = 0; i < s->_raw_size; i += numbytes)
	  {
	    reclen = maxrecsize;
	    numbytes = make_srec (srec, s->vma + i, abfd, s, i, &reclen,
				  flags);

	    if (remote_debug)
	      fprintf_unfiltered (gdb_stderr, "%.*s\\r\n", reclen-1, srec);
	    SERIAL_WRITE (desc, srec, reclen);

	    if (hashmark)
	      {
		putchar_unfiltered ('#');
		gdb_flush (gdb_stdout);
	      }
	  }			/* Per-packet (or S-record) loop */

	putchar_unfiltered ('\n');
      }				/* Loadable sections */

  if (hashmark) 
    putchar_unfiltered ('\n');
  
  /* Write a type 7 terminator record. no data for a type 7, and there
     is no data, so len is 0.  */

  reclen = maxrecsize;
  make_srec (srec, abfd->start_address, NULL, NULL, 0, &reclen, flags);

  if (remote_debug)
    fprintf_unfiltered (gdb_stderr, "%.*s\\r\n", reclen-1, srec);
  SERIAL_WRITE (desc, srec, reclen);

  SERIAL_WRITE (desc, "\r\r", 2); /* Some monitors need these to wake up */

  SERIAL_FLUSH_INPUT (desc);
}

/*
 * make_srec -- make an srecord. This writes each line, one at a
 *	time, each with it's own header and trailer line.
 *	An srecord looks like this:
 *
 * byte count-+     address
 * start ---+ |        |       data        +- checksum
 *	    | |        |                   |
 *	  S01000006F6B692D746573742E73726563E4
 *	  S315000448600000000000000000FC00005900000000E9
 *	  S31A0004000023C1400037DE00F023604000377B009020825000348D
 *	  S30B0004485A0000000000004E
 *	  S70500040000F6
 *
 *	S<type><length><address><data><checksum>
 *
 *      Where
 *      - length
 *        is the number of bytes following upto the checksum. Note that
 *        this is not the number of chars following, since it takes two
 *        chars to represent a byte.
 *      - type
 *        is one of:
 *        0) header record
 *        1) two byte address data record
 *        2) three byte address data record
 *        3) four byte address data record
 *        7) four byte address termination record
 *        8) three byte address termination record
 *        9) two byte address termination record
 *       
 *      - address
 *        is the start address of the data following, or in the case of
 *        a termination record, the start address of the image
 *      - data
 *        is the data.
 *      - checksum
 *	  is the sum of all the raw byte data in the record, from the length
 *        upwards, modulo 256 and subtracted from 255.
 *
 * This routine returns the length of the S-record.
 *
 */

static int
make_srec (srec, targ_addr, abfd, sect, sectoff, maxrecsize, flags)
     char *srec;
     CORE_ADDR targ_addr;
     bfd *abfd;
     asection *sect;
     int sectoff;
     int *maxrecsize;
     int flags;
{
  unsigned char checksum;
  int tmp;
  const static char hextab[] = "0123456789ABCDEF";
  const static char data_code_table[] = "xx123";
  const static char term_code_table[] = "xx987";
  const static char *formats[] = {NULL, NULL, "S%c%02X%04X", "S%c%02X%06X",
				    "S%c%02X%08X"};
  char const *code_table;
  int addr_size;
  int payload_size;
  int type_code;
  char *binbuf;
  char *p;

  if (sect)
    {
      tmp = flags;		/* Data record */
      code_table = data_code_table;
      binbuf = alloca (*maxrecsize/2);
    }
  else
    {
      tmp = flags >> SREC_TERM_SHIFT; /* Term record */
      code_table = term_code_table;
    }

  if (tmp & SREC_4_BYTE_ADDR && targ_addr > 0xffffff)
    addr_size = 4;
  else if (tmp & SREC_3_BYTE_ADDR && targ_addr > 0xffff)
    addr_size = 3;
  else if (tmp & SREC_2_BYTE_ADDR && targ_addr > 0xff)
    addr_size = 2;
  else
    fatal ("make_srec:  Bad address (0x%x), or bad flags (0x%x).", targ_addr,
	   flags);

/* Now that we know the address size, we can figure out how much data this
   record can hold.  */

  if (sect)
    {
      payload_size = (*maxrecsize - (1 + 1 + 2 + addr_size * 2 + 2)) / 2;
      payload_size = min (payload_size, sect->_raw_size - sectoff);

      bfd_get_section_contents (abfd, sect, binbuf, sectoff, payload_size);
    }
  else
    payload_size = 0;		/* Term packets have no payload */

/* Output the header.  */

  sprintf (srec, formats[addr_size], code_table[addr_size],
	   addr_size + payload_size + 1, targ_addr);

/* Note that the checksum is calculated on the raw data, not the hexified
   data.  It includes the length, address and the data portions of the
   packet.  */

  checksum = 0;
  
  checksum += (payload_size + addr_size + 1 /* Packet length */
	       + (targ_addr & 0xff)	/* Address... */
	       + ((targ_addr >>  8) & 0xff)
	       + ((targ_addr >> 16) & 0xff)
	       + ((targ_addr >> 24) & 0xff));
  
  p = srec + 1 + 1 + 2 + addr_size * 2;

  /* build the srecord */
  for (tmp = 0; tmp < payload_size; tmp++)
    {
      unsigned char k;

      k = binbuf[tmp];
      *p++ = hextab [k >> 4];
      *p++ = hextab [k & 0xf];
      checksum += k;
    }

  checksum = ~checksum;

  *p++ = hextab[checksum >> 4];
  *p++ = hextab[checksum & 0xf];
  *p++ = '\r';

  *maxrecsize = p - srec;
  return payload_size;
}
