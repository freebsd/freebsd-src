/* BFD back-end for Apple et al PowerPC Mac "XCOFF" files.
   Copyright 1995, 2000, 2001 Free Software Foundation, Inc.

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
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Tweak coffcode.h based on this being a PowerMac instead of RS/6000.  */

#define POWERMAC

#define TARGET_SYM	pmac_xcoff_vec
#define TARGET_NAME	"xcoff-powermac"

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"
#include "coff/rs6000.h"
#include "libcoff.h"
#include "xcoff-target.h"
