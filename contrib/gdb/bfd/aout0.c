/* BFD backend for SunOS style a.out with flags set to 0
   Copyright (C) 1990, 91, 92, 93, 1994 Free Software Foundation, Inc.
   Written by Cygnus Support.

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

#define TARGETNAME "a.out-zero-big"
#define MY(OP) CAT(aout0_big_,OP)

#include "bfd.h"

#define MY_exec_hdr_flags 0

#define MACHTYPE_OK(mtype) \
  ((mtype) == M_UNKNOWN || (mtype) == M_68010 || (mtype) == M_68020)

/* Include the usual a.out support.  */
#include "aoutf1.h"
