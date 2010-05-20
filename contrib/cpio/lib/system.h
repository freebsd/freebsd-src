/* System dependent definitions for GNU tar.

   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2003,
   2004, 2005, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <alloca.h>

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__
#  define __attribute__(spec) /* empty */
# endif
#endif

#include <sys/types.h>
#include <ctype.h>

/* IN_CTYPE_DOMAIN (C) is nonzero if the unsigned char C can safely be given
   as an argument to <ctype.h> macros like `isspace'.  */
#if STDC_HEADERS
# define IN_CTYPE_DOMAIN(c) 1
#else
# define IN_CTYPE_DOMAIN(c) ((unsigned) (c) <= 0177)
#endif

#define ISDIGIT(c) ((unsigned) (c) - '0' <= 9)
#define ISODIGIT(c) ((unsigned) (c) - '0' <= 7)
#define ISPRINT(c) (IN_CTYPE_DOMAIN (c) && isprint (c))
#define ISSPACE(c) (IN_CTYPE_DOMAIN (c) && isspace (c))

/* Declare string and memory handling routines.  Take care that an ANSI
   string.h and pre-ANSI memory.h might conflict, and that memory.h and
   strings.h conflict on some systems.  */

#if STDC_HEADERS || HAVE_STRING_H
# include <string.h>
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
#else
# include <strings.h>
# ifndef strchr
#  define strchr index
# endif
# ifndef strrchr
#  define strrchr rindex
# endif
# ifndef memcpy
#  define memcpy(d, s, n) bcopy ((char const *) (s), (char *) (d), n)
# endif
# ifndef memcmp
#  define memcmp(a, b, n) bcmp ((char const *) (a), (char const *) (b), n)
# endif
#endif

/* Declare errno.  */

#include <errno.h>
#ifndef errno
extern int errno;
#endif

/* Declare open parameters.  */

#if HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif
				/* Pick only one of the next three: */
#ifndef O_RDONLY
# define O_RDONLY	0	/* only allow read */
#endif
#ifndef O_WRONLY
# define O_WRONLY	1	/* only allow write */
#endif
#ifndef O_RDWR
# define O_RDWR		2	/* both are allowed */
#endif
#ifndef O_ACCMODE
# define O_ACCMODE (O_RDONLY | O_RDWR | O_WRONLY)
#endif
				/* The rest can be OR-ed in to the above: */
#ifndef O_CREAT
# define O_CREAT	8	/* create file if needed */
#endif
#ifndef O_EXCL
# define O_EXCL		16	/* file cannot already exist */
#endif
#ifndef O_TRUNC
# define O_TRUNC	32	/* truncate file on open */
#endif

#ifndef O_BINARY
# define O_BINARY 0
#endif
#ifndef O_DIRECTORY
# define O_DIRECTORY 0
#endif
#ifndef O_NOATIME
# define O_NOATIME 0
#endif
#ifndef O_NONBLOCK
# define O_NONBLOCK 0
#endif

/* Declare file status routines and bits.  */

#include <sys/stat.h>

#if !HAVE_LSTAT && !defined lstat
# define lstat stat
#endif

#if STX_HIDDEN && !_LARGE_FILES /* AIX */
# ifdef stat
#  undef stat
# endif
# define stat(file_name, buf) statx (file_name, buf, STATSIZE, STX_HIDDEN)
# ifdef lstat
#  undef lstat
# endif
# define lstat(file_name, buf) statx (file_name, buf, STATSIZE, STX_HIDDEN | STX_LINK)
#endif

#if STAT_MACROS_BROKEN
# undef S_ISBLK
# undef S_ISCHR
# undef S_ISCTG
# undef S_ISDIR
# undef S_ISFIFO
# undef S_ISLNK
# undef S_ISREG
# undef S_ISSOCK
#endif

/* On MSDOS, there are missing things from <sys/stat.h>.  */
#if MSDOS
# define S_ISUID 0
# define S_ISGID 0
# define S_ISVTX 0
#endif

#ifndef S_ISDIR
# define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
# define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISBLK
# ifdef S_IFBLK
#  define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
# else
#  define S_ISBLK(mode) 0
# endif
#endif
#ifndef S_ISCHR
# ifdef S_IFCHR
#  define S_ISCHR(mode) (((mode) & S_IFMT) == S_IFCHR)
# else
#  define S_ISCHR(mode) 0
# endif
#endif
#ifndef S_ISCTG
# ifdef S_IFCTG
#  define S_ISCTG(mode) (((mode) & S_IFMT) == S_IFCTG)
# else
#  define S_ISCTG(mode) 0
# endif
#endif
#ifndef S_ISDOOR
# define S_ISDOOR(mode) 0
#endif
#ifndef S_ISFIFO
# ifdef S_IFIFO
#  define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
# else
#  define S_ISFIFO(mode) 0
# endif
#endif
#ifndef S_ISLNK
# ifdef S_IFLNK
#  define S_ISLNK(mode) (((mode) & S_IFMT) == S_IFLNK)
# else
#  define S_ISLNK(mode) 0
# endif
#endif
#ifndef S_ISSOCK
# ifdef S_IFSOCK
#  define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)
# else
#  define S_ISSOCK(mode) 0
# endif
#endif

#if !HAVE_MKFIFO && !defined mkfifo && defined S_IFIFO
# define mkfifo(file_name, mode) (mknod (file_name, (mode) | S_IFIFO, 0))
#endif

#ifndef S_ISUID
# define S_ISUID 0004000
#endif
#ifndef S_ISGID
# define S_ISGID 0002000
#endif
#ifndef S_ISVTX
# define S_ISVTX 0001000
#endif
#ifndef S_IRUSR
# define S_IRUSR 0000400
#endif
#ifndef S_IWUSR
# define S_IWUSR 0000200
#endif
#ifndef S_IXUSR
# define S_IXUSR 0000100
#endif
#ifndef S_IRGRP
# define S_IRGRP 0000040
#endif
#ifndef S_IWGRP
# define S_IWGRP 0000020
#endif
#ifndef S_IXGRP
# define S_IXGRP 0000010
#endif
#ifndef S_IROTH
# define S_IROTH 0000004
#endif
#ifndef S_IWOTH
# define S_IWOTH 0000002
#endif
#ifndef S_IXOTH
# define S_IXOTH 0000001
#endif

#define MODE_WXUSR	(S_IWUSR | S_IXUSR)
#define MODE_R		(S_IRUSR | S_IRGRP | S_IROTH)
#define MODE_RW		(S_IWUSR | S_IWGRP | S_IWOTH | MODE_R)
#define MODE_RWX	(S_IXUSR | S_IXGRP | S_IXOTH | MODE_RW)
#define MODE_ALL	(S_ISUID | S_ISGID | S_ISVTX | MODE_RWX)

/* Include <unistd.h> before any preprocessor test of _POSIX_VERSION.  */
#include <unistd.h>

#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

/* Declare make device, major and minor.  Since major is a function on
   SVR4, we have to resort to GOT_MAJOR instead of just testing if
   major is #define'd.  */

#if MAJOR_IN_MKDEV
# include <sys/mkdev.h>
# if !defined(makedev) && defined(mkdev)
#  define makedev(a,b) mkdev((a),(b))
# endif
# define GOT_MAJOR
#endif

#if MAJOR_IN_SYSMACROS
# include <sys/sysmacros.h>
# define GOT_MAJOR
#endif

/* Some <sys/types.h> defines the macros. */
#ifdef major
# define GOT_MAJOR
#endif

#ifndef GOT_MAJOR
# if MSDOS
#  define major(device)		(device)
#  define minor(device)		(device)
#  define makedev(major, minor)	(((major) << 8) | (minor))
#  define GOT_MAJOR
# endif
#endif

/* For HP-UX before HP-UX 8, major/minor are not in <sys/sysmacros.h>.  */
#ifndef GOT_MAJOR
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
#  include <sys/mknod.h>
#  define GOT_MAJOR
# endif
#endif

#ifndef GOT_MAJOR
# define major(device)		(((device) >> 8) & 0xff)
# define minor(device)		((device) & 0xff)
# define makedev(major, minor)	(((major) << 8) | (minor))
#endif

#undef GOT_MAJOR

/* Declare wait status.  */

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(s)	(((s) >> 8) & 0xff)
#endif
#ifndef WIFSIGNALED
# define WIFSIGNALED(s)	(((s) & 0xffff) - 1 < (unsigned) 0xff)
#endif
#ifndef WTERMSIG
# define WTERMSIG(s)	((s) & 0x7f)
#endif

/* FIXME: It is wrong to use BLOCKSIZE for buffers when the logical block
   size is greater than 512 bytes; so ST_BLKSIZE code below, in preparation
   for some cleanup in this area, later.  */

/* Extract or fake data from a `struct stat'.  ST_BLKSIZE gives the
   optimal I/O blocksize for the file, in bytes.  Some systems, like
   Sequents, return st_blksize of 0 on pipes.  */

#define DEFAULT_ST_BLKSIZE 512

#if !HAVE_ST_BLKSIZE
# define ST_BLKSIZE(statbuf) DEFAULT_ST_BLKSIZE
#else
# define ST_BLKSIZE(statbuf) \
    ((statbuf).st_blksize > 0 ? (statbuf).st_blksize : DEFAULT_ST_BLKSIZE)
#endif

/* Extract or fake data from a `struct stat'.  ST_NBLOCKS gives the
   number of ST_NBLOCKSIZE-byte blocks in the file (including indirect blocks).
   HP-UX counts st_blocks in 1024-byte units,
   this loses when mixing HP-UX and BSD filesystems with NFS.  AIX PS/2
   counts st_blocks in 4K units.  */

#if !HAVE_ST_BLOCKS
# if defined(_POSIX_SOURCE) || !defined(BSIZE)
#  define ST_NBLOCKS(statbuf) ((statbuf).st_size / ST_NBLOCKSIZE + ((statbuf).st_size % ST_NBLOCKSIZE != 0))
# else
   off_t st_blocks ();
#  define ST_NBLOCKS(statbuf) (st_blocks ((statbuf).st_size))
# endif
#else
# define ST_NBLOCKS(statbuf) ((statbuf).st_blocks)
# if defined(hpux) || defined(__hpux__) || defined(__hpux)
#  define ST_NBLOCKSIZE 1024
# else
#  if defined(_AIX) && defined(_I386)
#   define ST_NBLOCKSIZE (4 * 1024)
#  endif
# endif
#endif

#ifndef ST_NBLOCKSIZE
# define ST_NBLOCKSIZE 512
#endif

#define ST_IS_SPARSE(st)                                  \
  (ST_NBLOCKS (st)                                        \
    < ((st).st_size / ST_NBLOCKSIZE + ((st).st_size % ST_NBLOCKSIZE != 0)))

/* Declare standard functions.  */

#if STDC_HEADERS
# include <stdlib.h>
#else
void *malloc ();
char *getenv ();
#endif

#include <stdbool.h>
#include <stddef.h>

#include <stdio.h>
#if !defined _POSIX_VERSION && MSDOS
# include <io.h>
#endif

#if WITH_DMALLOC
# define DMALLOC_FUNC_CHECK
# include <dmalloc.h>
#endif

#include <limits.h>

#ifndef MB_LEN_MAX
# define MB_LEN_MAX 1
#endif

#include <inttypes.h>

#include <intprops.h>

#define UINTMAX_STRSIZE_BOUND INT_BUFSIZE_BOUND (uintmax_t)

/* Prototypes for external functions.  */

#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if !HAVE_SETLOCALE
# define setlocale(category, locale) /* empty */
#endif

#include <time.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
#endif

/* Library modules.  */

#include <dirname.h>
#include <error.h>
#include <savedir.h>
#include <unlocked-io.h>
#include <xalloc.h>

#include <gettext.h>
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#if MSDOS
# include <process.h>
# define SET_BINARY_MODE(arc) setmode(arc, O_BINARY)
# define ERRNO_IS_EACCES errno == EACCES
# define mkdir(file, mode) (mkdir) (file)
# define TTY_NAME "con"
# define sys_reset_uid_gid()
#else
# include <pwd.h>
# include <grp.h>
# define SET_BINARY_MODE(arc)
# define ERRNO_IS_EACCES 0
# define TTY_NAME "/dev/tty"
# define sys_reset_uid_gid() \
 do { setuid (getuid ()); setgid (getgid ()); } while (0)
#endif

#if XENIX
# include <sys/inode.h>
#endif
