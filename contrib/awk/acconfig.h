/*
 * acconfig.h -- configuration definitions for gawk.
 */

/* 
 * Copyright (C) 1995-2000 the Free Software Foundation, Inc.
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

@TOP@

#undef HAVE_STRINGIZE	/* can use ANSI # operator in cpp */
#undef REGEX_MALLOC	/* use malloc instead of alloca in regex.c */
#undef SPRINTF_RET	/* return type of sprintf */
#undef BITOPS		/* bitwise ops (undocumented feature) */
#undef NONDECDATA	/* non-decimal input data (undocumented feature) */
#undef _FILE_OFFSET_BITS /* bits in a file offset, where this matters */
#undef _LARGEFILE_SOURCE /* makes fseeko etc. visible on some hosts */
#undef _LARGE_FILES	/* emables large files on AIX-style hosts */

@BOTTOM@

#include <custom.h>	/* overrides for stuff autoconf can't deal with */
