/*
 * gawkmisc.c --- miscellanious gawk routines that are OS specific.
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-2001 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

#include "awk.h"

#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif

/* some old compilers don't grok #elif. sigh */

#if defined(MSDOS) || defined(OS2) || defined(WIN32)
#include "gawkmisc.pc"
#else /* not MSDOS, not OS2, not WIN32 */
#if defined(VMS)
#include "vms/gawkmisc.vms"
#else /* not VMS */
#if defined(atarist)
#include "unsupported/atari/gawkmisc.atr"
#else /* not atarist */
#if defined(TANDEM)
#include "tmiscc"
#else /* not TANDEM */
#include "posix/gawkmisc.c"
#endif /* not TANDEM */
#endif /* not atarist */
#endif /* not VMS */
#endif /* not MSDOS, not OS2, not WIN32 */

/* xmalloc --- provide this so that other GNU library routines work */

#if __STDC__
typedef void *pointer;
#else
typedef char *pointer;
#endif

extern pointer xmalloc P((size_t bytes));	/* get rid of gcc warning */

pointer
xmalloc(size_t bytes)
{
	pointer p;

	emalloc(p, pointer, bytes, "xmalloc");

	return p;
}
