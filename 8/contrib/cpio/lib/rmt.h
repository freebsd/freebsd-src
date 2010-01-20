/* Definitions for communicating with a remote tape drive.

   Copyright (C) 1988, 1992, 1996, 1997, 2001, 2003, 2004 Free
   Software Foundation, Inc.

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

extern char *rmt_command;
extern char *rmt_dev_name__;

int rmt_open__ (const char *, int, int, const char *);
int rmt_close__ (int);
size_t rmt_read__ (int, char *, size_t);
size_t rmt_write__ (int, char *, size_t);
off_t rmt_lseek__ (int, off_t, int);
int rmt_ioctl__ (int, int, char *);

extern bool force_local_option;

/* A filename is remote if it contains a colon not preceded by a slash,
   to take care of `/:/' which is a shorthand for `/.../<CELL-NAME>/fs'
   on machines running OSF's Distributing Computing Environment (DCE) and
   Distributed File System (DFS).  However, when --force-local, a
   filename is never remote.  */

#define _remdev(dev_name) \
  (!force_local_option && (rmt_dev_name__ = strchr (dev_name, ':')) \
   && rmt_dev_name__ > (dev_name) \
   && ! memchr (dev_name, '/', rmt_dev_name__ - (dev_name)))

#define _isrmt(fd) \
  ((fd) >= __REM_BIAS)

#define __REM_BIAS (1 << 30)

#ifndef O_CREAT
# define O_CREAT 01000
#endif

#define rmtopen(dev_name, oflag, mode, command) \
  (_remdev (dev_name) ? rmt_open__ (dev_name, oflag, __REM_BIAS, command) \
   : open (dev_name, oflag, mode))

#define rmtaccess(dev_name, amode) \
  (_remdev (dev_name) ? 0 : access (dev_name, amode))

#define rmtstat(dev_name, buffer) \
  (_remdev (dev_name) ? (errno = EOPNOTSUPP), -1 : stat (dev_name, buffer))

#define rmtcreat(dev_name, mode, command) \
   (_remdev (dev_name) \
    ? rmt_open__ (dev_name, 1 | O_CREAT, __REM_BIAS, command) \
    : creat (dev_name, mode))

#define rmtlstat(dev_name, muffer) \
  (_remdev (dev_name) ? (errno = EOPNOTSUPP), -1 : lstat (dev_name, buffer))

#define rmtread(fd, buffer, length) \
  (_isrmt (fd) ? rmt_read__ (fd - __REM_BIAS, buffer, length) \
   : safe_read (fd, buffer, length))

#define rmtwrite(fd, buffer, length) \
  (_isrmt (fd) ? rmt_write__ (fd - __REM_BIAS, buffer, length) \
   : full_write (fd, buffer, length))

#define rmtlseek(fd, offset, where) \
  (_isrmt (fd) ? rmt_lseek__ (fd - __REM_BIAS, offset, where) \
   : lseek (fd, offset, where))

#define rmtclose(fd) \
  (_isrmt (fd) ? rmt_close__ (fd - __REM_BIAS) : close (fd))

#define rmtioctl(fd, request, argument) \
  (_isrmt (fd) ? rmt_ioctl__ (fd - __REM_BIAS, request, argument) \
   : ioctl (fd, request, argument))

#define rmtdup(fd) \
  (_isrmt (fd) ? (errno = EOPNOTSUPP), -1 : dup (fd))

#define rmtfstat(fd, buffer) \
  (_isrmt (fd) ? (errno = EOPNOTSUPP), -1 : fstat (fd, buffer))

#define rmtfcntl(cd, command, argument) \
  (_isrmt (fd) ? (errno = EOPNOTSUPP), -1 : fcntl (fd, command, argument))

#define rmtisatty(fd) \
  (_isrmt (fd) ? 0 : isatty (fd))
