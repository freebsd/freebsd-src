/* cre-conv-tab.c -- Create conversion table in a wordsize-dependent way.

 $Id$

Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU MP Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU MP Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <math.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"

static unsigned long int
upow (unsigned long int b, unsigned int e)
{
  unsigned long int y = 1;

  while (e != 0)
    {
      while ((e & 1) == 0)
	{
	  b = b * b;
	  e >>= 1;
	}
      y = y * b;
      e -= 1;
    }

  return y;
}

unsigned int
ulog2 (unsigned long int x)
{
  unsigned int i;
  for (i = 0;  x != 0;  i++)
    x >>= 1;
  return i;
}

void
main (void)
{
  int i;
  unsigned long idig;
  unsigned long big_base, big_base_inverted;
  double fdig;
  int dummy;
  int normalization_steps;

  unsigned long int max_uli;
  int bits_uli;

  max_uli = 1;
  for (i = 1; ; i++)
    {
      if ((max_uli << 1) == 0)
	break;
      max_uli <<= 1;
    }
  bits_uli = i;

  puts ("/* __mp_bases -- Structure for conversion between internal binary");
  puts ("   format and strings in base 2..36.  The fields are explained in");
  puts ("   gmp-impl.h.");
  puts ("");
  puts ("   ***** THIS FILE WAS CREATED BY A PROGRAM.  DON'T EDIT IT! *****");
  puts ("");
  puts ("Copyright (C) 1991 Free Software Foundation, Inc.");
  puts ("");
  puts ("This file is part of the GNU MP Library.");
  puts ("");
  puts ("The GNU MP Library is free software; you can redistribute it and/or");
  puts ("modify it under the terms of the GNU General Public License as");
  puts ("published by the Free Software Foundation; either version 2, or");
  puts ("(at your option) any later version.");
  puts ("");
  puts ("The GNU MP Library is distributed in the hope that it will be");
  puts ("useful, but WITHOUT ANY WARRANTY; without even the implied warranty");
  puts ("of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the");
  puts ("GNU General Public License for more details.");
  puts ("");
  puts ("You should have received a copy of the GNU General Public License");
  puts ("along with the GNU MP Library; see the file COPYING.  If not, write");
  puts ("to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,");
  puts ("USA.  */");
  puts ("");
  puts ("#include \"gmp.h\"");
  puts ("#include \"gmp-impl.h\"");
  puts ("");

  puts ("const struct bases __mp_bases[37] =\n{");
  puts ("  /*  0 */ {0, 0, 0, 0.0},");
  puts ("  /*  1 */ {0, 0, 0, 0.0},");
  for (i = 2; i <= 36; i++)
    {
      /* The weird expression here is because many /bin/cc compilers
	 generate incorrect code for conversions from large unsigned
	 integers to double.  */
      fdig = log(2.0)/log((double) i);
      idig = floor(bits_uli * fdig);
      if ((i & (i - 1)) == 0)
	{
	  big_base = ulog2 (i) - 1;
	  big_base_inverted = 0;
	}
      else
	{
	  big_base = upow (i, idig);
	  for (normalization_steps = 0;
	       (long int) (big_base << normalization_steps) >= 0;
	       normalization_steps++)
	    ;
	  udiv_qrnnd (big_base_inverted, dummy,
		      -(big_base << normalization_steps), 0,
		      big_base << normalization_steps);
	}
      printf ("  /* %2u */ {%lu, 0x%lX, 0x%lX, %.8f},\n",
	      i, idig, big_base, big_base_inverted, fdig);
    }
  puts ("};");

  exit (0);
}
