/* Remote target communications for d10v connected via a serial line.
   Copyright 1988, 1991, 1992, 1993, 1994, 1995, 1996, 1997 Free
   Software Foundation, Inc.

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
#include "gdb_string.h"
#include <fcntl.h>
#include "frame.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "target.h"
#include "wait.h"
/*#include "terminal.h"*/
#include "gdbcmd.h"
#include "objfiles.h"
#include "gdb-stabs.h"
#include "gdbthread.h"

#include "dcache.h"

#ifdef USG
#include <sys/types.h>
#endif

#include <signal.h>
#include "serial.h"

/* Prototypes for local functions */

static void remote_d10v_open PARAMS ((char *name, int from_tty));

/* Define the target subroutine names */
static struct target_ops remote_d10v_ops;

/* Open a connection to a remote debugger.
   NAME is the filename used for communication.  */

static void
remote_d10v_open (name, from_tty)
     char *name;
     int from_tty;
{
  pop_target ();
  push_remote_target (name, from_tty);
}


/* Translate a GDB virtual ADDR/LEN into a format the remote target
   understands.  Returns number of bytes that can be transfered
   starting at taddr, ZERO if no bytes can be transfered. */
int
remote_d10v_translate_xfer_address (memaddr, nr_bytes, taddr)
     CORE_ADDR memaddr;
     int nr_bytes;
     CORE_ADDR *taddr;
{
  CORE_ADDR phys;
  CORE_ADDR seg;
  CORE_ADDR off;
  char *from = "unknown";
  char *to = "unknown";
  unsigned short imap0 = read_register (IMAP0_REGNUM);
  unsigned short imap1 = read_register (IMAP1_REGNUM);
  unsigned short dmap = read_register (DMAP_REGNUM);

  /* GDB interprets addresses as:

       0x00xxxxxx: Logical data address segment        (DMAP translated memory)
       0x01xxxxxx: Logical instruction address segment (IMAP translated memory)
       0x10xxxxxx: Physical data memory segment        (On-chip data memory)
       0x11xxxxxx: Physical instruction memory segment (On-chip insn memory)
       0x12xxxxxx: Phisical unified memory segment     (Unified memory)

     The remote d10v board interprets addresses as:

       0x00xxxxxx: Phisical unified memory segment     (Unified memory)
       0x01xxxxxx: Physical instruction memory segment (On-chip insn memory)
       0x02xxxxxx: Physical data memory segment        (On-chip data memory)

     Translate according to current IMAP/dmap registers */

  enum {
    targ_unified = 0x00000000,
    targ_insn = 0x01000000,
    targ_data = 0x02000000,
  };

  seg = (memaddr >> 24);
  off = (memaddr & 0xffffffL);

  switch (seg) 
      {
      case 0x00: /* in logical data address segment */
	{
	  from = "logical-data";
	  if (off <= 0x7fffL)
	    {
	      /* On chip data */
	      phys = targ_data + off;
	      if (off + nr_bytes > 0x7fffL)
		/* don't cross VM boundary */
		nr_bytes = 0x7fffL - off + 1;
	      to = "chip-data";
	    }
	  else if (off <= 0xbfffL)
	    {
	      short map = dmap;
	      if (map & 0x1000)
		{
		  /* Instruction memory */
		  phys = targ_insn | ((map & 0xf) << 14) | (off & 0x3fff);
		  to = "chip-insn";
		}
	      else
		{
		  /* Unified memory */
		  phys = targ_unified | ((map & 0x3ff) << 14) | (off & 0x3fff);
		  to = "unified";
		}
	      if (off + nr_bytes > 0xbfffL)
		/* don't cross VM boundary */
		nr_bytes = (0xbfffL - off + 1);
	    }	    
	  else
	    {
	      /* Logical address out side of data segments, not supported */
	      return (0);
	    }
	  break;
	}

      case 0x01: /* in logical instruction address segment */
	{
	  short map;
	  from = "logical-insn";
	  if (off <= 0x1ffffL)
	    {
	      map = imap0;
	    }
	  else if (off <= 0x3ffffL)
	    {
	      map = imap1;
	    }
	  else
	    {
	      /* Logical address outside of IMAP[01] segment, not
		 supported */
	      return (0);
	    }
	  if ((off & 0x1ffff) + nr_bytes > 0x1ffffL)
	    {
	      /* don't cross VM boundary */
	      nr_bytes = 0x1ffffL - (off & 0x1ffffL) + 1;
	    }
	  if (map & 0x1000)
	    /* Instruction memory */
	    {
	      phys = targ_insn | off;
	      to = "chip-insn";
	    }
	  else
	    {
	      phys = ((map & 0x7fL) << 17) + (off & 0x1ffffL);
	      if (phys > 0xffffffL)
		/* Address outside of unified address segment */
		return (0);
	      phys |= targ_unified;
	      to = "unified";
	    }
	  break;
	}

      case 0x10: /* Physical data memory segment */
	from = "phys-data";
	phys = targ_data | off;
	to = "chip-data";
	break;

      case 0x11: /* Physical instruction memory */
	from = "phys-insn";
	phys = targ_insn | off;
	to = "chip-insn";
	break;

      case 0x12: /* Physical unified memory */
	from = "phys-unified";
	phys = targ_unified | off;
	to = "unified";
	break;

      default:
	return (0);
      }


  *taddr = phys;
  return nr_bytes;
}


void
_initialize_remote_d10v ()
{
  remote_d10v_ops.to_shortname = "d10v";
  remote_d10v_ops.to_longname = "Remote d10v serial target in gdb-specific protocol";
  remote_d10v_ops.to_doc = "Use a remote d10v via a serial line, using a gdb-specific protocol.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  remote_d10v_ops.to_open = remote_d10v_open;

  add_target (&remote_d10v_ops);
}
