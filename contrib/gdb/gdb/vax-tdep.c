/* Print VAX instructions for GDB, the GNU debugger.
   Copyright 1986, 1989, 1991, 1992 Free Software Foundation, Inc.

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
#include "symtab.h"
#include "opcode/vax.h"

/* Vax instructions are never longer than this.  */
#define MAXLEN 62

/* Number of elements in the opcode table.  */
#define NOPCODES (sizeof votstrs / sizeof votstrs[0])

static unsigned char *print_insn_arg ();

/* Print the vax instruction at address MEMADDR in debugged memory,
   on STREAM.  Returns length of the instruction, in bytes.  */

int
vax_print_insn (memaddr, stream)
     CORE_ADDR memaddr;
     GDB_FILE *stream;
{
  unsigned char buffer[MAXLEN];
  register int i;
  register unsigned char *p;
  register char *d;

  read_memory (memaddr, buffer, MAXLEN);

  for (i = 0; i < NOPCODES; i++)
    if (votstrs[i].detail.code == buffer[0]
	|| votstrs[i].detail.code == *(unsigned short *)buffer)
      break;

  /* Handle undefined instructions.  */
  if (i == NOPCODES)
    {
      fprintf_unfiltered (stream, "0%o", buffer[0]);
      return 1;
    }

  fprintf_unfiltered (stream, "%s", votstrs[i].name);

  /* Point at first byte of argument data,
     and at descriptor for first argument.  */
  p = buffer + 1 + (votstrs[i].detail.code >= 0x100);
  d = votstrs[i].detail.args;

  if (*d)
    fputc_unfiltered (' ', stream);

  while (*d)
    {
      p = print_insn_arg (d, p, memaddr + (p - buffer), stream);
      d += 2;
      if (*d)
	fprintf_unfiltered (stream, ",");
    }
  return p - buffer;
}

static unsigned char *
print_insn_arg (d, p, addr, stream)
     char *d;
     register char *p;
     CORE_ADDR addr;
     GDB_FILE *stream;
{
  register int regnum = *p & 0xf;
  float floatlitbuf;

  if (*d == 'b')
    {
      if (d[1] == 'b')
	fprintf_unfiltered (stream, "0x%x", addr + *p++ + 1);
      else
	{
	  fprintf_unfiltered (stream, "0x%x", addr + *(short *)p + 2);
	  p += 2;
	}
    }
  else
    switch ((*p++ >> 4) & 0xf)
      {
      case 0:
      case 1:
      case 2:
      case 3:			/* Literal mode */
	if (d[1] == 'd' || d[1] == 'f' || d[1] == 'g' || d[1] == 'h')
	  {
	    *(int *)&floatlitbuf = 0x4000 + ((p[-1] & 0x3f) << 4);
	    fprintf_unfiltered (stream, "$%f", floatlitbuf);
	  }
	else
	  fprintf_unfiltered (stream, "$%d", p[-1] & 0x3f);
	break;

      case 4:			/* Indexed */
	p = (char *) print_insn_arg (d, p, addr + 1, stream);
	fprintf_unfiltered (stream, "[%s]", reg_names[regnum]);
	break;

      case 5:			/* Register */
	fprintf_unfiltered (stream, reg_names[regnum]);
	break;

      case 7:			/* Autodecrement */
	fputc_unfiltered ('-', stream);
      case 6:			/* Register deferred */
	fprintf_unfiltered (stream, "(%s)", reg_names[regnum]);
	break;

      case 9:			/* Autoincrement deferred */
	fputc_unfiltered ('@', stream);
	if (regnum == PC_REGNUM)
	  {
	    fputc_unfiltered ('#', stream);
	    print_address (*(long *)p, stream);
	    p += 4;
	    break;
	  }
      case 8:			/* Autoincrement */
	if (regnum == PC_REGNUM)
	  {
	    fputc_unfiltered ('#', stream);
	    switch (d[1])
	      {
	      case 'b':
		fprintf_unfiltered (stream, "%d", *p++);
		break;

	      case 'w':
		fprintf_unfiltered (stream, "%d", *(short *)p);
		p += 2;
		break;

	      case 'l':
		fprintf_unfiltered (stream, "%d", *(long *)p);
		p += 4;
		break;

	      case 'q':
		fprintf_unfiltered (stream, "0x%x%08x", ((long *)p)[1], ((long *)p)[0]);
		p += 8;
		break;

	      case 'o':
		fprintf_unfiltered (stream, "0x%x%08x%08x%08x",
			 ((long *)p)[3], ((long *)p)[2],
			 ((long *)p)[1], ((long *)p)[0]);
		p += 16;
		break;

	      case 'f':
		if (INVALID_FLOAT (p, 4))
		  fprintf_unfiltered (stream, "<<invalid float 0x%x>>", *(int *) p);
		else
		  fprintf_unfiltered (stream, "%f", *(float *) p);
		p += 4;
		break;

	      case 'd':
		if (INVALID_FLOAT (p, 8))
		  fprintf_unfiltered (stream, "<<invalid float 0x%x%08x>>",
			   ((long *)p)[1], ((long *)p)[0]);
		else
		  fprintf_unfiltered (stream, "%f", *(double *) p);
		p += 8;
		break;

	      case 'g':
		fprintf_unfiltered (stream, "g-float");
		p += 8;
		break;

	      case 'h':
		fprintf_unfiltered (stream, "h-float");
		p += 16;
		break;

	      }
	  }
	else
	  fprintf_unfiltered (stream, "(%s)+", reg_names[regnum]);
	break;

      case 11:			/* Byte displacement deferred */
	fputc_unfiltered ('@', stream);
      case 10:			/* Byte displacement */
	if (regnum == PC_REGNUM)
	  print_address (addr + *p + 2, stream);
	else
	  fprintf_unfiltered (stream, "%d(%s)", *p, reg_names[regnum]);
	p += 1;
	break;

      case 13:			/* Word displacement deferred */
	fputc_unfiltered ('@', stream);
      case 12:			/* Word displacement */
	if (regnum == PC_REGNUM)
	  print_address (addr + *(short *)p + 3, stream);
	else
	  fprintf_unfiltered (stream, "%d(%s)", *(short *)p, reg_names[regnum]);
	p += 2;
	break;

      case 15:			/* Long displacement deferred */
	fputc_unfiltered ('@', stream);
      case 14:			/* Long displacement */
	if (regnum == PC_REGNUM)
	  print_address (addr + *(long *)p + 5, stream);
	else
	  fprintf_unfiltered (stream, "%d(%s)", *(long *)p, reg_names[regnum]);
	p += 4;
      }

  return (unsigned char *) p;
}
