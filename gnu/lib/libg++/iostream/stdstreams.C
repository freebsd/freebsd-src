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

// The ANSI draft requires that operations on cin/cout/cerr can be
// mixed with operations on stdin/stdout/stderr on a character by
// character basis.  This normally requires that the streambuf's
// used by cin/cout/cerr be stdiostreams.  However, if the stdio
// implementation is the one that is built using this library,
// then we don't need to, since in that case stdin/stdout/stderr
// are identical to &__std_filebuf_0/&__std_filebuf_1/&__std_filebuf_2.

#ifdef _STDIO_USES_IOSTREAM
#define USE_FILEBUF
#endif

#ifdef NAMES_HAVE_UNDERSCORE
#define UNDERSCORE "_"
#else
#define UNDERSCORE ""
#endif

#ifdef USE_FILEBUF
#define CIN_SBUF __std_filebuf_0
#define COUT_SBUF __std_filebuf_1
#define CERR_SBUF __std_filebuf_2
static int use_stdiobuf = 0;
#else
#define CIN_SBUF __stdin_stdiobuf
#define COUT_SBUF __stdout_stdiobuf
#define CERR_SBUF __stderr_stdiobuf
static int use_stdiobuf = 1;
#endif

struct _fake_filebuf;
extern _fake_filebuf __std_filebuf_0, __std_filebuf_1, __std_filebuf_2;
struct _fake_stdiobuf;
extern _fake_stdiobuf __stdin_stdiobuf, __stdout_stdiobuf, __stderr_stdiobuf;

#define cin CIN
#define cout COUT
#define cerr CERR
#define clog CLOG
#include "iostream.h"
#undef cin
#undef cout
#undef cerr
#undef clog

#ifdef __GNUC__
#define PAD 0 /* g++ allows 0-length arrays. */
#else
#define PAD 1
#endif
struct _fake_istream {
    struct myfields {
#ifdef __GNUC__
	_ios_fields *vb; /* pointer to virtual base class ios */
	_G_ssize_t _gcount;
#else
	/* This is supposedly correct for cfront. */
	_G_ssize_t _gcount;
	void *vptr;
	_ios_fields *vb; /* pointer to virtual base class ios */
#endif
    } mine;
    _ios_fields base;
    char filler[sizeof(struct istream)-sizeof(struct _ios_fields)+PAD];
};
struct _fake_ostream {
    struct myfields {
#ifndef __GNUC__
	void *vptr;
#endif
	_ios_fields *vb; /* pointer to virtual base class ios */
    } mine;
    _ios_fields base;
    char filler[sizeof(struct ostream)-sizeof(struct _ios_fields)+PAD];
};

#define STD_STR(SBUF, TIE, EXTRA_FLAGS) \
 (streambuf*)&SBUF, TIE, 0, ios::dont_close|ios::skipws|EXTRA_FLAGS, ' ',0,0,6

#ifdef __GNUC__
#define OSTREAM_DEF(TYPE, NAME, SBUF, TIE, EXTRA_FLAGS) \
  TYPE NAME = { {&NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#define ISTREAM_DEF(TYPE, NAME, SBUF, TIE, EXTRA_FLAGS) \
  TYPE NAME = { {&NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#else
#define OSTREAM_DEF(TYPE, NAME, SBUF, TIE, EXTRA_FLAGS) \
  TYPE NAME = { {0, &NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#define ISTREAM_DEF(TYPE, NAME, SBUF, TIE, EXTRA_FLAGS) \
  TYPE NAME = { {0, 0, &NAME.base}, {STD_STR(SBUF, TIE, EXTRA_FLAGS) }};
#endif

OSTREAM_DEF(_fake_ostream, cout, COUT_SBUF, NULL, 0)
OSTREAM_DEF(_fake_ostream, cerr, CERR_SBUF, (ostream*)&cout, ios::unitbuf)
ISTREAM_DEF(_fake_istream, cin, CIN_SBUF,  (ostream*)&cout, 0)

/* Only for (partial) compatibility with AT&T's library. */
OSTREAM_DEF(_fake_ostream, clog, CERR_SBUF, (ostream*)&cout, 0)

// Switches between using __std_filebuf_{0,1,2} and
// __std{in,out,err}_stdiobuf for standard streams.  This is
// normally not needed, but is provided for AT&T compatibility.

int ios::sync_with_stdio(int new_state)
{
#ifdef _STDIO_USES_IOSTREAM
    // It is always synced.
    return 0;
#else
    if (new_state == use_stdiobuf) // The usual case now.
	return use_stdiobuf;
    if (new_state) {
	cout.base._strbuf = (streambuf*)&__stdout_stdiobuf;
	cin.base._strbuf = (streambuf*)&__stdin_stdiobuf;
	cerr.base._strbuf = (streambuf*)&__stderr_stdiobuf;
	clog.base._strbuf = (streambuf*)&__stderr_stdiobuf;
    } else {
	cout.base._strbuf = (streambuf*)&__std_filebuf_1;
	cin.base._strbuf = (streambuf*)&__std_filebuf_0;
	cerr.base._strbuf = (streambuf*)&__std_filebuf_2;
	clog.base._strbuf = (streambuf*)&__std_filebuf_2;
    }
    int old_state = use_stdiobuf;
    use_stdiobuf = new_state;
    return old_state;
#endif
}
