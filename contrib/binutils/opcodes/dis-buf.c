/* Disassemble from a buffer, for GNU.
   Copyright 1993, 1994, 1996, 1997, 1998, 1999, 2000
   Free Software Foundation, Inc.

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

#include "sysdep.h"
#include "dis-asm.h"
#include <errno.h>
#include "opintl.h"

/* Get LENGTH bytes from info's buffer, at target address memaddr.
   Transfer them to myaddr.  */
int
buffer_read_memory (memaddr, myaddr, length, info)
     bfd_vma memaddr;
     bfd_byte *myaddr;
     unsigned int length;
     struct disassemble_info *info;
{
  unsigned int opb = info->octets_per_byte;
  unsigned int end_addr_offset = length / opb;
  unsigned int max_addr_offset = info->buffer_length / opb; 
  unsigned int octets = (memaddr - info->buffer_vma) * opb;

  if (memaddr < info->buffer_vma
      || memaddr - info->buffer_vma + end_addr_offset > max_addr_offset)
    /* Out of bounds.  Use EIO because GDB uses it.  */
    return EIO;
  memcpy (myaddr, info->buffer + octets, length);

  return 0;
}

/* Print an error message.  We can assume that this is in response to
   an error return from buffer_read_memory.  */
void
perror_memory (status, memaddr, info)
     int status;
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  if (status != EIO)
    /* Can't happen.  */
    info->fprintf_func (info->stream, _("Unknown error %d\n"), status);
  else
    /* Actually, address between memaddr and memaddr + len was
       out of bounds.  */
    info->fprintf_func (info->stream,
			_("Address 0x%x is out of bounds.\n"), memaddr);
}

/* This could be in a separate file, to save miniscule amounts of space
   in statically linked executables.  */

/* Just print the address is hex.  This is included for completeness even
   though both GDB and objdump provide their own (to print symbolic
   addresses).  */

void
generic_print_address (addr, info)
     bfd_vma addr;
     struct disassemble_info *info;
{
  char buf[30];

  sprintf_vma (buf, addr);
  (*info->fprintf_func) (info->stream, "0x%s", buf);
}

#if 0
/* Just concatenate the address as hex.  This is included for
   completeness even though both GDB and objdump provide their own (to
   print symbolic addresses).  */

void generic_strcat_address PARAMS ((bfd_vma, char *, int));

void
generic_strcat_address (addr, buf, len)
     bfd_vma addr;
     char *buf;
     int len;
{
  if (buf != (char *)NULL && len > 0)
    {
      char tmpBuf[30];

      sprintf_vma (tmpBuf, addr);
      if ((strlen (buf) + strlen (tmpBuf)) <= (unsigned int) len)
	strcat (buf, tmpBuf);
      else
	strncat (buf, tmpBuf, (len - strlen(buf)));
    }
  return;
}
#endif

/* Just return the given address.  */

int
generic_symbol_at_address (addr, info)
     bfd_vma addr ATTRIBUTE_UNUSED;
     struct disassemble_info *info ATTRIBUTE_UNUSED;
{
  return 1;
}
