/* An interface to write() that retries after interrupts.
   Copyright (C) 2002 Free Software Foundation, Inc.

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

#include <stddef.h>

#define SAFE_WRITE_ERROR ((size_t) -1)

/* Write up to COUNT bytes at BUF to descriptor FD, retrying if interrupted.
   Return the actual number of bytes written, zero for EOF, or SAFE_WRITE_ERROR
   upon error.  */
extern size_t safe_write (int fd, const void *buf, size_t count);
