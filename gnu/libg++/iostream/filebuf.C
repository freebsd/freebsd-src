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

#include "ioprivate.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

// An fstream can be in at most one of put mode, get mode, or putback mode.
// Putback mode is a variant of get mode.

// In a filebuf, there is only one current position, instead of two
// separate get and put pointers.  In get mode, the current posistion
// is that of gptr(); in put mode that of pptr().

// The position in the buffer that corresponds to the position
// in external file system is file_ptr().
// This is normally egptr(), except in putback mode, when it is _save_egptr.
// If the field _fb._offset is >= 0, it gives the offset in
// the file as a whole corresponding to eGptr(). (???)

// PUT MODE:
// If a filebuf is in put mode, pbase() is non-NULL and equal to base().
// Also, epptr() == ebuf().
// Also, eback() == gptr() && gptr() == egptr().
// The un-flushed character are those between pbase() and pptr().
// GET MODE:
// If a filebuf is in get or putback mode, eback() != egptr().
// In get mode, the unread characters are between gptr() and egptr().
// The OS file position corresponds to that of egptr().
// PUTBACK MODE:
// Putback mode is used to remember "excess" characters that have
// been sputbackc'd in a separate putback buffer.
// In putback mode, the get buffer points to the special putback buffer.
// The unread characters are the characters between gptr() and egptr()
// in the putback buffer, as well as the area between save_gptr()
// and save_egptr(), which point into the original reserve buffer.
// (The pointers save_gptr() and save_egptr() are the values
// of gptr() and egptr() at the time putback mode was entered.)
// The OS position corresponds to that of save_egptr().
//
// LINE BUFFERED OUTPUT:
// During line buffered output, pbase()==base() && epptr()==base().
// However, ptr() may be anywhere between base() and ebuf().
// This forces a call to filebuf::overflow(int C) on every put.
// If there is more space in the buffer, and C is not a '\n',
// then C is inserted, and pptr() incremented.
//
// UNBUFFERED STREAMS:
// If a filebuf is unbuffered(), the _shortbuf[1] is used as the buffer.

#define CLOSED_FILEBUF_FLAGS \
  (_S_IS_FILEBUF+_S_NO_READS+_S_NO_WRITES+_S_TIED_PUT_GET)

void filebuf::init()
{
    _fb._offset = 0;

    _link_in();
    _fb._fileno = -1;
}

filebuf::filebuf() : backupbuf(CLOSED_FILEBUF_FLAGS)
{
    init();
}

filebuf::filebuf(int fd) : backupbuf(CLOSED_FILEBUF_FLAGS)
{
    init();
    attach(fd);
}

filebuf::filebuf(int fd, char* p, int len) : backupbuf(CLOSED_FILEBUF_FLAGS)
{
    init();
    attach(fd);
    setbuf(p, len);
}

filebuf::~filebuf()
{
    if (!(xflags() & _S_DELETE_DONT_CLOSE))
	close();

    _un_link();
}

filebuf* filebuf::open(const char *filename, ios::openmode mode, int prot)
{
    if (is_open())
	return NULL;
    int posix_mode;
    int read_write;
    if (mode & ios::app)
	mode |= ios::out;
    if ((mode & (ios::in|ios::out)) == (ios::in|ios::out)) {
	posix_mode = O_RDWR;
	read_write = 0;
    }
    else if (mode & ios::out)
	posix_mode = O_WRONLY, read_write = _S_NO_READS;
    else if (mode & (int)ios::in)
	posix_mode = O_RDONLY, read_write = _S_NO_WRITES;
    else
	posix_mode = 0, read_write = _S_NO_READS+_S_NO_WRITES;
    if ((mode & (int)ios::trunc) || mode == (int)ios::out)
	posix_mode |= O_TRUNC;
    if (mode & ios::app)
	posix_mode |= O_APPEND, read_write |= _S_IS_APPENDING;
    if (!(mode & (int)ios::nocreate) && mode != ios::in)
	posix_mode |= O_CREAT;
    if (mode & (int)ios::noreplace)
	posix_mode |= O_EXCL;
    int fd = ::open(filename, posix_mode, prot);
    if (fd < 0)
	return NULL;
    _fb._fileno = fd;
    xsetflags(read_write, _S_NO_READS+_S_NO_WRITES+_S_IS_APPENDING);
    if (mode & (ios::ate|ios::app)) {
	if (seekoff(0, ios::end) == EOF)
	    return NULL;
    }
    _link_in();
    return this;
}

filebuf* filebuf::open(const char *filename, const char *mode)
{
    if (is_open())
	return NULL;
    int oflags = 0, omode;
    int read_write;
    int oprot = 0666;
    switch (*mode++) {
      case 'r':
	omode = O_RDONLY;
	read_write = _S_NO_WRITES;
	break;
      case 'w':
	omode = O_WRONLY;
	oflags = O_CREAT|O_TRUNC;
	read_write = _S_NO_READS;
	break;
      case 'a':
	omode = O_WRONLY;
	oflags = O_CREAT|O_APPEND;
	read_write = _S_NO_READS|_S_IS_APPENDING;
	break;
      default:
	errno = EINVAL;
	return NULL;
    }
    if (mode[0] == '+' || (mode[0] == 'b' && mode[1] == '+')) {
	omode = O_RDWR;
	read_write &= _S_IS_APPENDING;
    }
    int fdesc = ::open(filename, omode|oflags, oprot);
    if (fdesc < 0)
	return NULL;
    _fb._fileno = fdesc;
    xsetflags(read_write, _S_NO_READS+_S_NO_WRITES+_S_IS_APPENDING);
    if (read_write & _S_IS_APPENDING)
	if (seekoff(0, ios::end) == EOF)
	    return NULL;
    _link_in();
    return this;
}

filebuf* filebuf::attach(int fd)
{
    if (is_open())
	return NULL;
    _fb._fileno = fd;
    xsetflags(0, _S_NO_READS+_S_NO_WRITES);
    return this;
}

streambuf* filebuf::setbuf(char* p, int len)
{
    if (streambuf::setbuf(p, len) == NULL)
	return NULL;
    setp(_base, _base);
    setg(_base, _base, _base);
    return this;
}

int filebuf::overflow(int c)
{
    if (xflags() & _S_NO_WRITES) // SET ERROR
	return EOF;
    // Allocate a buffer if needed.
    if (base() == NULL) {
	doallocbuf();
	if (xflags() & _S_LINE_BUF+_S_UNBUFFERED) setp(_base, _base);
	else setp(_base, _ebuf);
	setg(_base, _base, _base);
	_flags |= _S_CURRENTLY_PUTTING;
    }
    // If currently reading, switch to writing.
    else if ((_flags & _S_CURRENTLY_PUTTING) == 0) {
	if (xflags() & _S_LINE_BUF+_S_UNBUFFERED) setp(gptr(), gptr());
	else setp(gptr(), ebuf());
	setg(egptr(), egptr(), egptr());
	_flags |= _S_CURRENTLY_PUTTING;
    }
    if (c == EOF)
	return do_flush();
    if (pptr() == ebuf() ) // Buffer is really full
	if (do_flush() == EOF)
	    return EOF;
    xput_char(c);
    if (unbuffered() || (linebuffered() && c == '\n'))
	if (do_flush() == EOF)
	    return EOF;
    return (unsigned char)c;
}

int filebuf::underflow()
{
#if 0
    /* SysV does not make this test; take it out for compatibility */
    if (fp->_flags & __SEOF)
	return (EOF);
#endif

    if (xflags() & _S_NO_READS)
	return EOF;
    if (gptr() < egptr())
	return *(unsigned char*)gptr();
    allocbuf();

    // FIXME This can/should be moved to __streambuf ??
    if ((xflags() & _S_LINE_BUF) || unbuffered()) {
	// Flush all line buffered files before reading.
	streambuf::flush_all_linebuffered();
    }

    switch_to_get_mode();

    _G_ssize_t count = sys_read(base(), ebuf() - base());
    if (count <= 0) {
	if (count == 0)
	    xsetflags(_S_EOF_SEEN);
	else
	    xsetflags(_S_ERR_SEEN), count = 0;
    }
    setg(base(), base(), base() + count);
    setp(base(), base());
    if (count == 0)
	return EOF;
    if (_fb._offset >= 0)
	_fb._offset += count;
    return *(unsigned char*)gptr();
}

int filebuf::do_write(const char *data, int to_do)
{
    if (to_do == 0)
	return 0;
    if (xflags() & _S_IS_APPENDING) {
	// On a system without a proper O_APPEND implementation,
	// you would need to sys_seek(0, ios::end) here, but is
	// is not needed nor desirable for Unix- or Posix-like systems.
	// Instead, just indicate that offset (before and after) is
	// unpredictable.
	_fb._offset = -1;
    }
    else if (egptr() != pbase()) {
	long new_pos = sys_seek(pbase()-egptr(), ios::cur);
	if (new_pos == -1)
	    return EOF;
	_fb._offset = new_pos;
    }
    _G_ssize_t count = sys_write(data, to_do);
    if (_cur_column)
	_cur_column = __adjust_column(_cur_column - 1, data, to_do) + 1;
    setg(base(), base(), base());
    if (xflags() & _S_LINE_BUF+_S_UNBUFFERED) setp(base(), base());
    else setp(base(), ebuf());
    return count != to_do ? EOF : 0;
}

int filebuf::sync()
{
//    char* ptr = cur_ptr();
    if (pptr() > pbase())
	if (do_flush()) return EOF;
    if (gptr() != egptr()) {
	streampos delta = gptr() - egptr();
	if (in_backup())
	    delta -= eGptr() - Gbase();
	_G_fpos_t new_pos = sys_seek(delta, ios::cur);
	if (new_pos == EOF)
	  return EOF;
	_fb._offset = new_pos;
	setg(eback(), gptr(), gptr());
    }
    // FIXME: Cleanup - can this be shared?
//    setg(base(), ptr, ptr);
    return 0;
}

streampos filebuf::seekoff(streamoff offset, _seek_dir dir, int mode)
{
    streampos result, new_offset, delta;
    _G_ssize_t count;

    if (mode == 0) // Don't move any pointers.
	dir = ios::cur, offset = 0;

    // Flush unwritten characters.
    // (This may do an unneeded write if we seek within the buffer.
    // But to be able to switch to reading, we would need to set
    // egptr to ptr.  That can't be done in the current design,
    // which assumes file_ptr() is eGptr.  Anyway, since we probably
    // end up flushing when we close(), it doesn't make much difference.)
    if (pptr() > pbase() || put_mode())
	if (switch_to_get_mode()) return EOF;

    if (base() == NULL) {
	doallocbuf();
	setp(base(), base());
	setg(base(), base(), base());
    }
    switch (dir) {
      case ios::cur:
	if (_fb._offset < 0) {
	    _fb._offset = sys_seek(0, ios::cur);
	    if (_fb._offset < 0)
		return EOF;
	}
	// Make offset absolute, assuming current pointer is file_ptr().
	offset += _fb._offset;

	offset -= _egptr - _gptr;
	if (in_backup())
	    offset -= _other_egptr - _other_gbase;
	dir = ios::beg;
	break;
      case ios::beg:
	break;
      case ios::end:
	struct stat st;
	if (sys_stat(&st) == 0 && S_ISREG(st.st_mode)) {
	    offset += st.st_size;
	    dir = ios::beg;
	}
	else
	    goto dumb;
    }
    // At this point, dir==ios::beg.

    // If destination is within current buffer, optimize:
    if (_fb._offset >= 0 && _eback != NULL) {
	// Offset relative to start of main get area.
	_G_fpos_t rel_offset = offset - _fb._offset
	    + (eGptr()-Gbase());
	if (rel_offset >= 0) {
	    if (in_backup())
		switch_to_main_get_area();
	    if (rel_offset <= _egptr - _eback) {
		setg(base(), base() + rel_offset, egptr());
		setp(base(), base());
		return offset;
	    }
	    // If we have streammarkers, seek forward by reading ahead.
	    if (have_markers()) {
		int to_skip = rel_offset - (_gptr - _eback);
		if (ignore(to_skip) != to_skip)
		    goto dumb;
		return offset;
	    }
	}
	if (rel_offset < 0 && rel_offset >= Bbase() - Bptr()) {
	    if (!in_backup())
		switch_to_backup_area();
	    gbump(_egptr + rel_offset - gptr());
	    return offset;
	}
    }

    unsave_markers();

    // Try to seek to a block boundary, to improve kernel page management.
    new_offset = offset & ~(ebuf() - base() - 1);
    delta = offset - new_offset;
    if (delta > ebuf() - base()) {
	new_offset = offset;
	delta = 0;
    }
    result = sys_seek(new_offset, ios::beg);
    if (result < 0)
	return EOF;
    if (delta == 0)
	count = 0;
    else {
	count = sys_read(base(), ebuf()-base());
	if (count < delta) {
	    // We weren't allowed to read, but try to seek the remainder.
	    offset = count == EOF ? delta : delta-count;
	    dir = ios::cur;
	    goto dumb;
	}
    }
    setg(base(), base()+delta, base()+count);
    setp(base(), base());
    _fb._offset = result + count;
    xflags(xflags() & ~ _S_EOF_SEEN);
    return offset;
  dumb:
    unsave_markers();
    result = sys_seek(offset, dir);
    if (result != EOF) {
	xflags(xflags() & ~_S_EOF_SEEN);
    }
    _fb._offset = result;
    setg(base(), base(), base());
    setp(base(), base());
    return result;
}

filebuf* filebuf::close()
{
    if (!is_open())
	return NULL;

    // This flushes as well as switching mode.
    if (pptr() > pbase() || put_mode())
	if (switch_to_get_mode()) return NULL;

    unsave_markers();

    int status = sys_close();

    // Free buffer.
    setb(NULL, NULL, 0);
    setg(NULL, NULL, NULL);
    setp(NULL, NULL);

    _un_link();
    _flags = _IO_MAGIC|CLOSED_FILEBUF_FLAGS;
    _fb._fileno = EOF;
    _fb._offset = 0;

    return status < 0 ? NULL : this;
}

_G_ssize_t filebuf::sys_read(char* buf, size_t size)
{
    for (;;) {
	_G_ssize_t count = ::read(_fb._fileno, buf, size);
	if (count != -1 || errno != EINTR)
	    return count;
    }
}

_G_fpos_t filebuf::sys_seek(_G_fpos_t offset, _seek_dir dir)
{
    return ::lseek(fd(), offset, (int)dir);
}

_G_ssize_t filebuf::sys_write(const void *buf, long n)
{
    long to_do = n;
    while (to_do > 0) {
	_G_ssize_t count = ::write(fd(), buf, to_do);
	if (count == EOF) {
	    if (errno == EINTR)
		continue;
	    else {
		_flags |= _S_ERR_SEEN;
		break;
	    }
	}
	to_do -= count;
	buf = (void*)((char*)buf + count);
    }
    n -= to_do;
    if (_fb._offset >= 0)
	_fb._offset += n;
    return n;
}

int filebuf::sys_stat(void* st)
{
    return ::_fstat(fd(), (struct stat*)st);
}

int filebuf::sys_close()
{
    return ::close(fd());
}

int filebuf::xsputn(const char *s, int n)
{
    if (n <= 0)
	return 0;
    // This is an optimized implementation.
    // If the amount to be written straddles a block boundary
    // (or the filebuf is unbuffered), use sys_write directly.

    int to_do = n;
    int must_flush = 0;
    // First figure out how much space is available in the buffer.
    int count = _epptr - _pptr; // Space available.
    if (linebuffered() && (_flags & _S_CURRENTLY_PUTTING)) {
	count =_ebuf - _pptr;
	if (count >= n) {
	    for (register const char *p = s + n; p > s; ) {
		if (*--p == '\n') {
		    count = p - s + 1;
		    must_flush = 1;
		    break;
		}
	    }
	}
    }
    // Then fill the buffer.
    if (count > 0) {
	if (count > to_do)
	    count = to_do;
	if (count > 20) {
	    memcpy(pptr(), s, count);
	    s += count;
	}
	else {
	    register char *p = pptr();;
	    for (register int i = count; --i >= 0; ) *p++ = *s++;
	}
	pbump(count);
	to_do -= count;
    }
    if (to_do + must_flush > 0) {
	// Next flush the (full) buffer.
	if (__overflow(this, EOF) == EOF)
	    return n - to_do;

	// Try to maintain alignment: write a whole number of blocks.
	// dont_write is what gets left over.
	int block_size = _ebuf - _base;
	int dont_write = block_size >= 128 ? to_do % block_size : 0;

	_G_ssize_t count = to_do - dont_write;
	if (do_write(s, count) == EOF)
	    return n - to_do;
	to_do = dont_write;

	// Now write out the remainder.  Normally, this will fit in the
	// buffer, but it's somewhat messier for line-buffered files,
	// so we let streambuf::sputn handle the general case.
	if (dont_write)
	    to_do -= streambuf::sputn(s+count, dont_write);
    }
    return n - to_do;
}

int filebuf::xsgetn(char *s, int n)
{
    // FIXME: OPTIMIZE THIS (specifically, when unbuffered()).
    return streambuf::xsgetn(s, n);
}

// Non-ANSI AT&T-ism:  Default open protection.
const int filebuf::openprot = 0644;
