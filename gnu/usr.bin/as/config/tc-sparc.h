/* tc-sparc.h - Macros and type defines for the sparc.
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

/*
 * $FreeBSD$
 */

#define TC_SPARC 1

#define LOCAL_LABELS_FB
#define WORKING_DOT_WORD

#ifdef __NetBSD__
#define AOUT_MACHTYPE	138
#endif

#ifdef sun
#define AOUT_MACHTYPE	3
#define AOUT_VERSION	1
#endif

#define tc_headers_hook(a)		{;} /* don't need it. */
#define tc_crawl_symbol_chain(a)	{;} /* don't need it. */

void tc_aout_pre_write_hook();

#define LISTING_HEADER "SPARC GAS "

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-sparc.h */
