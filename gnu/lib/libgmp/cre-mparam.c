/* cre-mparam.c -- Create machine-depedent parameter file.

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

#include "gmp.h"

unsigned int
ulog2 (x)
     unsigned long int x;
{
  unsigned int i;
  for (i = 0;  x != 0;  i++)
    x >>= 1;
  return i;
}

main ()
{
  int i;

  unsigned long int max_uli;
  int bits_uli;

  unsigned long int max_ui;
  int bits_ui;

  unsigned long int max_usi;
  int bits_usi;

  unsigned long int max_uc;
  int bits_uc;

  max_uli = 1;
  for (i = 0; ; i++)
    {
      if (max_uli == 0)
	break;
      max_uli <<= 1;
    }
  bits_uli = i;

  max_ui = 1;
  for (i = 0; ; i++)
    {
      if ((unsigned int) max_ui == 0)
	break;
      max_ui <<= 1;
    }
  bits_ui = i;

  max_usi = 1;
  for (i = 0; ; i++)
    {
      if ((unsigned short int) max_usi == 0)
	break;
      max_usi <<= 1;
    }
  bits_usi = i;

  max_uc = 1;
  for (i = 0; ; i++)
    {
      if ((unsigned char) max_uc == 0)
	break;
      max_uc <<= 1;
    }
  bits_uc = i;

  puts ("/* gmp-mparam.h -- Compiler/machine parameter header file.");
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

  printf ("#define BITS_PER_MP_LIMB %d\n", bits_uli);
  printf ("#define BYTES_PER_MP_LIMB %d\n", sizeof(mp_limb));

  printf ("#define BITS_PER_LONGINT %d\n", bits_uli);
  printf ("#define BITS_PER_INT %d\n", bits_ui);
  printf ("#define BITS_PER_SHORTINT %d\n", bits_usi);
  printf ("#define BITS_PER_CHAR %d\n", bits_uc);

  exit (0);
}
