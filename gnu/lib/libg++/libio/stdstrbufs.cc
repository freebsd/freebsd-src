/* 
Copyright (C) 1994 Free Software Foundation

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


/* This file provides definitions of _IO_stdin, _IO_stdout, and _IO_stderr
   for C++ code.  Compare stdfiles.c.
   (The difference is that here the vtable field is set to
   point to builtinbuf's vtable, so the objects are effectively
   of class builtinbuf.) */

#include "libioP.h"
#include <stdio.h>

#ifndef STD_VTABLE
#define STD_VTABLE builtinbuf_vtable
#endif

#define DEF_STDFILE(NAME, FD, CHAIN, FLAGS) \
  struct _IO_FILE_plus NAME = {FILEBUF_LITERAL(CHAIN, FLAGS, FD), STD_VTABLE}

DEF_STDFILE(_IO_stdin_, 0, 0, _IO_NO_WRITES);
DEF_STDFILE(_IO_stdout_, 1, &_IO_stdin_.file, _IO_NO_READS);
DEF_STDFILE(_IO_stderr_, 2, &_IO_stdout_.file,
            _IO_NO_READS+_IO_UNBUFFERED);

#ifdef _STDIO_USES_IOSTREAM
_IO_FILE *_IO_list_all = &_IO_stderr_.file;
#else /* !_STDIO_USES_IOSTREAM */
#include "stdiostream.h"

struct _IO_fake_stdiobuf {
  _IO_FILE file;
  const void *vtable;
  FILE *stdio_file;
};

/* Define stdiobuf_vtable as a name for the virtual function table
   of the stdiobuf class. */
#ifndef stdiobuf_vtable
#ifdef __GNUC__
extern char stdiobuf_vtable[]
  asm (_G_VTABLE_LABEL_PREFIX
#if _G_VTABLE_LABEL_HAS_LENGTH
       "8"
#endif
       "stdiobuf");
#else /* !__GNUC__ */
#if _G_VTABLE_LABEL_HAS_LENGTH
#define stdiobuf_vtable _G_VTABLE_LABEL_PREFIX_ID##8stdiobuf
#else
#define stdiobuf_vtable _G_VTABLE_LABEL_PREFIX_ID##stdiobuf
#endif
extern char stdiobuf_vtable[];
#endif /* !__GNUC__ */
#endif /* !stdiobuf_vtable */

#define DEF_STDIOFILE(NAME, FD, FILE, FLAGS, CHAIN) \
  struct _IO_fake_stdiobuf NAME = \
      {{ _IO_MAGIC+_IO_LINKED+_IO_IS_FILEBUF+_IO_UNBUFFERED+FLAGS, \
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CHAIN, &_IO_streambuf_jumps, FD},\
         stdiobuf_vtable, FILE}

DEF_STDIOFILE(_IO_stdin_buf, 0, stdin, _IO_NO_WRITES, &_IO_stderr_.file);
DEF_STDIOFILE(_IO_stdout_buf, 1, stdout, _IO_NO_READS, &_IO_stdin_buf.file);
DEF_STDIOFILE(_IO_stderr_buf, 2, stderr, _IO_NO_READS, &_IO_stdout_buf.file);

_IO_FILE *_IO_list_all = &_IO_stderr_buf.file;
#endif  /* !_STDIO_USES_IOSTREAM */
