/* alphavms.h -- BFD definitions for an openVMS host
   Copyright 1996, 2000 Free Software Foundation, Inc.
   Written by Klaus Kämpf (kkaempf@progis.de)
   of proGIS Softwareentwicklung, Aachen, Germany

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

#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/file.h>
#include <stdlib.h>
#include <unixlib.h>
#include <unixio.h>
#include <time.h>

#include "bfd.h"

#ifndef BFD_HOST_64_BIT
/* Make the basic types 64-bit quantities on the host.
   Also provide the support macros BFD needs.  */
# ifdef __GNUC__
#  define	BFD_HOST_64_BIT	long long
# else
#  define	BFD_HOST_64_BIT	long
# endif
typedef unsigned BFD_HOST_64_BIT uint64_type;
typedef BFD_HOST_64_BIT int64_type;

# define sprintf_vma(s,x) sprintf (s, "%016lx", x) /* BFD_HOST_64_BIT */
# define fprintf_vma(f,x) fprintf (f, "%016lx", x) /* BFD_HOST_64_BIT */

# define BYTES_IN_PRINTF_INT 4

/* These must have type unsigned long because they are used as
   arguments in printf functions.  */
# define uint64_typeLOW(x) ((unsigned long) (((x) & 0xffffffff))) /* BFD_HOST_64_BIT */
# define uint64_typeHIGH(x) ((unsigned long) (((x) >> 32) & 0xffffffff)) /* BFD_HOST_64_BIT */

#endif /* BFD_HOST_64_BIT */

#include "fopen-vms.h"

#define NO_FCNTL 1

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

extern int getpagesize PARAMS ((void));
