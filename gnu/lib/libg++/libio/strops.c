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

#include "strfile.h"
#include "libioP.h"
#include <string.h>

#define LEN(fp) (((_IO_strfile*)(fp))->_s._len)

#ifdef TODO
/* An "unbounded buffer" is when a buffer is supplied, but with no
   specified length.  An example is the buffer argument to sprintf.
   */
#endif

void
_IO_str_init_static (fp, ptr, size, pstart)
     _IO_FILE *fp;
     char *ptr;
     int size;
     char *pstart;
{
  if (size == 0)
    size = strlen(ptr);
  else if (size < 0)
    {
      /* If size is negative 'the characters are assumed to
	 continue indefinitely.'  This is kind of messy ... */
#if 1
      int s;
      size = 512;
      /* Try increasing powers of 2, as long as we don't wrap around.
	 This can lose in pathological cases (ptr near the end
	 of the address space).  A better solution might be to
	 adjust the size on underflow/overflow.  FIXME. */
      for (s; s = 2*size, s > 0 && ptr + s > ptr && s < 0x4000000L; )
	size = s;
      size = s;
#else
      /* The following semi-portable kludge assumes that
	 sizeof(unsigned long) == sizeof(char*). Hence,
	 (unsigned long)(-1) should be the largest possible address. */
      unsigned long highest = (unsigned long)(-1);
      /* Pointers are signed on some brain-damaged systems, in
	 which case we divide by two to get the maximum signed address. */
      if  ((char*)highest < ptr)
	highest >>= 1;
      size = (char*)highest - ptr;
#endif
    }
  _IO_setb(fp, ptr, ptr+size, 0);

  fp->_IO_write_base = ptr;
  fp->_IO_read_base = ptr;
  fp->_IO_read_ptr = ptr;
  if (pstart)
    {
      fp->_IO_write_ptr = pstart;
      fp->_IO_write_end = ptr+size;
      fp->_IO_read_end = pstart;
    }
  else
    {
      fp->_IO_write_ptr = ptr;
      fp->_IO_write_end = ptr;
      fp->_IO_read_end = ptr+size;
    }
  LEN(fp) = size;
  /* A null _allocate_buffer function flags the strfile as being static. */
  (((_IO_strfile*)(fp))->_s._allocate_buffer) =  (_IO_alloc_type)0;
}

void
_IO_str_init_readonly (fp, ptr, size)
     _IO_FILE *fp;
     const char *ptr;
     int size;
{
  _IO_str_init_static (fp, (char*)ptr, size, NULL);
  fp->_IO_file_flags |= _IO_NO_WRITES;
}

int _IO_str_overflow (fp, c)
     register _IO_FILE* fp;
     int c;
{
  int flush_only = c == EOF;
  _IO_size_t pos = fp->_IO_write_ptr - fp->_IO_write_base;
  _IO_size_t get_pos = fp->_IO_read_ptr - fp->_IO_read_base;
  if (fp->_flags & _IO_NO_WRITES)
      return flush_only ? 0 : EOF;
  if (pos > LEN(fp)) LEN(fp) = pos;
  if ((fp->_flags & _IO_TIED_PUT_GET) && !(fp->_flags & _IO_CURRENTLY_PUTTING))
    {
      pos = get_pos;
      fp->_flags |= _IO_CURRENTLY_PUTTING;
      get_pos = LEN(fp);
    }
  if (pos >= _IO_blen(fp) + flush_only)
    {
      if (fp->_flags & _IO_USER_BUF) /* not allowed to enlarge */
	{
#ifdef TODO
	  if (indefinite size)
	    {
	      fp->_IO_buf_end += 512;
	    }
	  else
#endif
	  return EOF;
	}
      else
	{
	  char *new_buf;
	  _IO_size_t new_size = 2 * _IO_blen(fp);
	  new_buf
	    = (char*)(*((_IO_strfile*)fp)->_s._allocate_buffer)(new_size);
	  if (new_buf == NULL)
	    {
	      /*	  __ferror(fp) = 1; */
	      return EOF;
	    }
	  memcpy(new_buf, fp->_IO_buf_base, _IO_blen(fp));
#if 0
	  if (lenp == &LEN(fp)) /* use '\0'-filling */
	      memset(new_buf + pos, 0, blen() - pos);
#endif
	  if (fp->_IO_buf_base)
	    {
	      (*((_IO_strfile*)fp)->_s._free_buffer)(fp->_IO_buf_base);
	      /* Make sure _IO_setb won't try to delete _IO_buf_base. */
	      fp->_IO_buf_base = NULL;
	    }
	  _IO_setb(fp, new_buf, new_buf + new_size, 1);
	  fp->_IO_write_base = new_buf;
	}
      fp->_IO_write_end = fp->_IO_buf_end;
    }

  fp->_IO_write_ptr = fp->_IO_buf_base + pos;

  fp->_IO_read_base = fp->_IO_buf_base;
  fp->_IO_read_ptr = fp->_IO_buf_base + get_pos;;
  fp->_IO_read_end = fp->_IO_buf_base + LEN(fp);;

  if (!flush_only)
    *fp->_IO_write_ptr++ = (unsigned char) c;
  return c;
}

int
_IO_str_underflow (fp)
     register _IO_FILE* fp;
{
  _IO_size_t ppos = fp->_IO_write_ptr - fp->_IO_write_base;
  if (ppos > LEN(fp)) LEN(fp) = ppos;
  if ((fp->_flags & _IO_TIED_PUT_GET) && (fp->_flags & _IO_CURRENTLY_PUTTING))
    {
      fp->_flags &= ~_IO_CURRENTLY_PUTTING;
      fp->_IO_write_ptr = fp->_IO_write_end;
    }
  fp->_IO_read_end = fp->_IO_read_base + LEN(fp);
  if (fp->_IO_read_ptr < fp->_IO_read_end)
    return *fp->_IO_read_ptr;
  else
    return EOF;
}

_IO_ssize_t
_IO_str_count (fp)
     register _IO_FILE *fp;
{
  _IO_ssize_t put_len = fp->_IO_write_ptr - fp->_IO_write_base;
  if (put_len < ((_IO_strfile*)fp)->_s._len)
    put_len = ((_IO_strfile*)fp)->_s._len;
  return put_len;
}     

_IO_pos_t
_IO_str_seekoff(fp, offset, mode)
     register _IO_FILE *fp;
     _IO_off_t offset;
     _IO_seekflags mode;
{
  _IO_ssize_t cur_size = _IO_str_count(fp);
  _IO_pos_t new_pos = EOF;
  int dir = mode & 3;

  /* Move the get pointer, if requested. */
  if (!(mode & _IO_seek_not_in))
    {
      switch (dir)
	{
	case _IO_seek_end:
	  offset += cur_size;
	  break;
	case _IO_seek_cur:
	  offset += fp->_IO_read_ptr - fp->_IO_read_base;
	  break;
	default: /* case _IO_seek_set: */
	  break;
	}
      if (offset < 0 || (_IO_size_t)offset > cur_size)
	return EOF;
      fp->_IO_read_ptr = fp->_IO_read_base + offset;
      fp->_IO_read_end = fp->_IO_read_base + cur_size;
      new_pos = offset;
    }

  /* Move the put pointer, if requested. */
  if (!(mode & _IO_seek_not_out))
    {
      switch (dir)
	{
	case _IO_seek_end:
	  offset += cur_size;
	  break;
	case _IO_seek_cur:
	  offset += fp->_IO_write_ptr - fp->_IO_write_base;
	  break;
	default: /* case _IO_seek_set: */
	  break;
	}
      if (offset < 0 || (_IO_size_t)offset > cur_size)
	return EOF;
      fp->_IO_write_ptr = fp->_IO_write_base + offset;
      new_pos = offset;
    }
  return new_pos;
}

int
_IO_str_pbackfail(fp, c)
     register _IO_FILE *fp;
     int c;
{
  if ((fp->_flags & _IO_NO_WRITES) && c != EOF)
    return EOF;
  return _IO_default_pbackfail(fp, c);
}

void
_IO_str_finish(fp)
     register _IO_FILE* fp;
{
  if (fp->_IO_buf_base && !(fp->_flags & _IO_USER_BUF))
    (((_IO_strfile*)fp)->_s._free_buffer)(fp->_IO_buf_base);
  fp->_IO_buf_base = NULL;

  _IO_default_finish(fp);
}

struct _IO_jump_t _IO_str_jumps = {
  _IO_str_overflow,
  _IO_str_underflow,
  _IO_default_xsputn,
  _IO_default_xsgetn,
  _IO_default_read,
  _IO_default_write,
  _IO_default_doallocate,
  _IO_str_pbackfail,
  _IO_default_setbuf,
  _IO_default_sync,
  _IO_str_finish,
  _IO_default_close,
  _IO_default_stat,
  _IO_default_seek,
  _IO_str_seekoff,
  _IO_default_seekpos,
  _IO_default_uflow
};
