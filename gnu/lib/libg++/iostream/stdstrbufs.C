//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1992 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "ioprivate.h"
#include <stdio.h>

// This file defines the standard streambufs, corresponding to cin, cout, cerr.
// We define two sets:
//
// __std_filebuf_0, __std_filebuf_1, __std_filebuf_2 are filebufs using
// file descriptor 0/1/2.
//
// __stdin_stdiobuf, __stdout_stdiobuf, __stderr_stdiobuf are stdiostreams
// pointing to stdin, stdout, stderr.


// To avoid problems depending on constructor order (and for
// efficiency) the standard streambufs (and streams) are
// constructed statically using C-style '{ ... }' initializers.
// Since you're not allowed to do this for structs that
// have virtuals, we define fake streambuf and stream classes
// that don't have any C++-isms, and initialize those.
// To initialize the vtable field of the standard filebufs,
// we use the expression 'vt_filebuf' which must evaluate to
// (the address of) the virtual function table for the
// filebuf class.

#if _G_NAMES_HAVE_UNDERSCORE
#define UNDERSCORE "_"
#else
#define UNDERSCORE ""
#endif

// First define the filebuf-based objects.

#if !defined(vt_filebuf)
#ifndef __GNUG__
// This works for cfront.
#define vt_filebuf __vtbl__7filebuf
extern char vt_filebuf[1];
#elif _G_DOLLAR_IN_LABEL
extern char vt_filebuf[1] asm(UNDERSCORE "_vt$filebuf");
#else
extern char vt_filebuf[1] asm(UNDERSCORE "_vt.filebuf");
#endif
#endif /* !defined(vt_filebuf) */

struct _fake_filebuf {
    struct __streambuf s;
    char* vtable;
    struct __file_fields f;
};

#define FILEBUF_LITERAL(CHAIN, FLAGS) \
       { _IO_MAGIC+_S_LINKED+_S_IS_FILEBUF+_S_IS_BACKUPBUF+FLAGS, \
	 0, 0, 0, 0, 0, 0, 0, 0, CHAIN, 0, 0, 0, 0, 0}

#define DEF_FILEBUF(NAME, FD, CHAIN, FLAGS) \
  _fake_filebuf NAME = {FILEBUF_LITERAL(CHAIN, FLAGS), vt_filebuf, {FD}};

DEF_FILEBUF(__std_filebuf_0, 0, 0, _S_NO_WRITES);
DEF_FILEBUF(__std_filebuf_1, 1, (streambuf*)&__std_filebuf_0, _S_NO_READS);
DEF_FILEBUF(__std_filebuf_2, 2, (streambuf*)&__std_filebuf_1,
	    _S_NO_READS+_S_UNBUFFERED);

// Nest define the stdiobuf-bases objects.

#if !defined(vt_stdiobuf)
#ifndef __GNUG__
// This works for cfront.
#define vt_stdiobuf __vtbl__8stdiobuf
extern char vt_stdiobuf[1];
#elif _G_DOLLAR_IN_LABEL
extern char vt_stdiobuf[1] asm(UNDERSCORE "_vt$stdiobuf");
#else
extern char vt_stdiobuf[1] asm(UNDERSCORE "_vt.stdiobuf");
#endif
#endif /* !defined(vt_stdiobuf) */

struct _fake_stdiobuf {
    struct __streambuf s;
    char* vtable;
    struct __file_fields f;
    FILE *_f;
};

#define DEF_STDIOBUF(NAME, FILE, FD, CHAIN, FLAGS) \
    _fake_stdiobuf NAME[1] = {{ \
	 FILEBUF_LITERAL(CHAIN, (FLAGS)|_S_UNBUFFERED),\
	 vt_stdiobuf, {FD}, FILE}};

DEF_STDIOBUF(__stdin_stdiobuf, stdin, 0, (streambuf*)&__std_filebuf_2,
	     _S_NO_WRITES);
DEF_STDIOBUF(__stdout_stdiobuf, stdout, 1, (streambuf*)__stdin_stdiobuf,
	     _S_NO_READS);
DEF_STDIOBUF(__stderr_stdiobuf, stderr, 2, (streambuf*)__stdout_stdiobuf,
	     _S_NO_READS);

streambuf* streambuf::_list_all = (streambuf*)__stderr_stdiobuf;
