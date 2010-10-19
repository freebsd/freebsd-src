

/* itbl-test.c

   Copyright (C) 1997  Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful, 
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

/* Stand-alone test for instruction specification table support.
   Run using "itbl-test <itbl> <asm.s>"
   where <itbl> is the name of the instruction table,
   and <asm.s> is the name of the assembler fie. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "itbl-ops.h"

static int test_reg (e_processor processor, e_type type, char *name,
		     unsigned long val);

int 
main (int argc, char **argv)
{
  unsigned int insn;
  FILE *fas;
  int aline = 0;
  char s[81], *name;

  if (argc < 3)
    {
      printf ("usage: %s itbl asm.s\n", argv[0]);
      exit (0);
    }
  if (itbl_parse (argv[1]) != 0)
    {
      printf ("failed to parse itbl\n");
      exit (0);
    }

  fas = fopen (argv[2], "r");
  if (fas == 0)
    {
      printf ("failed to open asm file %s\n", argv[2]);
      exit (0);
    }
  while (fgets (s, 80, fas))
    {
      char *p;
      aline++;

      if (p = strchr (s, ';'), p)	/* strip comments */
	*p = 0;
      if (p = strchr (s, '#'), p)	/* strip comments */
	*p = 0;
      p = s + strlen (s) - 1;
      while (p >= s && (*p == ' ' || *p == '\t' || *p == '\n'))	/* strip trailing spaces */
	p--;
      *(p + 1) = 0;
      p = s;
      while (*p && (*p == ' ' || *p == '\t' || *p == '\n'))	/* strip leading spaces */
	p++;
      if (!*p)
	continue;

      name = itbl_get_field (&p);
      insn = itbl_assemble (name, p);
      if (insn == 0)
	printf ("line %d: Invalid instruction (%s)\n", aline, s);
      else
	{
	  char buf[128];
	  printf ("line %d: insn(%s) = 0x%x)\n", aline, s, insn);
	  if (!itbl_disassemble (buf, insn))
	    printf ("line %d: Can't disassemble instruction "
		    "(0x%x)\n", aline, insn);
	  else
	    printf ("line %d: disasm(0x%x) = %s)\n", aline, insn, buf);
	}
    }

  test_reg (1, e_dreg, "d1", 1);
  test_reg (3, e_creg, "c2", 22);
  test_reg (3, e_dreg, "d3", 3);

  return 0;
}

static int 
test_reg (e_processor processor, e_type type, char *name,
	  unsigned long val)
{
  char *n;
  unsigned long v;

  n = itbl_get_name (processor, type, val);
  if (!n || strcmp (n, name))
    printf ("Error - reg name not found for proessor=%d, type=%d, val=%d\n",
	    processor, type, val);
  else
    printf ("name=%s found for processor=%d, type=%d, val=%d\n",
	    n, processor, type, val);

  /* We require that names be unique amoung processors and types. */
  if (! itbl_get_reg_val (name, &v)
      || v != val)
    printf ("Error - reg val not found for processor=%d, type=%d, name=%s\n",
	    processor, type, name);
  else
    printf ("val=0x%x found for processor=%d, type=%d, name=%s\n",
	    v, processor, type, name);
  return 0;
}
