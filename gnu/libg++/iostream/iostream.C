//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1991, 1992 Per Bothner.
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

#ifdef __GNUG__
#pragma implementation
#endif
#define _STREAM_COMPAT
#include "ioprivate.h"
#include <iostream.h>
#include <stdio.h>  /* Needed for sprintf */
#include <ctype.h>
#include <limits.h>
#include "floatio.h"

#define	BUF		(MAXEXP+MAXFRACT+1)	/* + decimal point */

//#define isspace(ch) ((ch)==' ' || (ch)=='\t' || (ch)=='\n')

istream::istream(streambuf *sb, ostream* tied) : ios(sb, tied)
{
    _flags |= ios::dont_close;
    _gcount = 0;
}

int skip_ws(streambuf* sb)
{
    int ch;
    for (;;) {
	ch = sb->sbumpc();
	if (ch == EOF || !isspace(ch))
	    return ch;
    }
}

istream& istream::get(char& c)
{
    if (ipfx1()) {
	int ch = _strbuf->sbumpc();
	if (ch == EOF) {
	  set(ios::eofbit|ios::failbit);
	  _gcount = 0;
	}
	else {
	  c = (char)ch;
	  _gcount = 1;
	}
    }
    return *this;
}

int istream::peek()
{
  if (!good())
    return EOF;
  if (_tie && rdbuf()->in_avail() == 0)
    _tie->flush();
  int ch = _strbuf->sgetc();
  if (ch == EOF)
    set(ios::eofbit);
  return ch;
}

istream& istream::ignore(int n /* = 1 */, int delim /* = EOF */)
{
    if (ipfx1()) {
	register streambuf* sb = _strbuf;
	if (delim == EOF) {
	    _gcount = sb->ignore(n);
	    return *this;
	}
	_gcount = 0;
	for (;;) {
#if 0
	    if (n != MAXINT) // FIXME
#endif
	    if (--n < 0)
		break;
	    int ch = sb->sbumpc();
	    if (ch == EOF) {
		set(ios::eofbit|ios::failbit);
		break;
	    }
	    _gcount++;
	    if (ch == delim)
		break;
	}
    }
    return *this;
}

istream& istream::read(char *s, int n)
{
    if (ipfx1()) {
	_gcount = _strbuf->sgetn(s, n);
	if (_gcount != n)
	    set(ios::failbit);
    }
    return *this;
}

istream& istream::seekg(streampos pos)
{
    pos = _strbuf->seekpos(pos, ios::in);
    if (pos == streampos(EOF))
	set(ios::badbit);
    return *this;
}

istream& istream::seekg(streamoff off, _seek_dir dir)
{
    streampos pos = _strbuf->seekoff(off, dir, ios::in);
    if (pos == streampos(EOF))
	set(ios::badbit);
    return *this;
}

streampos istream::tellg()
{
    streampos pos = _strbuf->seekoff(0, ios::cur, ios::in);
    if (pos == streampos(EOF))
	set(ios::badbit);
    return pos;
}

istream& istream::scan(const char *format ...)
{
    if (ipfx0()) {
	va_list ap;
	va_start(ap, format);
	_strbuf->vscan(format, ap, this);
	va_end(ap);
    }
    return *this;
}

istream& istream::vscan(const char *format, _G_va_list args)
{
    if (ipfx0())
	_strbuf->vscan(format, args, this);
    return *this;
}

istream& istream::operator>>(char& c)
{
    if (ipfx0()) {
	int ch = _strbuf->sbumpc();
	if (ch == EOF)
	    set(ios::eofbit|ios::failbit);
	else
	    c = (char)ch;
    }
    return *this;
}

istream& istream::operator>>(char* ptr)
{
  register char *p = ptr;
  int w = width(0);
  if (ipfx0()) {
    register streambuf* sb = _strbuf;
    for (;;)
      {
	int ch = sb->sbumpc();
	if (ch == EOF)
	  {
	    set(p == ptr ? (ios::eofbit|ios::failbit) : (ios::eofbit));
	    break;
	  }
	else if (isspace(ch))
	  {
	    sb->sputbackc(ch);
	    break;
	  }
	else if (w == 1)
	  {
	    set(ios::failbit);
	    sb->sputbackc(ch);
	    break;
	  }
	else *p++ = ch;
	w--;
      }
  }
  *p = '\0';
  return *this;
}

#ifdef __GNUC__
#define LONGEST long long
#else
#define LONGEST long
#endif

static int read_int(istream& stream, unsigned LONGEST& val, int& neg)
{
    if (!stream.ipfx0())
	return 0;
    register streambuf* sb = stream.rdbuf();
    int base = 10;
    int ndigits = 0;
    register int ch = skip_ws(sb);
    if (ch == EOF)
	goto eof_fail;
    neg = 0;
    if (ch == '+') {
	ch = skip_ws(sb);
    }
    else if (ch == '-') {
	neg = 1;
	ch = skip_ws(sb);
    }
    if (ch == EOF) goto eof_fail;
    if (!(stream.flags() & ios::basefield)) {
	if (ch == '0') {
	    ch = sb->sbumpc();
	    if (ch == EOF) {
		val = 0;
		return 1;
	    }
	    if (ch == 'x' || ch == 'X') {
		base = 16;
		ch = sb->sbumpc();
		if (ch == EOF) goto eof_fail;
	    }
	    else {
		sb->sputbackc(ch);
		base = 8;
		ch = '0';
	    }
	}
    }
    else if ((stream.flags() & ios::basefield) == ios::hex)
	base = 16;
    else if ((stream.flags() & ios::basefield) == ios::oct)
	base = 8;
    val = 0;
    for (;;) {
	if (ch == EOF)
	    break;
	int digit;
	if (ch >= '0' && ch <= '9')
	    digit = ch - '0';
	else if (ch >= 'A' && ch <= 'F')
	    digit = ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
	    digit = ch - 'a' + 10;
	else
	    digit = 999;
	if (digit >= base) {
	    sb->sputbackc(ch);
	    if (ndigits == 0)
		goto fail;
	    else
		return 1;
	}
	ndigits++;
	val = base * val + digit;
	ch = sb->sbumpc();
    }
    return 1;
  fail:
    stream.set(ios::failbit);
    return 0;
  eof_fail:
    stream.set(ios::failbit|ios::eofbit);
    return 0;
}

#define READ_INT(TYPE) \
istream& istream::operator>>(TYPE& i)\
{\
    unsigned LONGEST val; int neg;\
    if (read_int(*this, val, neg)) {\
	if (neg) val = -val;\
	i = (TYPE)val;\
    }\
    return *this;\
}

READ_INT(short)
READ_INT(unsigned short)
READ_INT(int)
READ_INT(unsigned int)
READ_INT(long)
READ_INT(unsigned long)
#ifdef __GNUG__
READ_INT(long long)
READ_INT(unsigned long long)
#endif

istream& istream::operator>>(double& x)
{
    if (ipfx0())
	scan("%lg", &x);
    return *this;
}

istream& istream::operator>>(float& x)
{
    if (ipfx0())
	scan("%g", &x);
    return *this;
}

istream& istream::operator>>(register streambuf* sbuf)
{
    if (ipfx0()) {
	register streambuf* inbuf = rdbuf();
	// FIXME: Should optimize!
	for (;;) {
	    register int ch = inbuf->sbumpc();
	    if (ch == EOF) {
		set(ios::eofbit);
		break;
	    }
	    if (sbuf->sputc(ch) == EOF) {
		set(ios::failbit);
		break;
	    }
	}
    }
    return *this;
}

ostream& ostream::operator<<(char c)
{
    if (opfx()) {
#if 1
	// This is what the cfront implementation does.
	_strbuf->sputc(c);
#else
	// This is what cfront documentation and current ANSI drafts say.
	int w = width(0);
	char fill_char = fill();
	register int padding = w > 0 ? w - 1 : 0;
	register streambuf *sb = _strbuf;
	if (!(flags() & ios::left)) // Default adjustment.
	    while (--padding >= 0) sb->sputc(fill_char);
	sb->sputc(c);
	if (flags() & ios::left) // Left adjustment.
	    while (--padding >= 0) sb->sputc(fill_char);
#endif
	osfx();
    }
    return *this;
}

/* Write VAL on STREAM.
   If SIGN<0, val is the absolute value of a negative number.
   If SIGN>0, val is a signed non-negative number.
   If SIGN==0, val is unsigned. */

static void write_int(ostream& stream, unsigned LONGEST val, int sign)
{
#define WRITE_BUF_SIZE (10 + sizeof(unsigned LONGEST) * 3)
    char buf[WRITE_BUF_SIZE];
    register char *buf_ptr = buf+WRITE_BUF_SIZE; // End of buf.
    char *show_base = "";
    int show_base_len = 0;
    int show_pos = 0; // If 1, print a '+'.

    // Now do the actual conversion, placing the result at the *end* of buf.
    // Note that we use separate code for decimal, octal, and hex,
    // so we can divide by optimizable constants.
    if ((stream.flags() & ios::basefield) == ios::oct) { // Octal
	do {
	    *--buf_ptr = (val & 7) + '0';
	    val = val >> 3;
	} while (val != 0);
	if ((stream.flags() & ios::showbase) && (val != 0))
	    *--buf_ptr = '0';
    }
    else if ((stream.flags() & ios::basefield) == ios::hex) { // Hex
	char *xdigs = (stream.flags() & ios::uppercase) ? "0123456789ABCDEF0X"
	    : "0123456789abcdef0x";
	do {
	    *--buf_ptr = xdigs[val & 15];
	    val = val >> 4;
	} while (val != 0);
	if ((stream.flags() & ios::showbase) && (val != 0)) {
	    show_base = xdigs + 16; // Either "0X" or "0x".
	    show_base_len = 2;
	}
    }
    else { // Decimal
#ifdef __GNUC__
	// Optimization:  Only use long long when we need to.
	while (val > UINT_MAX) {
	    *--buf_ptr = (val % 10) + '0';
	    val /= 10;
	}
	// Use more efficient (int) arithmetic for the rest.
	register unsigned int ival = (unsigned int)val;
#else
	register unsigned LONGEST ival = val;
#endif
	do {
	    *--buf_ptr = (ival % 10) + '0';
	    ival /= 10;
	} while (ival != 0);
	if (sign > 0 && (stream.flags() & ios::showpos))
	    show_pos=1;
    }

    int buf_len = buf+WRITE_BUF_SIZE - buf_ptr;
    int w = stream.width(0);

    // Calculate padding.
    int len = buf_len+show_pos;
    if (sign < 0) len++;
    len += show_base_len;
    int padding = len > w ? 0 : w - len;

    // Do actual output.
    register streambuf* sbuf = stream.rdbuf();
    ios::fmtflags pad_kind =
	stream.flags() & (ios::left|ios::right|ios::internal);
    char fill_char = stream.fill();
    if (padding > 0
	&& pad_kind != (ios::fmtflags)ios::left
	&& pad_kind != (ios::fmtflags)ios::internal) // Default (right) adjust.
	sbuf->padn(fill_char, padding);
    if (sign < 0) sbuf->sputc('-');
    else if (show_pos) sbuf->sputc('+');
    if (show_base_len)
	sbuf->sputn(show_base, show_base_len);
    if (pad_kind == (ios::fmtflags)ios::internal && padding > 0)
	sbuf->padn(fill_char, padding);
    sbuf->sputn(buf_ptr, buf_len);
    if (pad_kind == (ios::fmtflags)ios::left && padding > 0) // Left adjustment
	sbuf->padn(fill_char, padding);
    stream.osfx();
}

ostream& ostream::operator<<(int n)
{
    if (opfx()) {
	int sign = 1;
	if (n < 0 && (flags() & (ios::oct|ios::hex)) == 0)
	    n = -n, sign = -1;
	write_int(*this, n, sign);
    }
    return *this;
}

ostream& ostream::operator<<(unsigned int n)
{
    if (opfx())
	write_int(*this, n, 0);
    return *this;
}


ostream& ostream::operator<<(long n)
{
    if (opfx()) {
	int sign = 1;
	if (n < 0 && (flags() & (ios::oct|ios::hex)) == 0)
	    n = -n, sign = -1;
	write_int(*this, n, sign);
    }
    return *this;
}

ostream& ostream::operator<<(unsigned long n)
{
    if (opfx())
	write_int(*this, n, 0);
    return *this;
}

#ifdef __GNUG__
ostream& ostream::operator<<(long long n)
{
    if (opfx()) {
	int sign = 1;
	if (n < 0 && (flags() & (ios::oct|ios::hex)) == 0)
	    n = -n, sign = -1;
	write_int(*this, n, sign);
    }
    return *this;
}


ostream& ostream::operator<<(unsigned long long n)
{
    if (opfx())
	write_int(*this, n, 0);
    return *this;
}
#endif /*__GNUG__*/

ostream& ostream::operator<<(double n)
{
    if (opfx()) {
	// Uses __cvt_double (renamed from static cvt), in Chris Torek's
	// stdio implementation.  The setup code uses the same logic
	// as in __vsbprintf.C (also based on Torek's code).
	int format_char;
#if 0
	if (flags() ios::showpos) sign = '+';
#endif
	if ((flags() & ios::floatfield) == ios::fixed)
	    format_char = 'f';
	else if ((flags() & ios::floatfield) == ios::scientific)
	    format_char = flags() & ios::uppercase ? 'E' : 'e';
	else
	    format_char = flags() & ios::uppercase ? 'G' : 'g';

	int fpprec = 0; // 'Extra' (suppressed) floating precision.
	int prec = precision();
	if (prec > MAXFRACT) {
	    if (flags() & (ios::fixed|ios::scientific) & ios::showpos)
		fpprec = prec - MAXFRACT;
	    prec = MAXFRACT;
	}
	else if (prec <= 0 && !(flags() & ios::fixed))
	  prec = 6; /* default */

	// Do actual conversion.
#ifdef USE_DTOA
	if (__outfloat(n, rdbuf(), format_char, width(0),
		       prec, flags(), 0, fill()) < 0)
	    set(ios::badbit|ios::failbit); // ??
#else
	int negative;
	char buf[BUF];
	int sign = '\0';
	char *cp = buf;
	*cp = 0;
	int size = __cvt_double(n, prec,
				flags() & ios::showpoint ? 0x80 : 0,
				&negative,
				format_char, cp, buf + sizeof(buf));
	if (negative) sign = '-';
	if (*cp == 0)
	    cp++;

	// Calculate padding.
	int fieldsize = size + fpprec;
	if (sign) fieldsize++;
	int padding = 0;
	int w = width(0);
	if (fieldsize < w)
	    padding = w - fieldsize;

	// Do actual output.
	register streambuf* sbuf = rdbuf();
	register i;
	char fill_char = fill();
	ios::fmtflags pad_kind =
	    flags() & (ios::left|ios::right|ios::internal);
	if (pad_kind != (ios::fmtflags)ios::left // Default (right) adjust.
	    && pad_kind != (ios::fmtflags)ios::internal)
	    for (i = padding; --i >= 0; ) sbuf->sputc(fill_char);
	if (sign)
	    sbuf->sputc(sign);
	if (pad_kind == (ios::fmtflags)ios::internal)
	    for (i = padding; --i >= 0; ) sbuf->sputc(fill_char);
	
	// Emit the actual concented field, followed by extra zeros.
	sbuf->sputn(cp, size);
	for (i = fpprec; --i >= 0; ) sbuf->sputc('0');

	if (pad_kind == (ios::fmtflags)ios::left) // Left adjustment
	    for (i = padding; --i >= 0; ) sbuf->sputc(fill_char);
#endif
	osfx();
    }
    return *this;
}

ostream& ostream::operator<<(const char *s)
{
    if (opfx()) {
	if (s == NULL)
	    s = "(null)";
	int len = strlen(s);
	int w = width(0);
	char fill_char = fill();
	register streambuf *sbuf = rdbuf();
	register int padding = w > len ? w - len : 0;
	if (!(flags() & ios::left)) // Default adjustment.
	    while (--padding >= 0) sbuf->sputc(fill_char);
	sbuf->sputn(s, len);
	if (flags() & ios::left) // Left adjustment.
	    while (--padding >= 0) sbuf->sputc(fill_char);
	osfx();
    }
    return *this;
}

ostream& ostream::operator<<(const void *p)
{
    if (opfx()) {
	form("%p", p);
	osfx();
    }
    return *this;
}

ostream& ostream::operator<<(register streambuf* sbuf)
{
    if (opfx()) {
	register streambuf* outbuf = rdbuf();
	// FIXME: Should optimize!
	for (;;) {
	    register int ch = sbuf->sbumpc();
	    if (ch == EOF) break;
	    if (outbuf->sputc(ch) == EOF) {
		set(ios::badbit);
		break;
	    }
	}
	osfx();
    }
    return *this;
}

ostream::ostream(streambuf* sb, ostream* tied) : ios(sb, tied)
{
    _flags |= ios::dont_close;
}

ostream& ostream::seekp(streampos pos)
{
    pos = _strbuf->seekpos(pos, ios::out);
    if (pos == streampos(EOF))
	set(ios::badbit);
    return *this;
}

ostream& ostream::seekp(streamoff off, _seek_dir dir)
{
    streampos pos = _strbuf->seekoff(off, dir, ios::out);
    if (pos == streampos(EOF))
	set(ios::badbit);
    return *this;
}

streampos ostream::tellp()
{
    streampos pos = _strbuf->seekoff(0, ios::cur, ios::out);
    if (pos == streampos(EOF))
	set(ios::badbit);
    return pos;
}

ostream& ostream::form(const char *format ...)
{
    if (opfx()) {
	va_list ap;
	va_start(ap, format);
	_strbuf->vform(format, ap);
	va_end(ap);
    }
    return *this;
}

ostream& ostream::vform(const char *format, _G_va_list args)
{
    if (opfx())
	_strbuf->vform(format, args);
    return *this;
}

ostream& ostream::flush()
{
    if (_strbuf->sync())
	set(ios::badbit);
    return *this;
}

ostream& flush(ostream& outs)
{
  return outs.flush();
}

istream& ws(istream& ins)
{
    if (ins.ipfx1()) {
	int ch = skip_ws(ins._strbuf);
	if (ch == EOF)
	    ins.set(ios::eofbit);
	else
	    ins._strbuf->sputbackc(ch);
    }
    return ins;
}

// Skip white-space.  Return 0 on failure (EOF), or 1 on success.
// Differs from ws() manipulator in that failbit is set on EOF.
// Called by ipfx() and ipfx0() if needed.

int istream::_skip_ws()
{
    int ch = skip_ws(_strbuf);
    if (ch == EOF) {
	set(ios::eofbit|ios::failbit);
	return 0;
    }
    else {
	_strbuf->sputbackc(ch);
	return 1;
    }
}

ostream& ends(ostream& outs)
{
    outs.put('\0');
    return outs;
}

ostream& endl(ostream& outs)
{
    return flush(outs.put('\n'));
}

ostream& ostream::write(const char *s, int n)
{
    if (opfx()) {
	if (_strbuf->sputn(s, n) != n)
	    set(ios::failbit);
    }
    return *this;
}

void ostream::do_osfx()
{
    if (flags() & ios::unitbuf)
	flush();
    if (flags() & ios::stdio) {
	fflush(stdout);
	fflush(stderr);
    }
}

iostream::iostream(streambuf* sb, ostream* tied) : ios(sb, tied)
{
    _flags |= ios::dont_close;
    _gcount = 0;
}

// NOTE: extension for compatibility with old libg++.
// Not really compatible with fistream::close().
#ifdef _STREAM_COMPAT
void ios::close()
{
    if (!(_flags & (unsigned int)ios::dont_close))
	delete _strbuf;
    else if (_strbuf->_flags & _S_IS_FILEBUF)
	((struct filebuf*)_strbuf)->close();
    else if (_strbuf != NULL)
	_strbuf->sync();
    _flags |= ios::dont_close;
    _strbuf = NULL;
    _state = badbit;
}

int istream::skip(int i)
{
    int old = (_flags & ios::skipws) != 0;
    if (i)
	_flags |= ios::skipws;
    else
	_flags &= ~ios::skipws;
    return old;
}
#endif
