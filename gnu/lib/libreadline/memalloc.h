/* memalloc.h -- consolidate code for including alloca.h or malloc.h and
   defining alloca. */

/* Copyright (C) 1993 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Bash is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with Bash; see the file COPYING.  If not, write to the Free Software
   Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#if !defined (__MEMALLOC_H__)
#  define __MEMALLOC_H__

#if defined (sparc) && defined (sun) && !defined (HAVE_ALLOCA_H)
#  define HAVE_ALLOCA_H
#endif

#if defined (__GNUC__) && !defined (HAVE_ALLOCA)
#  define HAVE_ALLOCA
#endif

#if defined (HAVE_ALLOCA_H) && !defined (HAVE_ALLOCA)
#  define HAVE_ALLOCA
#endif /* HAVE_ALLOCA_H && !HAVE_ALLOCA */

#if !defined (BUILDING_MAKEFILE)

#if defined (__GNUC__)
#  undef alloca
#  define alloca __builtin_alloca
#else /* !__GNUC__ */
#  if defined (HAVE_ALLOCA_H)
#    if defined (IBMESA)
#      include <malloc.h>
#    else /* !IBMESA */
#      include <alloca.h>
#    endif /* !IBMESA */
#  else
extern char *alloca ();
#  endif /* !HAVE_ALLOCA_H */
#endif /* !__GNUC__ */

#endif /* !BUILDING_MAKEFILE */

#endif /* __MEMALLOC_H__ */
