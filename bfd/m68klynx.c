/* BFD back-end for m68k binaries under LynxOS.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 2001, 2002, 2003
   Free Software Foundation, Inc.

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

#define N_SHARED_LIB(x) 0

#define TEXT_START_ADDR 0
#define TARGET_PAGE_SIZE 4096
#define SEGMENT_SIZE TARGET_PAGE_SIZE
#define DEFAULT_ARCH bfd_arch_m68k

/* Do not "beautify" the CONCAT* macro args.  Traditional C will not
   remove whitespace added here, and thus will fail to concatenate
   the tokens.  */
#define MY(OP) CONCAT2 (m68klynx_aout_,OP)
#define TARGETNAME "a.out-m68k-lynx"

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"

#include "libaout.h"
#include "aout/aout64.h"

#define TARGET_IS_BIG_ENDIAN_P

#ifdef LYNX_CORE

char *lynx_core_file_failing_command();
int lynx_core_file_failing_signal();
bfd_boolean lynx_core_file_matches_executable_p();
const bfd_target *lynx_core_file_p();

#define	MY_core_file_failing_command lynx_core_file_failing_command
#define	MY_core_file_failing_signal lynx_core_file_failing_signal
#define	MY_core_file_matches_executable_p lynx_core_file_matches_executable_p
#define	MY_core_file_p lynx_core_file_p

#endif /* LYNX_CORE */

#include "aout-target.h"
