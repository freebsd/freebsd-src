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

#define _STREAM_COMPAT
#ifdef __GNUG__
#pragma implementation
#endif
#include "ioprivate.h"
#include <string.h>

void streambuf::_un_link()
{
    if (_flags & _S_LINKED) {
	streambuf **f;
	for (f = &_list_all; *f != NULL; f = &(*f)->xchain()) {
	    if (*f == this) {
		*f = xchain();
		break;
	    }
	}
	_flags &= ~_S_LINKED;
    }
}

void streambuf::_link_in()
{
    if ((_flags & _S_LINKED) == 0) {
	_flags |= _S_LINKED;
	xchain() = _list_all;
	_list_all = this;
    }
}

// Return minimum _pos markers
// Assumes the current get area is the main get area.
int streambuf::_least_marker()
{
    int least_so_far = _egptr - _eback;
    for (register streammarker *mark = _markers;
	 mark != NULL; mark = mark->_next)
	if (mark->_pos < least_so_far)
	    least_so_far = mark->_pos;
    return least_so_far;
}

// Switch current get area from backup buffer to (start of) main get area.

void streambuf::switch_to_main_get_area()
{
    char *tmp;
    _flags &= ~_S_IN_BACKUP;
    // Swap _egptr and _other_egptr.
    tmp= _egptr; _egptr= _other_egptr; _other_egptr= tmp;
    // Swap _eback and _other_gbase.	
    tmp= _eback; _eback = _other_gbase; _other_gbase = tmp;
    _gptr = _eback;
}

// Switch current get area from main get area to (end of) backup area.

void streambuf::switch_to_backup_area()
{
    char *tmp;
    _flags |= _S_IN_BACKUP;
    // Swap _egptr and _other_egptr.
    tmp = _egptr; _egptr = _other_egptr; _other_egptr = tmp;
    // Swap _gbase and _other_gbase.	
    tmp = _eback; _eback = _other_gbase; _other_gbase = tmp;
    _gptr = _egptr;
}

int streambuf::switch_to_get_mode()
{
    if (_pptr > _pbase)
	if (overflow(EOF) == EOF)
	    return EOF;
    if (in_backup()) {
	_eback = _aux_limit;
    }
    else {
	_eback = _base;
	if (_pptr > _egptr)
	    _egptr = _pptr;
    }
    _gptr = _pptr;

    setp(_gptr, _gptr);

    _flags &= ~_S_CURRENTLY_PUTTING;
    return 0;
}

void streambuf::free_backup_area()
{
    if (in_backup())
	switch_to_main_get_area();  // Just in case.
    delete [] _other_gbase;
    _other_gbase = NULL;
    _other_egptr = NULL;
    _aux_limit = NULL;
}

#if 0
int streambuf::switch_to_put_mode()
{
    _pbase = _gptr;
    _pptr = _gptr;
    _epptr = in_backup() ? _egptr : _ebuf; // wrong if line- or un-buffered?

    _gptr = _egptr;
    _eback = _egptr;

    _flags |= _S_CURRENTLY_PUTTING;
    return 0;
}
#endif

#ifdef _G_FRIEND_BUG
int __underflow(register streambuf *sb) { return __UNDERFLOW(sb); }
int __UNDERFLOW(register streambuf *sb)
#else
int __underflow(register streambuf *sb)
#endif
{
    if (sb->put_mode())
        if (sb->switch_to_get_mode() == EOF) return EOF;
    if (sb->_gptr < sb->_egptr)
	return *(unsigned char*)sb->_gptr;
    if (sb->in_backup()) {
	sb->switch_to_main_get_area();
	if (sb->_gptr < sb->_egptr)
	    return *sb->_gptr;
    }
    if (sb->have_markers()) {
	// Append [_gbase.._egptr] to backup area.
	int least_mark = sb->_least_marker();
	// needed_size is how much space we need in the backup area.
	int needed_size = (sb->_egptr - sb->_eback) - least_mark;
	int current_Bsize = sb->_other_egptr - sb->_other_gbase;
	int avail; // Extra space available for future expansion.
	if (needed_size > current_Bsize) {
	    avail = 0; // 100 ?? FIXME
	    char *new_buffer = new char[avail+needed_size];
	    if (least_mark < 0) {
		memcpy(new_buffer + avail,
		       sb->_other_egptr + least_mark,
		       -least_mark);
		memcpy(new_buffer +avail - least_mark,
		       sb->_eback,
		       sb->_egptr - sb->_eback);
	    }
	    else
		memcpy(new_buffer + avail,
		       sb->_eback + least_mark,
		       needed_size);
	    delete [] sb->_other_gbase;
	    sb->_other_gbase = new_buffer;
	    sb->_other_egptr = new_buffer + avail + needed_size;
	}
	else {
	    avail = current_Bsize - needed_size;
	    if (least_mark < 0) {
		memmove(sb->_other_gbase + avail,
			sb->_other_egptr + least_mark,
			-least_mark);
		memcpy(sb->_other_gbase + avail - least_mark,
		       sb->_eback,
		       sb->_egptr - sb->_eback);
	    }
	    else if (needed_size > 0)
		memcpy(sb->_other_gbase + avail,
		       sb->_eback + least_mark,
		       needed_size);
	}
	// FIXME: Dubious arithmetic if pointers are NULL
	sb->_aux_limit = sb->_other_gbase + avail;
	// Adjust all the streammarkers.
	int delta = sb->_egptr - sb->_eback;
	for (register streammarker *mark = sb->_markers;
	     mark != NULL; mark = mark->_next)
	    mark->_pos -= delta;
    }
    else if (sb->have_backup())
	sb->free_backup_area();
    return sb->underflow();
}

#ifdef _G_FRIEND_BUG
int __overflow(register streambuf *sb, int c) { return __OVERFLOW(sb, c); }
int __OVERFLOW(register streambuf *sb, int c)
#else
int __overflow(streambuf* sb, int c)
#endif
{
    return sb->overflow(c);
}

int streambuf::xsputn(register const char* s, int n)
{
    if (n <= 0)
	return 0;
    register int more = n;
    for (;;) {
	int count = _epptr - _pptr; // Space available.
	if (count > 0) {
	    if (count > more)
		count = more;
	    if (count > 20) {
		memcpy(_pptr, s, count);
		s += count;
		_pptr += count;
	    }
	    else if (count <= 0)
		count = 0;
	    else {
		register char *p = _pptr;
		for (register int i = count; --i >= 0; ) *p++ = *s++;
		_pptr = p;
	    }
	    more -= count;
	}
	if (more == 0 || __overflow(this, (unsigned char)*s++) == EOF)
	    break;
	more--;
    }
    return n - more;
}

int streambuf::padn(char pad, int count)
{
#define PADSIZE 16
    static char const blanks[PADSIZE] =
	 {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
    static char const zeroes[PADSIZE] =
	 {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};
    char padbuf[PADSIZE];
    const char *padptr;
    register int i;
    
    if (pad == ' ')
	padptr = blanks;
    else if (pad == '0')
	padptr = zeroes;
    else {
	for (i = PADSIZE; --i >= 0; ) padbuf[i] = pad;
	padptr = padbuf;
    }
    for (i = count; i >= PADSIZE; i -= PADSIZE)
	if (sputn(padptr, PADSIZE) != PADSIZE)
	    return EOF;
    if (i > 0 && sputn(padptr, i) != i)
	return EOF;
    return pad;
}

int streambuf::xsgetn(char* s, int n)
{
    register int more = n;
    for (;;) {
	int count = _egptr - _gptr; // Data available.
	if (count > 0) {
	    if (count > more)
		count = more;
	    if (count > 20) {
		memcpy(s, _gptr, count);
		s += count;
		_gptr += count;
	    }
	    else if (count <= 0)
		count = 0;
	    else {
		register char *p = _gptr;
		for (register int i = count; --i >= 0; ) *s++ = *p++;
		_gptr = p;
	    }
	    more -= count;
	}
	if (more == 0 || __underflow(this) == EOF)
	    break;
    }
    return n - more;
}

int streambuf::ignore(int n)
{
    register int more = n;
    for (;;) {
	int count = _egptr - _gptr; // Data available.
	if (count > 0) {
	    if (count > more)
		count = more;
	    _gptr += count;
	    more -= count;
	}
	if (more == 0 || __underflow(this) == EOF)
	    break;
    }
    return n - more;
}

int streambuf::sync()
{
    if (gptr() == egptr() && pptr() == pbase())
	return 0;
    return EOF;
}

int streambuf::pbackfail(int c)
{
    if (_gptr > _eback)
	_gptr--;
    else if (seekoff(-1, ios::cur, ios::in) == EOF)
	return EOF;
    if (c != EOF && *_gptr != c)
	*_gptr = c;
    return (unsigned char)c;
}

streambuf* streambuf::setbuf(char* p, int len)
{
    if (sync() == EOF)
	return NULL;
    if (p == NULL || len == 0) {
	unbuffered(1);
	setb(_shortbuf, _shortbuf+1, 0);
    }
    else {
	unbuffered(0);
	setb(p, p+len, 0);
    }
    setp(0, 0);
    setg(0, 0, 0);
    return this;
}

streampos streambuf::seekpos(streampos pos, int mode)
{
    return seekoff(pos, ios::beg, mode);
}

void streambuf::setb(char* b, char* eb, int a)
{
    if (_base && !(_flags & _S_USER_BUF))
	FREE_BUF(_base);
    _base = b;
    _ebuf = eb;
    if (a)
	_flags &= ~_S_USER_BUF;
    else
	_flags |= _S_USER_BUF;
}

int streambuf::doallocate()
{
    char *buf = ALLOC_BUF(_G_BUFSIZ);
    if (buf == NULL)
	return EOF;
    setb(buf, buf+_G_BUFSIZ, 1);
    return 1;
}

void streambuf::doallocbuf()
{
    if (base() || (!unbuffered() && doallocate() != EOF)) return;
    setb(_shortbuf, _shortbuf+1, 0);
}

streambuf::streambuf(int flags)
{
  _flags = _IO_MAGIC|flags;
  _base = NULL;
  _ebuf = NULL;
  _eback = NULL;
  _gptr = NULL;
  _egptr = NULL;
  _pbase = NULL;
  _pptr = NULL;
  _epptr = NULL;
  _chain = NULL; // Not necessary.

  _other_gbase = NULL;
  _aux_limit = NULL;
  _other_egptr = NULL;
  _markers = NULL;
  _cur_column = 0;
}

streambuf::~streambuf()
{
    if (_base && !(_flags & _S_USER_BUF))
	FREE_BUF(_base);

    for (register streammarker *mark = _markers;
	 mark != NULL; mark = mark->_next)
	mark->_sbuf = NULL;
    
}

streampos
streambuf::seekoff(streamoff, _seek_dir, int mode /*=ios::in|ios::out*/)
{
    return EOF;
}

int streambuf::sputbackc(char c)
{
    if (_gptr > _eback && (unsigned char)_gptr[-1] == (unsigned char)c) {
	_gptr--;
	return (unsigned char)c;
    }
    return pbackfail(c);
}

int streambuf::sungetc()
{
    if (_gptr > _eback) {
	_gptr--;
	return (unsigned char)*_gptr;
    }
    else
	return pbackfail(EOF);
}

#if 0 /* Work in progress */
void streambuf::collumn(int c)
{
    if (c == -1)
	_collumn = -1;
    else
	_collumn = c - (_pptr - _pbase);
}
#endif


int streambuf::get_column()
{
    if (_cur_column) 
	return __adjust_column(_cur_column - 1, pbase(), pptr() - pbase());
    return -1;
}

int streambuf::set_column(int i)
{
    _cur_column = i+1;
    return 0;
}

int streambuf::flush_all()
{
    int result = 0;
    for (streambuf *sb = _list_all; sb != NULL; sb = sb->xchain())
	if (sb->overflow(EOF) == EOF)
	    result = EOF;
    return result;
}

void streambuf::flush_all_linebuffered()
{
    for (streambuf *sb = _list_all; sb != NULL; sb = sb->xchain())
	if (sb->linebuffered())
	    sb->overflow(EOF);
}

int backupbuf::underflow()
{
    return EOF;
}

int backupbuf::overflow(int c)
{
    return EOF;
}

streammarker::streammarker(streambuf *sb)
{
    _sbuf = sb;
    if (!(sb->xflags() & _S_IS_BACKUPBUF)) {
	set_streampos(sb->seekoff(0, ios::cur, ios::in));
	_next = 0;
    }
    else {
	if (sb->put_mode())
	    sb->switch_to_get_mode();
	if (((backupbuf*)sb)->in_backup())
	    set_offset(sb->_gptr - sb->_egptr);
	else
	    set_offset(sb->_gptr - sb->_eback);

	// Should perhaps sort the chain?
	_next = ((backupbuf*)sb)->_markers;
	((backupbuf*)sb)->_markers = this;
    }
}

streammarker::~streammarker()
{
    if (saving()) {
	// Unlink from sb's chain.
	register streammarker **ptr = &((backupbuf*)_sbuf)->_markers;
	for (; ; ptr = &(*ptr)->_next)
	    if (*ptr == NULL)
		break;
	    else if (*ptr == this) {
		*ptr = _next;
		return;
	    }
    }
#if 0
    if _sbuf has a backup area that is no longer needed, should we delete
    it now, or wait until underflow()?
#endif
}

#define BAD_DELTA EOF

int streammarker::delta(streammarker& other_mark)
{
    if (_sbuf != other_mark._sbuf)
	return BAD_DELTA;
    if (saving() && other_mark.saving())
	return _pos - other_mark._pos;
    else if (!saving() && !other_mark.saving())
	return _spos - other_mark._spos;
    else
	return BAD_DELTA;
}

int streammarker::delta()
{
    if (_sbuf == NULL)
	return BAD_DELTA;
    if (saving()) {
	int cur_pos;
	if (_sbuf->in_backup())
	    cur_pos = _sbuf->_gptr - _sbuf->_egptr;
	else
	    cur_pos = _sbuf->_gptr - _sbuf->_eback;
	return _pos - cur_pos;
    }
    else {
	if (_spos == EOF)
	    return BAD_DELTA;
	int cur_pos = _sbuf->seekoff(0, ios::cur);
	if (cur_pos == EOF)
	    return BAD_DELTA;
	return _pos - cur_pos;
    }
}

int streambuf::seekmark(streammarker& mark, int delta /* = 0 */)
{
    if (mark._sbuf != this)
	return EOF;
    if (!mark.saving()) {
	return seekpos(mark._spos, ios::in);
    }
    else if (mark._pos >= 0) {
	if (in_backup())
	    switch_to_main_get_area();
	_gptr = _eback + mark._pos;
    }
    else {
	if (!in_backup())
	    switch_to_backup_area();
	_gptr = _egptr + mark._pos;
    }
    return 0;
}

void streambuf::unsave_markers()
{
    register streammarker *mark =_markers;
    if (_markers) {
	streampos offset = seekoff(0, ios::cur, ios::in);
	if (offset != EOF) {
	    offset += eGptr() - Gbase();
	    for ( ; mark != NULL; mark = mark->_next)
		mark->set_streampos(mark->_pos + offset);
	}
	else {
	    for ( ; mark != NULL; mark = mark->_next)
		mark->set_streampos(EOF);
	}
	_markers = 0;
    }

    free_backup_area();
}

int backupbuf::pbackfail(int c)
{
  if (_gptr <= _eback) {
    // Need to handle a filebuf in write mode (switch to read mode).  FIXME!

    if (have_backup() && !in_backup()) {
	switch_to_backup_area();
    }
    if (!have_backup()) {
	// No backup buffer: allocate one.
	// Use short buffer, if unused? (probably not)  FIXME 
	int backup_size = 128;
	_other_gbase = new char [backup_size];
	_other_egptr = _other_gbase + backup_size;
	_aux_limit = _other_egptr;
	switch_to_backup_area();
    }
    else if (gptr() <= eback()) {
	// Increase size of existing backup buffer.
	size_t new_size;
	size_t old_size = egptr() - eback();
	new_size = 2 * old_size;
	char* new_buf = new char [new_size];
	memcpy(new_buf+(new_size-old_size), eback(), old_size);
	delete [] eback();
	setg(new_buf, new_buf+(new_size-old_size), new_buf+new_size);
	_aux_limit = _gptr;
    }
  }
  _gptr--;
  if (c != EOF && *_gptr != c)
    *_gptr = c;
  return (unsigned char)*_gptr;
}

unsigned __adjust_column(unsigned start, const char *line, int count)
{
    register const char *ptr = line + count;
    while (ptr > line)
	if (*--ptr == '\n')
	    return line + count - ptr - 1;
    return start + count;
}

int ios::readable() { return !(rdbuf()->_flags & _S_NO_READS); }
int ios::writable() { return !(rdbuf()->_flags & _S_NO_WRITES); }
int ios::is_open() { return rdbuf()
			 && (rdbuf()->_flags & _S_NO_READS+_S_NO_WRITES)
			     != _S_NO_READS+_S_NO_WRITES; }

#if defined(linux)
#define IO_CLEANUP ;
#endif

#ifdef IO_CLEANUP
  IO_CLEANUP
#else
struct __io_defs {
    __io_defs() { }
    ~__io_defs() { streambuf::flush_all(); }
};   
__io_defs io_defs__;
#endif
