/*-
 * Copyright (c) 2009 Michihiro NAKAJIMA
 * Copyright (c) 2003-2006 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

/*
 * TODO: A lot of stuff in here isn't actually used by libarchive and
 * can be trimmed out.  Note that this file is used by libarchive and
 * libarchive_test but nowhere else.  (But note that it gets compiled
 * with many different Windows environments, including MinGW, Visual
 * Studio, and Cygwin.  Significant changes should be tested in all three.)
 */

/*
 * TODO: Don't use off_t in here.  Use __int64 instead.  Note that
 * Visual Studio and the Windows SDK define off_t as 32 bits; Win32's
 * more modern file handling APIs all use __int64 instead of off_t.
 */

#ifndef LIBARCHIVE_ARCHIVE_WINDOWS_H_INCLUDED
#define	LIBARCHIVE_ARCHIVE_WINDOWS_H_INCLUDED

/* Start of configuration for native Win32  */

#include <errno.h>
#define	set_errno(val)	((errno)=val)
#include <io.h>
#include <stdlib.h>   //brings in NULL
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>
#include <direct.h>
#define NOCRYPT
#include <windows.h>
//#define	EFTYPE 7

#if !defined(STDIN_FILENO)
#define STDIN_FILENO 0
#endif

#if !defined(STDOUT_FILENO)
#define STDOUT_FILENO 1
#endif

#if !defined(STDERR_FILENO)
#define STDERR_FILENO 2
#endif


#if defined(_MSC_VER)
/* TODO: Fix the code, don't suppress the warnings. */
#pragma warning(disable:4244)   /* 'conversion' conversion from 'type1' to 'type2', possible loss of data */
#endif
#if defined(__BORLANDC__)
#pragma warn -8068	/* Constant out of range in comparison. */
#pragma warn -8072	/* Suspicious pointer arithmetic. */
#endif

#ifndef NULL
#ifdef  __cplusplus
#define	NULL    0
#else
#define	NULL    ((void *)0)
#endif
#endif

/* Alias the Windows _function to the POSIX equivalent. */
#define	access		_access
#define	chdir		__la_chdir
#define	chmod		__la_chmod
#define	close		_close
#define	fcntl		__la_fcntl
#ifndef fileno
#define	fileno		_fileno
#endif
#define	fstat		__la_fstat
#define	ftruncate	__la_ftruncate
#define	futimes		__la_futimes
#define	getcwd		_getcwd
#define link		__la_link
#define	lseek		__la_lseek
#define	lstat		__la_stat
#define	mbstowcs	__la_mbstowcs
#define	mkdir(d,m)	__la_mkdir(d, m)
#define	mktemp		_mktemp
#define	open		__la_open
#define	read		__la_read
#define	rmdir		__la_rmdir
#if !defined(__BORLANDC__)
#define setmode		_setmode
#endif
#define	stat(path,stref)		__la_stat(path,stref)
#if !defined(__BORLANDC__)
#define	strdup		_strdup
#endif
#define	tzset		_tzset
#if !defined(__BORLANDC__)
#define	umask		_umask
#endif
#define	unlink		__la_unlink
#define	utimes		__la_utimes
#define	waitpid		__la_waitpid
#define	write		__la_write

#ifndef O_RDONLY
#define	O_RDONLY	_O_RDONLY
#define	O_WRONLY	_O_WRONLY
#define	O_TRUNC		_O_TRUNC
#define	O_CREAT		_O_CREAT
#define	O_EXCL		_O_EXCL
#define	O_BINARY	_O_BINARY
#endif

#ifndef _S_IFIFO
  #define	_S_IFIFO        0010000   /* pipe */
#endif
#ifndef _S_IFCHR
  #define	_S_IFCHR        0020000   /* character special */
#endif
#ifndef _S_IFDIR
  #define	_S_IFDIR        0040000   /* directory */
#endif
#ifndef _S_IFBLK
  #define	_S_IFBLK        0060000   /* block special */
#endif
#ifndef _S_IFLNK
  #define	_S_IFLNK        0120000   /* symbolic link */
#endif
#ifndef _S_IFSOCK
  #define	_S_IFSOCK       0140000   /* socket */
#endif
#ifndef	_S_IFREG
  #define	_S_IFREG        0100000   /* regular */
#endif
#ifndef	_S_IFMT
  #define	_S_IFMT         0170000   /* file type mask */
#endif

#ifndef S_IFIFO
#define	S_IFIFO     _S_IFIFO
#endif
//#define	S_IFCHR  _S_IFCHR
//#define	S_IFDIR  _S_IFDIR
#ifndef S_IFBLK
#define	S_IFBLK     _S_IFBLK
#endif
#ifndef S_IFLNK
#define	S_IFLNK     _S_IFLNK
#endif
#ifndef S_IFSOCK
#define	S_IFSOCK    _S_IFSOCK
#endif
//#define	S_IFREG  _S_IFREG
//#define	S_IFMT   _S_IFMT

#ifndef S_ISBLK
#define	S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)	/* block special */
#define	S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)	/* fifo or socket */
#define	S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)	/* char special */
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)	/* directory */
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)	/* regular file */
#endif
#define	S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK) /* Symbolic link */
#define	S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK) /* Socket */

#define	_S_ISUID        0004000   /* set user id on execution */
#define	_S_ISGID        0002000   /* set group id on execution */
#define	_S_ISVTX        0001000   /* save swapped text even after use */

#define	S_ISUID        _S_ISUID
#define	S_ISGID        _S_ISGID
#define	S_ISVTX        _S_ISVTX

#define	_S_IRWXU	     (_S_IREAD | _S_IWRITE | _S_IEXEC)
#define	_S_IXUSR	     _S_IEXEC  /* read permission, user */
#define	_S_IWUSR	     _S_IWRITE /* write permission, user */
#define	_S_IRUSR	     _S_IREAD  /* execute/search permission, user */
#define	_S_IRWXG        (_S_IRWXU >> 3)
#define	_S_IXGRP        (_S_IXUSR >> 3) /* read permission, group */
#define	_S_IWGRP        (_S_IWUSR >> 3) /* write permission, group */
#define	_S_IRGRP        (_S_IRUSR >> 3) /* execute/search permission, group */
#define	_S_IRWXO        (_S_IRWXG >> 3) 
#define	_S_IXOTH        (_S_IXGRP >> 3) /* read permission, other */
#define	_S_IWOTH        (_S_IWGRP >> 3) /* write permission, other */
#define	_S_IROTH        (_S_IRGRP  >> 3) /* execute/search permission, other */

#ifndef S_IRWXU
#define	S_IRWXU	     _S_IRWXU
#define	S_IXUSR	     _S_IXUSR
#define	S_IWUSR	     _S_IWUSR
#define	S_IRUSR	     _S_IRUSR
#endif
#define	S_IRWXG        _S_IRWXG
#define	S_IXGRP        _S_IXGRP
#define	S_IWGRP        _S_IWGRP
#define	S_IRGRP        _S_IRGRP
#define	S_IRWXO        _S_IRWXO
#define	S_IXOTH        _S_IXOTH
#define	S_IWOTH        _S_IWOTH
#define	S_IROTH        _S_IROTH

#define	F_DUPFD	  	0	/* Duplicate file descriptor.  */
#define	F_GETFD		1	/* Get file descriptor flags.  */
#define	F_SETFD		2	/* Set file descriptor flags.  */
#define	F_GETFL		3	/* Get file status flags.  */
#define	F_SETFL		4	/* Set file status flags.  */
#define	F_GETOWN		5	/* Get owner (receiver of SIGIO).  */
#define	F_SETOWN		6	/* Set owner (receiver of SIGIO).  */
#define	F_GETLK		7	/* Get record locking info.  */
#define	F_SETLK		8	/* Set record locking info (non-blocking).  */
#define	F_SETLKW		9	/* Set record locking info (blocking).  */

/* XXX missing */
#define	F_GETLK64	7	/* Get record locking info.  */
#define	F_SETLK64	8	/* Set record locking info (non-blocking).  */
#define	F_SETLKW64	9	/* Set record locking info (blocking).  */

/* File descriptor flags used with F_GETFD and F_SETFD.  */
#define	FD_CLOEXEC	1	/* Close on exec.  */

//NOT SURE IF O_NONBLOCK is OK here but at least the 0x0004 flag is not used by anything else...
#define	O_NONBLOCK 0x0004 /* Non-blocking I/O.  */
//#define	O_NDELAY   O_NONBLOCK

/* Symbolic constants for the access() function */
#if !defined(F_OK)
    #define	R_OK    4       /*  Test for read permission    */
    #define	W_OK    2       /*  Test for write permission   */
    #define	X_OK    1       /*  Test for execute permission */
    #define	F_OK    0       /*  Test for existence of file  */
#endif


#ifdef _LARGEFILE_SOURCE
# define __USE_LARGEFILE 1		/* declare fseeko and ftello */
#endif

#if defined _FILE_OFFSET_BITS && _FILE_OFFSET_BITS == 64
# define __USE_FILE_OFFSET64  1	/* replace 32-bit functions by 64-bit ones */
#endif

#if __USE_LARGEFILE && __USE_FILE_OFFSET64
/* replace stat and seek by their large-file equivalents */
#undef	stat
#define	stat		_stati64

#undef	lseek
#define	lseek       _lseeki64
#define	lseek64     _lseeki64
#define	tell        _telli64
#define	tell64      _telli64

#ifdef __MINGW32__
# define fseek      fseeko64
# define fseeko     fseeko64
# define ftell      ftello64
# define ftello     ftello64
# define ftell64    ftello64
#endif /* __MINGW32__ */
#endif /* LARGE_FILES */

#ifdef USE_WINSOCK_TIMEVAL
/* Winsock timeval has long size tv_sec. */
#define __timeval timeval
#else
struct _timeval64i32 {
	time_t		tv_sec;
	long		tv_usec;
};
#define __timeval _timeval64i32
#endif

/* End of Win32 definitions. */

/* Tell libarchive code that we have simulations for these. */
#ifndef HAVE_FTRUNCATE
#define HAVE_FTRUNCATE 1
#endif
#ifndef HAVE_FUTIMES
#define HAVE_FUTIMES 1
#endif
#ifndef HAVE_UTIMES
#define HAVE_UTIMES 1
#endif
#ifndef HAVE_LINK
#define HAVE_LINK 1
#endif

/* Replacement POSIX function */
extern int	 __la_chdir(const char *path);
extern int	 __la_chmod(const char *path, mode_t mode);
extern int	 __la_fcntl(int fd, int cmd, int val);
extern int	 __la_fstat(int fd, struct stat *st);
extern int	 __la_ftruncate(int fd, off_t length);
extern int	 __la_futimes(int fd, const struct __timeval *times);
extern int	 __la_link(const char *src, const char *dst);
extern __int64	 __la_lseek(int fd, __int64 offset, int whence);
extern size_t	 __la_mbstowcs(wchar_t *wcstr, const char *mbstr, size_t nwchars);
extern int	 __la_mkdir(const char *path, mode_t mode);
extern int	 __la_open(const char *path, int flags, ...);
extern ssize_t	 __la_read(int fd, void *buf, size_t nbytes);
extern int	 __la_rmdir(const char *path);
extern int	 __la_stat(const char *path, struct stat *st);
extern int	 __la_unlink(const char *path);
extern int	 __la_utimes(const char *name, const struct __timeval *times);
extern pid_t	 __la_waitpid(pid_t wpid, int *status, int option);
extern ssize_t	 __la_write(int fd, const void *buf, size_t nbytes);

#define _stat64i32(path, st)	__la_stat(path, st)
#define _stat64(path, st)	__la_stat(path, st)
/* for status returned by la_waitpid */
#define WIFEXITED(sts)		((sts & 0x100) == 0)
#define WEXITSTATUS(sts)	(sts & 0x0FF)

#endif /* LIBARCHIVE_ARCHIVE_WINDOWS_H_INCLUDED */
