/* Host callback routines for GDB.
   Copyright 1995 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* This file provides a standard way for targets to talk to the host OS
   level.

   This interface will probably need a bit more banging to make it
   smooth.  Currently the simulator uses this file to provide the
   callbacks for itself when it's built standalone, which is rather
   ugly. */

#ifndef INSIDE_SIMULATOR
#include "defs.h"
#endif

#include "ansidecl.h"
#include "callback.h"
#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>



/* Set the callback copy of errno from what we see now. */
static int 
wrap (p, val)
     host_callback *p;
     int val;
{
  p->last_errno = errno;
  return val;
}

/* Make sure the FD provided is ok.  If not, return non-zero
   and set errno. */

static int 
fdbad (p, fd)
     host_callback *p;
     int fd;
{
  if (fd < 0 || fd > MAX_CALLBACK_FDS || !p->fdopen[fd])
    {
      p->last_errno = EINVAL;
      return -1;
    }
  return 0;
}

static int 
fdmap (p, fd)
     host_callback *p;
     int fd;
{
  return p->fdmap[fd];
}

int 
os_close (p, fd)
     host_callback *p;
     int fd;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = wrap (p, close (fdmap (p, fd)));
  return result;
}

int 
os_get_errno (p)
     host_callback *p;
{
  /* !!! fixme, translate from host to taget errno value */
  return p->last_errno;
}


int 
os_isatty (p, fd)
     host_callback *p;
     int fd;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = wrap (p, isatty (fdmap (fd)));
  return result;
}

int 
os_lseek (p, fd, off, way)
     host_callback *p;
     int fd;
     long off;
     int way;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = lseek (fdmap (p, fd), off, way);
  return result;
}

int 
os_open (p, name, flags)
     host_callback *p;
     const char *name;
     int flags;
{
  int i;
  for (i = 0; i < MAX_CALLBACK_FDS; i++)
    {
      if (!p->fdopen[i])
	{
	  int f = open (name, flags);
	  if (f < 0)
	    {
	      p->last_errno = errno;
	      return f;
	    }
	  p->fdopen[i] = 1;
	  p->fdmap[i] = f;
	  return i;
	}
    }
  p->last_errno = EMFILE;
  return -1;
}

int 
os_read (p, fd, buf, len)
     host_callback *p;
     int fd;
     char *buf;
     int len;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = wrap (p, read (fdmap (p, fd), buf, len));
  return result;
}

int 
os_read_stdin (p, buf, len)
     host_callback *p;
     char *buf;
     int len;
{
  return wrap (p, read (0, buf, len));
}

int 
os_write (p, fd, buf, len)
     host_callback *p;
     int fd;
     const char *buf;
     int len;
{
  int result;

  result = fdbad (p, fd);
  if (result)
    return result;
  result = wrap (p, write (fdmap (p, fd), buf, len));
  return result;
}

/* ignore the grossness of INSIDE_SIMULATOR, it will go away one day. */
int 
os_write_stdout (p, buf, len)
     host_callback *p;
     const char *buf;
     int len;
{
#ifdef INSIDE_SIMULATOR
  return os_write (p, 1, buf, len);
#else
  int i;
  char b[2];
  for (i = 0; i< len; i++) 
    {
      b[0] = buf[i];
      b[1] = 0;
      if (target_output_hook)
	target_output_hook (b);
      else
	fputs_filtered (b, gdb_stdout);
    }
  return len;
#endif
}

int 
os_rename (p, f1, f2)
     host_callback *p;
     const char *f1;
     const char *f2;
{
  return wrap (p, rename (f1, f2));
}


int
os_system (p, s)
     host_callback *p;
     const char *s;
{
  return wrap (p, system (s));
}

long 
os_time (p, t)
     host_callback *p;
     long *t;
{
  return wrap (p, time (t));
}


int 
os_unlink (p, f1)
     host_callback *p;
     const char *f1;
{
  return wrap (p, unlink (f1));
}


int
os_shutdown (p)
host_callback *p;
{
  int i;
  for (i = 0; i < MAX_CALLBACK_FDS; i++)
    {
      if (p->fdopen[i] && !p->alwaysopen[i]) {
	close (p->fdmap[i]);
	p->fdopen[i] = 0;
      }
    }
  return 1;
}

int os_init(p)
host_callback *p;
{
  int i;
  os_shutdown (p);
  for (i= 0; i < 3; i++)
    {
      p->fdmap[i] = i;
      p->fdopen[i] = 1;
      p->alwaysopen[i] = 1;
    }
  return 1;
}


/* !!fixme!!
   This bit is ugly.  When the interface has settled down I'll 
   move the whole file into sim/common and remove this bit. */

/* VARARGS */
void
#ifdef ANSI_PROTOTYPES
os_printf_filtered (host_callback *p, const char *format, ...)
#else
os_printf_filtered (p, va_alist)
     host_callback *p;
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start (args, format);
#else
  char *format;

  va_start (args);
  format = va_arg (args, char *);
#endif

#ifdef INSIDE_SIMULATOR
  vprintf (format, args);
#else
  vfprintf_filtered (stdout, format, args);
#endif

  va_end (args);
}

host_callback default_callback =
{
  os_close,
  os_get_errno,
  os_isatty,
  os_lseek,
  os_open,
  os_read,
  os_read_stdin,
  os_rename,
  os_system,
  os_time,
  os_unlink,
  os_write,
  os_write_stdout,

  os_shutdown,
  os_init,

  os_printf_filtered,

  0, 		/* last errno */
};
