/* filemode.c -- make a string describing file modes
   Copyright (C) 1985, 1990 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_IREAD
#define S_IREAD S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC S_IXUSR
#endif

#if !defined(S_ISREG) || defined(NO_MODE_T)
/* Doesn't have POSIX.1 stat stuff or doesn't have mode_t.  */
#define mode_t unsigned short
#endif

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
#if !defined(S_ISMPB) && defined(S_IFMPB) /* V7 */
#define S_ISMPB(m) (((m) & S_IFMT) == S_IFMPB)
#define S_ISMPC(m) (((m) & S_IFMT) == S_IFMPC)
#endif
#if !defined(S_ISNWK) && defined(S_IFNWK) /* HP/UX */
#define S_ISNWK(m) (((m) & S_IFMT) == S_IFNWK)
#endif

void mode_string ();
static char ftypelet ();
static void rwx ();
static void setst ();

/* filemodestring - fill in string STR with an ls-style ASCII
   representation of the st_mode field of file stats block STATP.
   10 characters are stored in STR; no terminating null is added.
   The characters stored in STR are:

   0	File type.  'd' for directory, 'c' for character
	special, 'b' for block special, 'm' for multiplex,
	'l' for symbolic link, 's' for socket, 'p' for fifo,
	'-' for regular, '?' for any other file type

   1	'r' if the owner may read, '-' otherwise.

   2	'w' if the owner may write, '-' otherwise.

   3	'x' if the owner may execute, 's' if the file is
	set-user-id, '-' otherwise.
	'S' if the file is set-user-id, but the execute
	bit isn't set.

   4	'r' if group members may read, '-' otherwise.

   5	'w' if group members may write, '-' otherwise.

   6	'x' if group members may execute, 's' if the file is
	set-group-id, '-' otherwise.
	'S' if it is set-group-id but not executable.

   7	'r' if any user may read, '-' otherwise.

   8	'w' if any user may write, '-' otherwise.

   9	'x' if any user may execute, 't' if the file is "sticky"
	(will be retained in swap space after execution), '-'
	otherwise.
	'T' if the file is sticky but not executable.  */

void
filemodestring (statp, str)
     struct stat *statp;
     char *str;
{
  mode_string (statp->st_mode, str);
}

/* Like filemodestring, but only the relevant part of the `struct stat'
   is given as an argument.  */

void
mode_string (mode, str)
     unsigned short mode;
     char *str;
{
  str[0] = ftypelet (mode);
  rwx ((mode & 0700) << 0, &str[1]);
  rwx ((mode & 0070) << 3, &str[4]);
  rwx ((mode & 0007) << 6, &str[7]);
  setst (mode, str);
}

/* Return a character indicating the type of file described by
   file mode BITS:
   'd' for directories
   'b' for block special files
   'c' for character special files
   'm' for multiplexor files
   'l' for symbolic links
   's' for sockets
   'p' for fifos
   '-' for regular files
   '?' for any other file type.  */

static char
ftypelet (bits)
     mode_t bits;
{
#ifdef S_ISBLK
  if (S_ISBLK (bits))
    return 'b';
#endif
  if (S_ISCHR (bits))
    return 'c';
  if (S_ISDIR (bits))
    return 'd';
  if (S_ISREG (bits))
    return '-';
#ifdef S_ISFIFO
  if (S_ISFIFO (bits))
    return 'p';
#endif
#ifdef S_ISLNK
  if (S_ISLNK (bits))
    return 'l';
#endif
#ifdef S_ISSOCK
  if (S_ISSOCK (bits))
    return 's';
#endif
#ifdef S_ISMPC
  if (S_ISMPC (bits))
    return 'm';
#endif
#ifdef S_ISNWK
  if (S_ISNWK (bits))
    return 'n';
#endif
  return '?';
}

/* Look at read, write, and execute bits in BITS and set
   flags in CHARS accordingly.  */

static void
rwx (bits, chars)
     unsigned short bits;
     char *chars;
{
  chars[0] = (bits & S_IREAD) ? 'r' : '-';
  chars[1] = (bits & S_IWRITE) ? 'w' : '-';
  chars[2] = (bits & S_IEXEC) ? 'x' : '-';
}

/* Set the 's' and 't' flags in file attributes string CHARS,
   according to the file mode BITS.  */

static void
setst (bits, chars)
     unsigned short bits;
     char *chars;
{
#ifdef S_ISUID
  if (bits & S_ISUID)
    {
      if (chars[3] != 'x')
	/* Set-uid, but not executable by owner.  */
	chars[3] = 'S';
      else
	chars[3] = 's';
    }
#endif
#ifdef S_ISGID
  if (bits & S_ISGID)
    {
      if (chars[6] != 'x')
	/* Set-gid, but not executable by group.  */
	chars[6] = 'S';
      else
	chars[6] = 's';
    }
#endif
#ifdef S_ISVTX
  if (bits & S_ISVTX)
    {
      if (chars[9] != 'x')
	/* Sticky, but not executable by others.  */
	chars[9] = 'T';
      else
	chars[9] = 't';
    }
#endif
}
