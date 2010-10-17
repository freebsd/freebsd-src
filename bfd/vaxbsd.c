/* BFD back-end for BSD and Ultrix/VAX (1K page size) a.out-ish binaries.
   Copyright 2002, 2003 Free Software Foundation, Inc.

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

#define N_HEADER_IN_TEXT(x) 0
#define ENTRY_CAN_BE_ZERO
#define N_SHARED_LIB(x) 0 /* Avoids warning */
#define TEXT_START_ADDR 0
#define TARGET_PAGE_SIZE 1024
#define SEGMENT_SIZE TARGET_PAGE_SIZE
#define DEFAULT_ARCH bfd_arch_vax

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not   
   remove whitespace added here, and thus will fail to concatenate the 
   tokens.  */
#define MY(OP) CONCAT2 (vaxbsd_,OP)

#define TARGETNAME "a.out-vax-bsd"

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "libaout.h"

#include "aout-target.h"
