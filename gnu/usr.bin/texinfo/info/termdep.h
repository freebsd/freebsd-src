/* termdep.h -- System things that terminal.c depends on. */

/* This file is part of GNU Info, a program for reading online documentation
   stored in Info format.

   Copyright (C) 1993 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#if defined (HAVE_SYS_FCNTL_H)
#include <sys/fcntl.h>
#else
#include <fcntl.h>
#endif /* !HAVE_SYS_FCNTL_H */

#if defined (HAVE_TERMIO_H)
#include <termio.h>
#include <string.h>
#if defined (HAVE_SYS_PTEM_H)
#if !defined (M_XENIX)
#include <sys/stream.h>
#include <sys/ptem.h>
#undef TIOCGETC
#else /* M_XENIX */
#define tchars tc
#endif /* M_XENIX */
#endif /* HAVE_SYS_PTEM_H */
#else /* !HAVE_TERMIO_H */
#include <sys/file.h>
#include <sgtty.h>
#include <strings.h>
#endif /* !HAVE_TERMIO_H */

#if defined (HAVE_SYS_TTOLD_H)
#include <sys/ttold.h>
#endif /* HAVE_SYS_TTOLD_H */

#if !defined (HAVE_RINDEX)
#undef index
#undef rindex
#define index strchr
#define rindex strrchr
#endif

#if !defined (HAVE_BCOPY)
#undef bcopy
#define bcopy(source, dest, count) memcpy(dest, source, count)
#endif

/* eof */
