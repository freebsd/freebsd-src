/* filemode.c -- make a string describing file modes
   Copyright (C) 1985, 1990 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include <sys/stat.h>

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
	'-' for any other file type

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
	'T' if the file is sticky but not executable. */

void
filemodestring (statp, str)
     struct stat *statp;
     char *str;
{
  mode_string (statp->st_mode, str);
}

/* Like filemodestring, but only the relevant part of the `struct stat'
   is given as an argument. */

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
   '-' for any other file type. */

static char
ftypelet (bits)
     unsigned short bits;
{
  switch (bits & S_IFMT)
    {
    default:
      return '-';
    case S_IFDIR:
      return 'd';
#ifdef S_IFLNK
    case S_IFLNK:
      return 'l';
#endif
#ifdef S_IFCHR
    case S_IFCHR:
      return 'c';
#endif
#ifdef S_IFBLK
    case S_IFBLK:
      return 'b';
#endif
#ifdef S_IFMPC
    case S_IFMPC:
    case S_IFMPB:
      return 'm';
#endif
#ifdef S_IFSOCK
    case S_IFSOCK:
      return 's';
#endif
#ifdef S_IFIFO
#if S_IFIFO != S_IFSOCK
    case S_IFIFO:
      return 'p';
#endif
#endif
#ifdef S_IFNWK			/* HP-UX */
    case S_IFNWK:
      return 'n';
#endif
    }
}

/* Look at read, write, and execute bits in BITS and set
   flags in CHARS accordingly. */

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
   according to the file mode BITS. */

static void
setst (bits, chars)
     unsigned short bits;
     char *chars;
{
#ifdef S_ISUID
  if (bits & S_ISUID)
    {
      if (chars[3] != 'x')
	/* Set-uid, but not executable by owner. */
	chars[3] = 'S';
      else
	chars[3] = 's';
    }
#endif
#ifdef S_ISGID
  if (bits & S_ISGID)
    {
      if (chars[6] != 'x')
	/* Set-gid, but not executable by group. */
	chars[6] = 'S';
      else
	chars[6] = 's';
    }
#endif
#ifdef S_ISVTX
  if (bits & S_ISVTX)
    {
      if (chars[9] != 'x')
	/* Sticky, but not executable by others. */
	chars[9] = 'T';
      else
	chars[9] = 't';
    }
#endif
}


