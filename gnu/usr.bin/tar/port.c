/* Supporting routines which may sometimes be missing.
   Copyright (C) 1988, 1992 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#ifndef STDC_HEADERS
extern int errno;
#endif

#ifdef BSD42
#include <sys/file.h>
#else
#ifndef V7
#include <fcntl.h>
#endif
#endif

#include "tar.h"
#include "port.h"

extern long baserec;

/* All machine-dependent #ifdefs should appear here, instead of
   being scattered through the file.  For UN*X systems, it is better to
   figure out what is needed in the configure script, for most of the
   features. */

#ifdef __MSDOS__
char TTY_NAME[] = "con";
#define HAVE_STRSTR
#define HAVE_RENAME
#define HAVE_MKDIR
#else
char TTY_NAME[] = "/dev/tty";
#endif

/* End of system-dependent #ifdefs */


#ifndef HAVE_VALLOC
/*
 * valloc() does a malloc() on a page boundary.  On some systems,
 * this can make large block I/O more efficient.
 */
char *
valloc (size)
     unsigned size;
{
  return (malloc (size));
}

#endif /* !HAVE_VALLOC */

#ifndef HAVE_MKDIR
/*
 * Written by Robert Rother, Mariah Corporation, August 1985.
 *
 * If you want it, it's yours.  All I ask in return is that if you
 * figure out how to do this in a Bourne Shell script you send me
 * a copy.
 *					sdcsvax!rmr or rmr@uscd
 *
 * Severely hacked over by John Gilmore to make a 4.2BSD compatible
 * subroutine.	11Mar86; hoptoad!gnu
 *
 * Modified by rmtodd@uokmax 6-28-87 -- when making an already existing dir,
 * subroutine didn't return EEXIST.  It does now.
 */

/*
 * Make a directory.
 */
int
mkdir (dpath, dmode)
     char *dpath;
     int dmode;
{
  int cpid, status;
  struct stat statbuf;

  if (stat (dpath, &statbuf) == 0)
    {
      errno = EEXIST;		/* Stat worked, so it already exists */
      return -1;
    }

  /* If stat fails for a reason other than non-existence, return error */
  if (errno != ENOENT)
    return -1;

  switch (cpid = fork ())
    {

    case -1:			/* Error in fork() */
      return (-1);		/* Errno is set already */

    case 0:			/* Child process */
      /*
		 * Cheap hack to set mode of new directory.  Since this
		 * child process is going away anyway, we zap its umask.
		 * FIXME, this won't suffice to set SUID, SGID, etc. on this
		 * directory.  Does anybody care?
		 */
      status = umask (0);	/* Get current umask */
      status = umask (status | (0777 & ~dmode));	/* Set for mkdir */
      execl ("/bin/mkdir", "mkdir", dpath, (char *) 0);
      _exit (-1);		/* Can't exec /bin/mkdir */

    default:			/* Parent process */
      while (cpid != wait (&status));	/* Wait for kid to finish */
    }

  if (WIFSIGNALED (status) || WEXITSTATUS (status) != 0)
    {
      errno = EIO;		/* We don't know why, but */
      return -1;		/* /bin/mkdir failed */
    }

  return 0;
}

int
rmdir (dpath)
     char *dpath;
{
  int cpid, status;
  struct stat statbuf;

  if (stat (dpath, &statbuf) != 0)
    {
      /* Stat just set errno.  We don't have to */
      return -1;
    }

  switch (cpid = fork ())
    {

    case -1:			/* Error in fork() */
      return (-1);		/* Errno is set already */

    case 0:			/* Child process */
      execl ("/bin/rmdir", "rmdir", dpath, (char *) 0);
      _exit (-1);		/* Can't exec /bin/mkdir */

    default:			/* Parent process */
      while (cpid != wait (&status));	/* Wait for kid to finish */
    }

  if (WIFSIGNALED (status) || WEXITSTATUS (status) != 0)
    {
      errno = EIO;		/* We don't know why, but */
      return -1;		/* /bin/mkdir failed */
    }

  return 0;
}

#endif /* !HAVE_MKDIR */

#ifndef HAVE_RENAME
/* Rename file FROM to file TO.
   Return 0 if successful, -1 if not. */

int
rename (from, to)
     char *from;
     char *to;
{
  struct stat from_stats;

  if (stat (from, &from_stats))
    return -1;

  if (unlink (to) && errno != ENOENT)
    return -1;

  if (link (from, to))
    return -1;

  if (unlink (from) && errno != ENOENT)
    {
      unlink (to);
      return -1;
    }

  return 0;
}

#endif /* !HAVE_RENAME */

#ifdef minix
/* Minix has bcopy but not bzero, and no memset.  Thanks, Andy. */
void
bzero (s1, n)
     register char *s1;
     register int n;
{
  while (n--)
    *s1++ = '\0';
}

/* It also has no bcmp() */
int
bcmp (s1, s2, n)
     register char *s1, *s2;
     register int n;
{
  for (; n--; ++s1, ++s2)
    {
      if (*s1 != *s2)
	return *s1 - *s2;
    }
  return 0;
}

/*
 * Groan, Minix doesn't have execlp either!
 *
 * execlp(file,arg0,arg1...argn,(char *)NULL)
 * exec a program, automatically searching for the program through
 * all the directories on the PATH.
 *
 * This version is naive about variable argument lists, it assumes
 * a straightforward C calling sequence.  If your system has odd stacks
 * *and* doesn't have execlp, YOU get to fix it.
 */
int
execlp (filename, arg0)
     char *filename, *arg0;
{
  register char *p, *path;
  register char *fnbuffer;
  char **argstart = &arg0;
  struct stat statbuf;
  extern char **environ;

  if ((p = getenv ("PATH")) == NULL)
    {
      /* couldn't find path variable -- try to exec given filename */
      return execve (filename, argstart, environ);
    }

  /*
	 * make a place to build the filename.  We malloc larger than we
	 * need, but we know it will fit in this.
	 */
  fnbuffer = malloc (strlen (p) + 1 + strlen (filename));
  if (fnbuffer == NULL)
    {
      errno = ENOMEM;
      return -1;
    }

  /*
	 * try each component of the path to see if the file's there
	 * and executable.
	 */
  for (path = p; path; path = p)
    {
      /* construct full path name to try */
      if ((p = index (path, ':')) == NULL)
	{
	  strcpy (fnbuffer, path);
	}
      else
	{
	  strncpy (fnbuffer, path, p - path);
	  fnbuffer[p - path] = '\0';
	  p++;			/* Skip : for next time */
	}
      if (strlen (fnbuffer) != 0)
	strcat (fnbuffer, "/");
      strcat (fnbuffer, filename);

      /* check to see if file is there and is a normal file */
      if (stat (fnbuffer, &statbuf) < 0)
	{
	  if (errno == ENOENT)
	    continue;		/* file not there,keep on looking */
	  else
	    goto fail;		/* failed for some reason, return */
	}
      if (!S_ISREG (statbuf.st_mode))
	continue;

      if (execve (fnbuffer, argstart, environ) < 0
	  && errno != ENOENT
	  && errno != ENOEXEC)
	{
	  /* failed, for some other reason besides "file
			 * not found" or "not a.out format"
			 */
	  goto fail;
	}

      /*
		 * If we got error ENOEXEC, the file is executable but is
		 * not an object file.  Try to execute it as a shell script,
		 * returning error if we can't execute /bin/sh.
		 *
		 * FIXME, this code is broken in several ways.  Shell
		 * scripts should not in general be executed by the user's
		 * SHELL variable program.  On more mature systems, the
		 * script can specify with #!/bin/whatever.  Also, this
		 * code clobbers argstart[-1] if the exec of the shell
		 * fails.
		 */
      if (errno == ENOEXEC)
	{
	  char *shell;

	  /* Try to execute command "sh arg0 arg1 ..." */
	  if ((shell = getenv ("SHELL")) == NULL)
	    shell = "/bin/sh";
	  argstart[-1] = shell;
	  argstart[0] = fnbuffer;
	  execve (shell, &argstart[-1], environ);
	  goto fail;		/* Exec didn't work */
	}

      /*
		 * If we succeeded, the execve() doesn't return, so we
		 * can only be here is if the file hasn't been found yet.
		 * Try the next place on the path.
		 */
    }

  /* all attempts failed to locate the file.  Give up. */
  errno = ENOENT;

fail:
  free (fnbuffer);
  return -1;
}

#endif /* minix */


#ifdef EMUL_OPEN3
#include "open3.h"
/*
 * open3 -- routine to emulate the 3-argument open system
 * call that is present in most modern Unix systems.
 * This version attempts to support all the flag bits except for O_NDELAY
 * and O_APPEND, which are silently ignored.  The emulation is not as efficient
 * as the real thing (at worst, 4 system calls instead of one), but there's
 * not much I can do about that.
 *
 * Written 6/10/87 by rmtodd@uokmax
 *
 * open3(path, flag, mode)
 * Attempts to open the file specified by
 * the given pathname.  The following flag bits (#defined in tar.h)
 * specify options to the routine:
 *	O_RDONLY	file open for read only
 *	O_WRONLY	file open for write only
 *	O_RDWR		file open for both read & write
 * (Needless to say, you should only specify one of the above).
 * 	O_CREAT		file is created with specified mode if it needs to be.
 *	O_TRUNC		if file exists, it is truncated to 0 bytes
 *	O_EXCL		used with O_CREAT--routine returns error if file exists
 * Function returns file descriptor if successful, -1 and errno if not.
 */

/*
 * array to give arguments to access for various modes
 * FIXME, this table depends on the specific integer values of O_XXX,
 * and also contains integers (args to 'access') that should be #define's.
 */
static int modes[] =
{
  04,				/* O_RDONLY */
  02,				/* O_WRONLY */
  06,				/* O_RDWR */
  06,				/* invalid but we'd better cope -- O_WRONLY+O_RDWR */
};

/* Shut off the automatic emulation of open(), we'll need it. */
#undef open

int
open3 (path, flags, mode)
     char *path;
     int flags, mode;
{
  int exists = 1;
  int call_creat = 0;
  int fd;
  /*
	 * We actually do the work by calling the open() or creat() system
	 * call, depending on the flags.  Call_creat is true if we will use
	 * creat(), false if we will use open().
	 */

  /*
	 * See if the file exists and is accessible in the requested mode.
	 *
	 * Strictly speaking we shouldn't be using access, since access checks
	 * against real uid, and the open call should check against euid.
	 * Most cases real uid == euid, so it won't matter.   FIXME.
	 * FIXME, the construction "flags & 3" and the modes table depends
	 * on the specific integer values of the O_XXX #define's.  Foo!
	 */
  if (access (path, modes[flags & 3]) < 0)
    {
      if (errno == ENOENT)
	{
	  /* the file does not exist */
	  exists = 0;
	}
      else
	{
	  /* probably permission violation */
	  if (flags & O_EXCL)
	    {
	      /* Oops, the file exists, we didn't want it. */
	      /* No matter what the error, claim EEXIST. */
	      errno = EEXIST;
	    }
	  return -1;
	}
    }

  /* if we have the O_CREAT bit set, check for O_EXCL */
  if (flags & O_CREAT)
    {
      if ((flags & O_EXCL) && exists)
	{
	  /* Oops, the file exists and we didn't want it to. */
	  errno = EEXIST;
	  return -1;
	}
      /*
		 * If the file doesn't exist, be sure to call creat() so that
		 * it will be created with the proper mode.
		 */
      if (!exists)
	call_creat = 1;
    }
  else
    {
      /* If O_CREAT isn't set and the file doesn't exist, error. */
      if (!exists)
	{
	  errno = ENOENT;
	  return -1;
	}
    }

  /*
	 * If the O_TRUNC flag is set and the file exists, we want to call
	 * creat() anyway, since creat() guarantees that the file will be
	 * truncated and open()-for-writing doesn't.
	 * (If the file doesn't exist, we're calling creat() anyway and the
	 * file will be created with zero length.)
	 */
  if ((flags & O_TRUNC) && exists)
    call_creat = 1;
  /* actually do the call */
  if (call_creat)
    {
      /*
		 * call creat.  May have to close and reopen the file if we
		 * want O_RDONLY or O_RDWR access -- creat() only gives
		 * O_WRONLY.
		 */
      fd = creat (path, mode);
      if (fd < 0 || (flags & O_WRONLY))
	return fd;
      if (close (fd) < 0)
	return -1;
      /* Fall out to reopen the file we've created */
    }

  /*
	 * calling old open, we strip most of the new flags just in case.
	 */
  return open (path, flags & (O_RDONLY | O_WRONLY | O_RDWR | O_BINARY));
}

#endif /* EMUL_OPEN3 */

#ifndef HAVE_MKNOD
#ifdef __MSDOS__
typedef int dev_t;
#endif
/* Fake mknod by complaining */
int
mknod (path, mode, dev)
     char *path;
     unsigned short mode;
     dev_t dev;
{
  int fd;

  errno = ENXIO;		/* No such device or address */
  return -1;			/* Just give an error */
}

/* Fake links by copying */
int
link (path1, path2)
     char *path1;
     char *path2;
{
  char buf[256];
  int ifd, ofd;
  int nrbytes;
  int nwbytes;

  fprintf (stderr, "%s: %s: cannot link to %s, copying instead\n",
	   tar, path1, path2);
  if ((ifd = open (path1, O_RDONLY | O_BINARY)) < 0)
    return -1;
  if ((ofd = creat (path2, 0666)) < 0)
    return -1;
  setmode (ofd, O_BINARY);
  while ((nrbytes = read (ifd, buf, sizeof (buf))) > 0)
    {
      if ((nwbytes = write (ofd, buf, nrbytes)) != nrbytes)
	{
	  nrbytes = -1;
	  break;
	}
    }
  /* Note use of "|" rather than "||" below: we want to close
	 * the files even if an error occurs.
	 */
  if ((nrbytes < 0) | (0 != close (ifd)) | (0 != close (ofd)))
    {
      unlink (path2);
      return -1;
    }
  return 0;
}

/* everyone owns everything on MS-DOS (or is it no one owns anything?) */
int
chown (path, uid, gid)
     char *path;
     int uid;
     int gid;
{
  return 0;
}

int
geteuid ()
{
  return 0;
}

#endif /* !HAVE_MKNOD */

#ifdef __TURBOC__
#include <time.h>
#include <fcntl.h>
#include <io.h>

struct utimbuf
{
  time_t actime;		/* Access time. */
  time_t modtime;		/* Modification time. */
};

int
utime (char *filename, struct utimbuf *utb)
{
  struct tm *tm;
  struct ftime filetime;
  time_t when;
  int fd;
  int status;

  if (utb == 0)
    when = time (0);
  else
    when = utb->modtime;

  fd = _open (filename, O_RDWR);
  if (fd == -1)
    return -1;

  tm = localtime (&when);
  if (tm->tm_year < 80)
    filetime.ft_year = 0;
  else
    filetime.ft_year = tm->tm_year - 80;
  filetime.ft_month = tm->tm_mon + 1;
  filetime.ft_day = tm->tm_mday;
  if (tm->tm_hour < 0)
    filetime.ft_hour = 0;
  else
    filetime.ft_hour = tm->tm_hour;
  filetime.ft_min = tm->tm_min;
  filetime.ft_tsec = tm->tm_sec / 2;

  status = setftime (fd, &filetime);
  _close (fd);
  return status;
}

#endif /* __TURBOC__ */

/* Stash argv[0] here so panic will know what the program is called */
char *myname = 0;

void
panic (s)
     char *s;
{
  if (myname)
    fprintf (stderr, "%s:", myname);
  fprintf (stderr, s);
  putc ('\n', stderr);
  exit (12);
}


PTR
ck_malloc (size)
     size_t size;
{
  PTR ret;

  if (!size)
    size++;
  ret = malloc (size);
  if (ret == 0)
    panic ("Couldn't allocate memory");
  return ret;
}

/* Used by alloca.c and bison.simple. */
char *
xmalloc (size)
     size_t size;
{
  return (char *) ck_malloc (size);
}

PTR
ck_realloc (ptr, size)
     PTR ptr;
     size_t size;
{
  PTR ret;

  if (!ptr)
    ret = ck_malloc (size);
  else
    ret = realloc (ptr, size);
  if (ret == 0)
    panic ("Couldn't re-allocate memory");
  return ret;
}

/* Implement a variable sized buffer of 'stuff'.  We don't know what it is,
   nor do we care, as long as it doesn't mind being aligned on a char boundry.
 */

struct buffer
  {
    int allocated;
    int length;
    char *b;
  };

#define MIN_ALLOCATE 50

char *
init_buffer ()
{
  struct buffer *b;

  b = (struct buffer *) ck_malloc (sizeof (struct buffer));
  b->allocated = MIN_ALLOCATE;
  b->b = (char *) ck_malloc (MIN_ALLOCATE);
  b->length = 0;
  return (char *) b;
}

void
flush_buffer (bb)
     char *bb;
{
  struct buffer *b;

  b = (struct buffer *) bb;
  free (b->b);
  b->b = 0;
  b->allocated = 0;
  b->length = 0;
  free ((void *) b);
}

void
add_buffer (bb, p, n)
     char *bb;
     char *p;
     int n;
{
  struct buffer *b;

  b = (struct buffer *) bb;
  if (b->length + n > b->allocated)
    {
      b->allocated = b->length + n + MIN_ALLOCATE;
      b->b = (char *) ck_realloc (b->b, b->allocated);
    }
  bcopy (p, b->b + b->length, n);
  b->length += n;
}

char *
get_buffer (bb)
     char *bb;
{
  struct buffer *b;

  b = (struct buffer *) bb;
  return b->b;
}

char *
merge_sort (list, n, off, cmp)
     char *list;
     int (*cmp) ();
     unsigned n;
     int off;
{
  char *ret;

  char *alist, *blist;
  unsigned alength, blength;

  char *tptr;
  int tmp;
  char **prev;
#define NEXTOF(ptr)	(* ((char **)(((char *)(ptr))+off) ) )
  if (n == 1)
    return list;
  if (n == 2)
    {
      if ((*cmp) (list, NEXTOF (list)) > 0)
	{
	  ret = NEXTOF (list);
	  NEXTOF (ret) = list;
	  NEXTOF (list) = 0;
	  return ret;
	}
      return list;
    }
  alist = list;
  alength = (n + 1) / 2;
  blength = n / 2;
  for (tptr = list, tmp = (n - 1) / 2; tmp; tptr = NEXTOF (tptr), tmp--)
    ;
  blist = NEXTOF (tptr);
  NEXTOF (tptr) = 0;

  alist = merge_sort (alist, alength, off, cmp);
  blist = merge_sort (blist, blength, off, cmp);
  prev = &ret;
  for (; alist && blist;)
    {
      if ((*cmp) (alist, blist) < 0)
	{
	  tptr = NEXTOF (alist);
	  *prev = alist;
	  prev = &(NEXTOF (alist));
	  alist = tptr;
	}
      else
	{
	  tptr = NEXTOF (blist);
	  *prev = blist;
	  prev = &(NEXTOF (blist));
	  blist = tptr;
	}
    }
  if (alist)
    *prev = alist;
  else
    *prev = blist;

  return ret;
}

void
ck_close (fd)
     int fd;
{
  if (close (fd) < 0)
    {
      msg_perror ("can't close a file #%d", fd);
      exit (EX_SYSTEM);
    }
}

#include <ctype.h>

/* Quote_copy_string is like quote_string, but instead of modifying the
   string in place, it malloc-s a copy  of the string, and returns that.
   If the string does not have to be quoted, it returns the NULL string.
   The allocated copy can, of course, be freed with free() after the
   caller is done with it.
 */
char *
quote_copy_string (string)
     char *string;
{
  char *from_here;
  char *to_there = 0;
  char *copy_buf = 0;
  int c;
  int copying = 0;

  from_here = string;
  while (*from_here)
    {
      c = *from_here++ & 0xFF;
      if (c == '\\')
	{
	  if (!copying)
	    {
	      int n;

	      n = (from_here - string) - 1;
	      copying++;
	      copy_buf = (char *) malloc (n + 5 + strlen (from_here) * 4);
	      if (!copy_buf)
		return 0;
	      bcopy (string, copy_buf, n);
	      to_there = copy_buf + n;
	    }
	  *to_there++ = '\\';
	  *to_there++ = '\\';
	}
      else if (isprint (c))
	{
	  if (copying)
	    *to_there++ = c;
	}
      else
	{
	  if (!copying)
	    {
	      int n;

	      n = (from_here - string) - 1;
	      copying++;
	      copy_buf = (char *) malloc (n + 5 + strlen (from_here) * 4);
	      if (!copy_buf)
		return 0;
	      bcopy (string, copy_buf, n);
	      to_there = copy_buf + n;
	    }
	  *to_there++ = '\\';
	  if (c == '\n')
	    *to_there++ = 'n';
	  else if (c == '\t')
	    *to_there++ = 't';
	  else if (c == '\f')
	    *to_there++ = 'f';
	  else if (c == '\b')
	    *to_there++ = 'b';
	  else if (c == '\r')
	    *to_there++ = 'r';
	  else if (c == '\177')
	    *to_there++ = '?';
	  else
	    {
	      to_there[0] = (c >> 6) + '0';
	      to_there[1] = ((c >> 3) & 07) + '0';
	      to_there[2] = (c & 07) + '0';
	      to_there += 3;
	    }
	}
    }
  if (copying)
    {
      *to_there = '\0';
      return copy_buf;
    }
  return (char *) 0;
}


/* Un_quote_string takes a quoted c-string (like those produced by
   quote_string or quote_copy_string and turns it back into the
   un-quoted original.  This is done in place.
 */

/* There is no un-quote-copy-string.  Write it yourself */

char *
un_quote_string (string)
     char *string;
{
  char *ret;
  char *from_here;
  char *to_there;
  int tmp;

  ret = string;
  to_there = string;
  from_here = string;
  while (*from_here)
    {
      if (*from_here != '\\')
	{
	  if (from_here != to_there)
	    *to_there++ = *from_here++;
	  else
	    from_here++, to_there++;
	  continue;
	}
      switch (*++from_here)
	{
	case '\\':
	  *to_there++ = *from_here++;
	  break;
	case 'n':
	  *to_there++ = '\n';
	  from_here++;
	  break;
	case 't':
	  *to_there++ = '\t';
	  from_here++;
	  break;
	case 'f':
	  *to_there++ = '\f';
	  from_here++;
	  break;
	case 'b':
	  *to_there++ = '\b';
	  from_here++;
	  break;
	case 'r':
	  *to_there++ = '\r';
	  from_here++;
	  break;
	case '?':
	  *to_there++ = 0177;
	  from_here++;
	  break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	  tmp = *from_here - '0';
	  from_here++;
	  if (*from_here < '0' || *from_here > '7')
	    {
	      *to_there++ = tmp;
	      break;
	    }
	  tmp = tmp * 8 + *from_here - '0';
	  from_here++;
	  if (*from_here < '0' || *from_here > '7')
	    {
	      *to_there++ = tmp;
	      break;
	    }
	  tmp = tmp * 8 + *from_here - '0';
	  from_here++;
	  *to_there = tmp;
	  break;
	default:
	  ret = 0;
	  *to_there++ = '\\';
	  *to_there++ = *from_here++;
	  break;
	}
    }
  if (*to_there)
    *to_there++ = '\0';
  return ret;
}

#ifndef __MSDOS__
void
ck_pipe (pipes)
     int *pipes;
{
  if (pipe (pipes) < 0)
    {
      msg_perror ("can't open a pipe");
      exit (EX_SYSTEM);
    }
}
#endif /* !__MSDOS__ */

#ifndef HAVE_STRSTR
/*
 * strstr - find first occurrence of wanted in s
 */

char *				/* found string, or NULL if none */
strstr (s, wanted)
     char *s;
     char *wanted;
{
  register char *scan;
  register size_t len;
  register char firstc;

  if (*wanted == '\0')
    return (char *) 0;
  /*
	 * The odd placement of the two tests is so "" is findable.
	 * Also, we inline the first char for speed.
	 * The ++ on scan has been moved down for optimization.
	 */
  firstc = *wanted;
  len = strlen (wanted);
  for (scan = s; *scan != firstc || strncmp (scan, wanted, len) != 0;)
    if (*scan++ == '\0')
      return (char *) 0;
  return scan;
}

#endif /* !HAVE_STRSTR */

#ifndef HAVE_FTRUNCATE

#ifdef F_CHSIZE
int
ftruncate (fd, length)
     int fd;
     off_t length;
{
  return fcntl (fd, F_CHSIZE, length);
}

#else /* !F_CHSIZE */
#ifdef F_FREESP
/* code courtesy of William Kucharski, kucharsk@Solbourne.com */

int
ftruncate (fd, length)
     int fd;			/* file descriptor */
     off_t length;		/* length to set file to */
{
  struct flock fl;

  fl.l_whence = 0;
  fl.l_len = 0;
  fl.l_start = length;
  fl.l_type = F_WRLCK;		/* write lock on file space */

  /*
	 * This relies on the UNDOCUMENTED F_FREESP argument to
	 * fcntl(2), which truncates the file so that it ends at the
	 * position indicated by fl.l_start.
	 *
	 * Will minor miracles never cease?
	 */

  if (fcntl (fd, F_FREESP, &fl) < 0)
    return -1;

  return 0;
}

#else /* !F_FREESP */

int
ftruncate (fd, length)
     int fd;
     off_t length;
{
  errno = EIO;
  return -1;
}

#endif /* !F_FREESP */
#endif /* !F_CHSIZE */
#endif /* !HAVE_FTRUNCATE */


extern FILE *msg_file;

#if defined (HAVE_VPRINTF) && __STDC__
#include <stdarg.h>

void
msg (char *str,...)
{
  va_list args;

  va_start (args, str);
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  vfprintf (stderr, str, args);
  va_end (args);
  putc ('\n', stderr);
  fflush (stderr);
}

void
msg_perror (char *str,...)
{
  va_list args;
  int save_e;

  save_e = errno;
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  va_start (args, str);
  vfprintf (stderr, str, args);
  va_end (args);
  errno = save_e;
  perror (" ");
  fflush (stderr);
}

#endif /* HAVE_VPRINTF and __STDC__ */

#if defined(HAVE_VPRINTF) && !__STDC__
#include <varargs.h>
void
msg (str, va_alist)
     char *str;
     va_dcl
{
  va_list args;

  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  va_start (args);
  vfprintf (stderr, str, args);
  va_end (args);
  putc ('\n', stderr);
  fflush (stderr);
}

void
msg_perror (str, va_alist)
     char *str;
     va_dcl
{
  va_list args;
  int save_e;

  save_e = errno;
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  va_start (args);
  vfprintf (stderr, str, args);
  va_end (args);
  errno = save_e;
  perror (" ");
  fflush (stderr);
}

#endif /* HAVE_VPRINTF and not __STDC__ */

#if !defined(HAVE_VPRINTF) && defined(HAVE_DOPRNT)
void
msg (str, args)
     char *str;
     int args;
{
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  _doprnt (str, &args, stderr);
  putc ('\n', stderr);
  fflush (stderr);
}

void
msg_perror (str, args)
     char *str;
     int args;
{
  int save_e;

  save_e = errno;
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  _doprnt (str, &args, stderr);
  errno = save_e;
  perror (" ");
  fflush (stderr);
}

#endif /* !HAVE_VPRINTF and HAVE_DOPRNT */

#if !defined(HAVE_VPRINTF) && !defined(HAVE_DOPRNT)
void
msg (str, a1, a2, a3, a4, a5, a6)
     char *str;
{
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  fprintf (stderr, str, a1, a2, a3, a4, a5, a6);
  putc ('\n', stderr);
  fflush (stderr);
}

void
msg_perror (str, a1, a2, a3, a4, a5, a6)
     char *str;
{
  int save_e;

  save_e = errno;
  fflush (msg_file);
  fprintf (stderr, "%s: ", tar);
  if (f_sayblock)
    fprintf (stderr, "rec %d: ", baserec + (ar_record - ar_block));
  fprintf (stderr, str, a1, a2, a3, a4, a5, a6);
  fprintf (stderr, ": ");
  errno = save_e;
  perror (" ");
}

#endif /* !HAVE_VPRINTF and !HAVE_DOPRNT */
