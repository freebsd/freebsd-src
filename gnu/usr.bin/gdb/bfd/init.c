/* bfd initialization stuff
   Copyright (C) 1990-1991 Free Software Foundation, Inc.
   Written by Steve Chamberlain of Cygnus Support.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

extern void DEFUN_VOID (bfd_section_init);

static boolean initialized = false;

/*
SECTION
	Initialization

	This is the initialization section

*/

/*
FUNCTION
	bfd_init

SYNOPSIS
	void bfd_init(void);

DESCRIPTION
	This routine must be called before any other bfd function to
	initialize magical internal data structures.
*/

void DEFUN_VOID(bfd_init)
{
  if (initialized == false) {
    initialized = true;

    bfd_arch_init();
  }
}


/*
INTERNAL_FUNCTION
	bfd_check_init

DESCRIPTION
	This routine is called before any other bfd function using
	initialized data is used to ensure that the structures have
	been initialized. Soon this function will go away, and the bfd
	library will assume that bfd_init has been called.

SYNOPSIS
	void bfd_check_init(void);
*/

void DEFUN_VOID(bfd_check_init)
{
  if (initialized == false) {
    bfd_init();
  }
}
