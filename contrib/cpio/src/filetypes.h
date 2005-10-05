/* filetypes.h - deal with POSIX annoyances
   Copyright (C) 1991 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Include sys/types.h and sys/stat.h before this file.  */

#ifndef S_ISREG			/* Doesn't have POSIX.1 stat stuff.  */
#define mode_t unsigned short
#endif

/* Define the POSIX macros for systems that lack them.  */
#if !defined(S_ISBLK) && defined(S_IFBLK)
#define	S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif
#if !defined(S_ISCHR) && defined(S_IFCHR)
#define	S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif
#if !defined(S_ISDIR) && defined(S_IFDIR)
#define	S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#define	S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISFIFO) && defined(S_IFIFO)
#define	S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif
#if !defined(S_ISLNK) && defined(S_IFLNK)
#define	S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#if !defined(S_ISSOCK) && defined(S_IFSOCK)
#define	S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX network special */
#define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
#endif

/* Define the file type bits used in cpio archives.
   They have the same values as the S_IF bits in traditional Unix.  */

#define	CP_IFMT 0170000		/* Mask for all file type bits.  */

#if defined(S_ISBLK)
#define CP_IFBLK 0060000
#endif
#if defined(S_ISCHR)
#define CP_IFCHR 0020000
#endif
#if defined(S_ISDIR)
#define CP_IFDIR 0040000
#endif
#if defined(S_ISREG)
#define CP_IFREG 0100000
#endif
#if defined(S_ISFIFO)
#define CP_IFIFO 0010000
#endif
#if defined(S_ISLNK)
#define CP_IFLNK 0120000
#endif
#if defined(S_ISSOCK)
#define CP_IFSOCK 0140000
#endif
#if defined(S_ISNWK)
#define CP_IFNWK 0110000
#endif

#ifndef S_ISLNK
#define lstat stat
#endif
int lstat ();
int stat ();
