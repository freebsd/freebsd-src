/*
Copyright (C) 1993 Free Software Foundation

This file is part of the GNU IO Library.  This library is free
software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option)
any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

As a special exception, if you link this library with files
compiled with a GNU compiler to produce an executable, this does not cause
the resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why
the executable file might be covered by the GNU General Public License. */

#include <libio.h>
#ifdef TODO
Merge into  libio.h ?
#endif

typedef void *(*_IO_alloc_type) __P((_IO_size_t));
typedef void (*_IO_free_type) __P((void*));

struct _IO_str_fields
{
  /* The current length is max(_len, _IO_write_ptr-_IO_write_base). */
  _IO_size_t _len;
  _IO_alloc_type _allocate_buffer;
  _IO_free_type _free_buffer;
};

typedef struct _IO_strfile_
{
  struct _IO_FILE _f;
  const void *_vtable;
  struct _IO_str_fields _s;
} _IO_strfile;
