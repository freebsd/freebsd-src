/* Extended support for using errno values.
   Copyright (C) 1992 Free Software Foundation, Inc.
   Written by Fred Fish.  fnf@cygnus.com

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include "config.h"

#include <stdio.h>
#ifndef NEED_sys_errlist
/* Note that errno.h might declare sys_errlist in a way that the
 * compiler might consider incompatible with our later declaration,
 * perhaps by using const attributes.  So we hide the declaration
 * in errno.h (if any) using a macro. */
#define sys_errlist sys_errlist__
#endif
#include <errno.h>
#ifndef NEED_sys_errlist
#undef sys_errlist
#endif

/*  Routines imported from standard C runtime libraries. */

#ifdef __STDC__
#include <stddef.h>
extern void *malloc (size_t size);				/* 4.10.3.3 */
extern void *memset (void *s, int c, size_t n);			/* 4.11.6.1 */
#else	/* !__STDC__ */
#ifndef const
#define const
#endif
extern char *malloc ();		/* Standard memory allocater */
extern char *memset ();
#endif	/* __STDC__ */

#ifndef NULL
#  ifdef __STDC__
#    define NULL (void *) 0
#  else
#    define NULL 0
#  endif
#endif

#ifndef MAX
#  define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* Translation table for errno values.  See intro(2) in most UNIX systems
   Programmers Reference Manuals.

   Note that this table is generally only accessed when it is used at runtime
   to initialize errno name and message tables that are indexed by errno
   value.

   Not all of these errnos will exist on all systems.  This table is the only
   thing that should have to be updated as new error numbers are introduced.
   It's sort of ugly, but at least its portable. */

struct error_info
{
  int value;		/* The numeric value from <errno.h> */
  char *name;		/* The equivalent symbolic value */
  char *msg;		/* Short message about this value */
};

static const struct error_info error_table[] =
{
#if defined (EPERM)
  EPERM, "EPERM", "Not owner",
#endif
#if defined (ENOENT)
  ENOENT, "ENOENT", "No such file or directory",
#endif
#if defined (ESRCH)
  ESRCH, "ESRCH", "No such process",
#endif
#if defined (EINTR)
  EINTR, "EINTR", "Interrupted system call",
#endif
#if defined (EIO)
  EIO, "EIO", "I/O error",
#endif
#if defined (ENXIO)
  ENXIO, "ENXIO", "No such device or address",
#endif
#if defined (E2BIG)
  E2BIG, "E2BIG", "Arg list too long",
#endif
#if defined (ENOEXEC)
  ENOEXEC, "ENOEXEC", "Exec format error",
#endif
#if defined (EBADF)
  EBADF, "EBADF", "Bad file number",
#endif
#if defined (ECHILD)
  ECHILD, "ECHILD", "No child processes",
#endif
#if defined (EWOULDBLOCK)	/* Put before EAGAIN, sometimes aliased */
  EWOULDBLOCK, "EWOULDBLOCK", "Operation would block",
#endif
#if defined (EAGAIN)
  EAGAIN, "EAGAIN", "No more processes",
#endif
#if defined (ENOMEM)
  ENOMEM, "ENOMEM", "Not enough space",
#endif
#if defined (EACCES)
  EACCES, "EACCES", "Permission denied",
#endif
#if defined (EFAULT)
  EFAULT, "EFAULT", "Bad address",
#endif
#if defined (ENOTBLK)
  ENOTBLK, "ENOTBLK", "Block device required",
#endif
#if defined (EBUSY)
  EBUSY, "EBUSY", "Device busy",
#endif
#if defined (EEXIST)
  EEXIST, "EEXIST", "File exists",
#endif
#if defined (EXDEV)
  EXDEV, "EXDEV", "Cross-device link",
#endif
#if defined (ENODEV)
  ENODEV, "ENODEV", "No such device",
#endif
#if defined (ENOTDIR)
  ENOTDIR, "ENOTDIR", "Not a directory",
#endif
#if defined (EISDIR)
  EISDIR, "EISDIR", "Is a directory",
#endif
#if defined (EINVAL)
  EINVAL, "EINVAL", "Invalid argument",
#endif
#if defined (ENFILE)
  ENFILE, "ENFILE", "File table overflow",
#endif
#if defined (EMFILE)
  EMFILE, "EMFILE", "Too many open files",
#endif
#if defined (ENOTTY)
  ENOTTY, "ENOTTY", "Not a typewriter",
#endif
#if defined (ETXTBSY)
  ETXTBSY, "ETXTBSY", "Text file busy",
#endif
#if defined (EFBIG)
  EFBIG, "EFBIG", "File too large",
#endif
#if defined (ENOSPC)
  ENOSPC, "ENOSPC", "No space left on device",
#endif
#if defined (ESPIPE)
  ESPIPE, "ESPIPE", "Illegal seek",
#endif
#if defined (EROFS)
  EROFS, "EROFS", "Read-only file system",
#endif
#if defined (EMLINK)
  EMLINK, "EMLINK", "Too many links",
#endif
#if defined (EPIPE)
  EPIPE, "EPIPE", "Broken pipe",
#endif
#if defined (EDOM)
  EDOM, "EDOM", "Math argument out of domain of func",
#endif
#if defined (ERANGE)
  ERANGE, "ERANGE", "Math result not representable",
#endif
#if defined (ENOMSG)
  ENOMSG, "ENOMSG", "No message of desired type",
#endif
#if defined (EIDRM)
  EIDRM, "EIDRM", "Identifier removed",
#endif
#if defined (ECHRNG)
  ECHRNG, "ECHRNG", "Channel number out of range",
#endif
#if defined (EL2NSYNC)
  EL2NSYNC, "EL2NSYNC", "Level 2 not synchronized",
#endif
#if defined (EL3HLT)
  EL3HLT, "EL3HLT", "Level 3 halted",
#endif
#if defined (EL3RST)
  EL3RST, "EL3RST", "Level 3 reset",
#endif
#if defined (ELNRNG)
  ELNRNG, "ELNRNG", "Link number out of range",
#endif
#if defined (EUNATCH)
  EUNATCH, "EUNATCH", "Protocol driver not attached",
#endif
#if defined (ENOCSI)
  ENOCSI, "ENOCSI", "No CSI structure available",
#endif
#if defined (EL2HLT)
  EL2HLT, "EL2HLT", "Level 2 halted",
#endif
#if defined (EDEADLK)
  EDEADLK, "EDEADLK", "Deadlock condition",
#endif
#if defined (ENOLCK)
  ENOLCK, "ENOLCK", "No record locks available",
#endif
#if defined (EBADE)
  EBADE, "EBADE", "Invalid exchange",
#endif
#if defined (EBADR)
  EBADR, "EBADR", "Invalid request descriptor",
#endif
#if defined (EXFULL)
  EXFULL, "EXFULL", "Exchange full",
#endif
#if defined (ENOANO)
  ENOANO, "ENOANO", "No anode",
#endif
#if defined (EBADRQC)
  EBADRQC, "EBADRQC", "Invalid request code",
#endif
#if defined (EBADSLT)
  EBADSLT, "EBADSLT", "Invalid slot",
#endif
#if defined (EDEADLOCK)
  EDEADLOCK, "EDEADLOCK", "File locking deadlock error",
#endif
#if defined (EBFONT)
  EBFONT, "EBFONT", "Bad font file format",
#endif
#if defined (ENOSTR)
  ENOSTR, "ENOSTR", "Device not a stream",
#endif
#if defined (ENODATA)
  ENODATA, "ENODATA", "No data available",
#endif
#if defined (ETIME)
  ETIME, "ETIME", "Timer expired",
#endif
#if defined (ENOSR)
  ENOSR, "ENOSR", "Out of streams resources",
#endif
#if defined (ENONET)
  ENONET, "ENONET", "Machine is not on the network",
#endif
#if defined (ENOPKG)
  ENOPKG, "ENOPKG", "Package not installed",
#endif
#if defined (EREMOTE)
  EREMOTE, "EREMOTE", "Object is remote",
#endif
#if defined (ENOLINK)
  ENOLINK, "ENOLINK", "Link has been severed",
#endif
#if defined (EADV)
  EADV, "EADV", "Advertise error",
#endif
#if defined (ESRMNT)
  ESRMNT, "ESRMNT", "Srmount error",
#endif
#if defined (ECOMM)
  ECOMM, "ECOMM", "Communication error on send",
#endif
#if defined (EPROTO)
  EPROTO, "EPROTO", "Protocol error",
#endif
#if defined (EMULTIHOP)
  EMULTIHOP, "EMULTIHOP", "Multihop attempted",
#endif
#if defined (EDOTDOT)
  EDOTDOT, "EDOTDOT", "RFS specific error",
#endif
#if defined (EBADMSG)
  EBADMSG, "EBADMSG", "Not a data message",
#endif
#if defined (ENAMETOOLONG)
  ENAMETOOLONG, "ENAMETOOLONG", "File name too long",
#endif
#if defined (EOVERFLOW)
  EOVERFLOW, "EOVERFLOW", "Value too large for defined data type",
#endif
#if defined (ENOTUNIQ)
  ENOTUNIQ, "ENOTUNIQ", "Name not unique on network",
#endif
#if defined (EBADFD)
  EBADFD, "EBADFD", "File descriptor in bad state",
#endif
#if defined (EREMCHG)
  EREMCHG, "EREMCHG", "Remote address changed",
#endif
#if defined (ELIBACC)
  ELIBACC, "ELIBACC", "Can not access a needed shared library",
#endif
#if defined (ELIBBAD)
  ELIBBAD, "ELIBBAD", "Accessing a corrupted shared library",
#endif
#if defined (ELIBSCN)
  ELIBSCN, "ELIBSCN", ".lib section in a.out corrupted",
#endif
#if defined (ELIBMAX)
  ELIBMAX, "ELIBMAX", "Attempting to link in too many shared libraries",
#endif
#if defined (ELIBEXEC)
  ELIBEXEC, "ELIBEXEC", "Cannot exec a shared library directly",
#endif
#if defined (EILSEQ)
  EILSEQ, "EILSEQ", "Illegal byte sequence",
#endif
#if defined (ENOSYS)
  ENOSYS, "ENOSYS", "Operation not applicable",
#endif
#if defined (ELOOP)
  ELOOP, "ELOOP", "Too many symbolic links encountered",
#endif
#if defined (ERESTART)
  ERESTART, "ERESTART", "Interrupted system call should be restarted",
#endif
#if defined (ESTRPIPE)
  ESTRPIPE, "ESTRPIPE", "Streams pipe error",
#endif
#if defined (ENOTEMPTY)
  ENOTEMPTY, "ENOTEMPTY", "Directory not empty",
#endif
#if defined (EUSERS)
  EUSERS, "EUSERS", "Too many users",
#endif
#if defined (ENOTSOCK)
  ENOTSOCK, "ENOTSOCK", "Socket operation on non-socket",
#endif
#if defined (EDESTADDRREQ)
  EDESTADDRREQ, "EDESTADDRREQ", "Destination address required",
#endif
#if defined (EMSGSIZE)
  EMSGSIZE, "EMSGSIZE", "Message too long",
#endif
#if defined (EPROTOTYPE)
  EPROTOTYPE, "EPROTOTYPE", "Protocol wrong type for socket",
#endif
#if defined (ENOPROTOOPT)
  ENOPROTOOPT, "ENOPROTOOPT", "Protocol not available",
#endif
#if defined (EPROTONOSUPPORT)
  EPROTONOSUPPORT, "EPROTONOSUPPORT", "Protocol not supported",
#endif
#if defined (ESOCKTNOSUPPORT)
  ESOCKTNOSUPPORT, "ESOCKTNOSUPPORT", "Socket type not supported",
#endif
#if defined (EOPNOTSUPP)
  EOPNOTSUPP, "EOPNOTSUPP", "Operation not supported on transport endpoint",
#endif
#if defined (EPFNOSUPPORT)
  EPFNOSUPPORT, "EPFNOSUPPORT", "Protocol family not supported",
#endif
#if defined (EAFNOSUPPORT)
  EAFNOSUPPORT, "EAFNOSUPPORT", "Address family not supported by protocol",
#endif
#if defined (EADDRINUSE)
  EADDRINUSE, "EADDRINUSE", "Address already in use",
#endif
#if defined (EADDRNOTAVAIL)
  EADDRNOTAVAIL, "EADDRNOTAVAIL","Cannot assign requested address",
#endif
#if defined (ENETDOWN)
  ENETDOWN, "ENETDOWN", "Network is down",
#endif
#if defined (ENETUNREACH)
  ENETUNREACH, "ENETUNREACH", "Network is unreachable",
#endif
#if defined (ENETRESET)
  ENETRESET, "ENETRESET", "Network dropped connection because of reset",
#endif
#if defined (ECONNABORTED)
  ECONNABORTED, "ECONNABORTED", "Software caused connection abort",
#endif
#if defined (ECONNRESET)
  ECONNRESET, "ECONNRESET", "Connection reset by peer",
#endif
#if defined (ENOBUFS)
  ENOBUFS, "ENOBUFS", "No buffer space available",
#endif
#if defined (EISCONN)
  EISCONN, "EISCONN", "Transport endpoint is already connected",
#endif
#if defined (ENOTCONN)
  ENOTCONN, "ENOTCONN", "Transport endpoint is not connected",
#endif
#if defined (ESHUTDOWN)
  ESHUTDOWN, "ESHUTDOWN", "Cannot send after transport endpoint shutdown",
#endif
#if defined (ETOOMANYREFS)
  ETOOMANYREFS, "ETOOMANYREFS", "Too many references: cannot splice",
#endif
#if defined (ETIMEDOUT)
  ETIMEDOUT, "ETIMEDOUT", "Connection timed out",
#endif
#if defined (ECONNREFUSED)
  ECONNREFUSED, "ECONNREFUSED", "Connection refused",
#endif
#if defined (EHOSTDOWN)
  EHOSTDOWN, "EHOSTDOWN", "Host is down",
#endif
#if defined (EHOSTUNREACH)
  EHOSTUNREACH, "EHOSTUNREACH", "No route to host",
#endif
#if defined (EALREADY)
  EALREADY, "EALREADY", "Operation already in progress",
#endif
#if defined (EINPROGRESS)
  EINPROGRESS, "EINPROGRESS", "Operation now in progress",
#endif
#if defined (ESTALE)
  ESTALE, "ESTALE", "Stale NFS file handle",
#endif
#if defined (EUCLEAN)
  EUCLEAN, "EUCLEAN", "Structure needs cleaning",
#endif
#if defined (ENOTNAM)
  ENOTNAM, "ENOTNAM", "Not a XENIX named type file",
#endif
#if defined (ENAVAIL)
  ENAVAIL, "ENAVAIL", "No XENIX semaphores available",
#endif
#if defined (EISNAM)
  EISNAM, "EISNAM", "Is a named type file",
#endif
#if defined (EREMOTEIO)
  EREMOTEIO, "EREMOTEIO", "Remote I/O error",
#endif
  0, NULL, NULL
};

/* Translation table allocated and initialized at runtime.  Indexed by the
   errno value to find the equivalent symbolic value. */

static char **error_names;
static int num_error_names = 0;

/* Translation table allocated and initialized at runtime, if it does not
   already exist in the host environment.  Indexed by the errno value to find
   the descriptive string.

   We don't export it for use in other modules because even though it has the
   same name, it differs from other implementations in that it is dynamically
   initialized rather than statically initialized. */

#ifdef NEED_sys_errlist

static int sys_nerr;
static char **sys_errlist;

#else

extern int sys_nerr;
extern char *sys_errlist[];

#endif


/*

NAME

	init_error_tables -- initialize the name and message tables

SYNOPSIS

	static void init_error_tables ();

DESCRIPTION

	Using the error_table, which is initialized at compile time, generate
	the error_names and the sys_errlist (if needed) tables, which are
	indexed at runtime by a specific errno value.

BUGS

	The initialization of the tables may fail under low memory conditions,
	in which case we don't do anything particularly useful, but we don't
	bomb either.  Who knows, it might succeed at a later point if we free
	some memory in the meantime.  In any case, the other routines know
	how to deal with lack of a table after trying to initialize it.  This
	may or may not be considered to be a bug, that we don't specifically
	warn about this particular failure mode.

*/

static void
init_error_tables ()
{
  const struct error_info *eip;
  int nbytes;

  /* If we haven't already scanned the error_table once to find the maximum
     errno value, then go find it now. */

  if (num_error_names == 0)
    {
      for (eip = error_table; eip -> name != NULL; eip++)
	{
	  if (eip -> value >= num_error_names)
	    {
	      num_error_names = eip -> value + 1;
	    }
	}
    }

  /* Now attempt to allocate the error_names table, zero it out, and then
     initialize it from the statically initialized error_table. */

  if (error_names == NULL)
    {
      nbytes = num_error_names * sizeof (char *);
      if ((error_names = (char **) malloc (nbytes)) != NULL)
	{
	  memset (error_names, 0, nbytes);
	  for (eip = error_table; eip -> name != NULL; eip++)
	    {
	      error_names[eip -> value] = eip -> name;
	    }
	}
    }

#ifdef NEED_sys_errlist

  /* Now attempt to allocate the sys_errlist table, zero it out, and then
     initialize it from the statically initialized error_table. */

  if (sys_errlist == NULL)
    {
      nbytes = num_error_names * sizeof (char *);
      if ((sys_errlist = (char **) malloc (nbytes)) != NULL)
	{
	  memset (sys_errlist, 0, nbytes);
	  sys_nerr = num_error_names;
	  for (eip = error_table; eip -> name != NULL; eip++)
	    {
	      sys_errlist[eip -> value] = eip -> msg;
	    }
	}
    }

#endif

}

/*

NAME

	errno_max -- return the max errno value

SYNOPSIS

	int errno_max ();

DESCRIPTION

	Returns the maximum errno value for which a corresponding symbolic
	name or message is available.  Note that in the case where
	we use the sys_errlist supplied by the system, it is possible for
	there to be more symbolic names than messages, or vice versa.
	In fact, the manual page for perror(3C) explicitly warns that one
	should check the size of the table (sys_nerr) before indexing it,
	since new error codes may be added to the system before they are
	added to the table.  Thus sys_nerr might be smaller than value
	implied by the largest errno value defined in <errno.h>.

	We return the maximum value that can be used to obtain a meaningful
	symbolic name or message.

*/

int
errno_max ()
{
  int maxsize;

  if (error_names == NULL)
    {
      init_error_tables ();
    }
  maxsize = MAX (sys_nerr, num_error_names);
  return (maxsize - 1);
}

#ifdef NEED_strerror

/*

NAME

	strerror -- map an error number to an error message string

SYNOPSIS

	char *strerror (int errnoval)

DESCRIPTION

	Maps an errno number to an error message string, the contents of
	which are implementation defined.  On systems which have the external
	variables sys_nerr and sys_errlist, these strings will be the same
	as the ones used by perror().

	If the supplied error number is within the valid range of indices
	for the sys_errlist, but no message is available for the particular
	error number, then returns the string "Error NUM", where NUM is the
	error number.

	If the supplied error number is not a valid index into sys_errlist,
	returns NULL.

	The returned string is only guaranteed to be valid only until the
	next call to strerror.

*/

char *
strerror (errnoval)
  int errnoval;
{
  char *msg;
  static char buf[32];

#ifdef NEED_sys_errlist

  if (error_names == NULL)
    {
      init_error_tables ();
    }

#endif

  if ((errnoval < 0) || (errnoval >= sys_nerr))
    {
      /* Out of range, just return NULL */
      msg = NULL;
    }
  else if ((sys_errlist == NULL) || (sys_errlist[errnoval] == NULL))
    {
      /* In range, but no sys_errlist or no entry at this index. */
      sprintf (buf, "Error %d", errnoval);
      msg = buf;
    }
  else
    {
      /* In range, and a valid message.  Just return the message. */
      msg = sys_errlist[errnoval];
    }
  
  return (msg);
}

#endif	/* NEED_strerror */


/*

NAME

	strerrno -- map an error number to a symbolic name string

SYNOPSIS

	char *strerrno (int errnoval)

DESCRIPTION

	Given an error number returned from a system call (typically
	returned in errno), returns a pointer to a string containing the
	symbolic name of that error number, as found in <errno.h>.

	If the supplied error number is within the valid range of indices
	for symbolic names, but no name is available for the particular
	error number, then returns the string "Error NUM", where NUM is
	the error number.

	If the supplied error number is not within the range of valid
	indices, then returns NULL.

BUGS

	The contents of the location pointed to are only guaranteed to be
	valid until the next call to strerrno.

*/

char *
strerrno (errnoval)
  int errnoval;
{
  char *name;
  static char buf[32];

  if (error_names == NULL)
    {
      init_error_tables ();
    }

  if ((errnoval < 0) || (errnoval >= num_error_names))
    {
      /* Out of range, just return NULL */
      name = NULL;
    }
  else if ((error_names == NULL) || (error_names[errnoval] == NULL))
    {
      /* In range, but no error_names or no entry at this index. */
      sprintf (buf, "Error %d", errnoval);
      name = buf;
    }
  else
    {
      /* In range, and a valid name.  Just return the name. */
      name = error_names[errnoval];
    }

  return (name);
}

/*

NAME

	strtoerrno -- map a symbolic errno name to a numeric value

SYNOPSIS

	int strtoerrno (char *name)

DESCRIPTION

	Given the symbolic name of a error number, map it to an errno value.
	If no translation is found, returns 0.

*/

int
strtoerrno (name)
  char *name;
{
  int errnoval = 0;

  if (name != NULL)
    {
      if (error_names == NULL)
	{
	  init_error_tables ();
	}
      for (errnoval = 0; errnoval < num_error_names; errnoval++)
	{
	  if ((error_names[errnoval] != NULL) &&
	      (strcmp (name, error_names[errnoval]) == 0))
	    {
	      break;
	    }
	}
      if (errnoval == num_error_names)
	{
	  errnoval = 0;
	}
    }
  return (errnoval);
}


/* A simple little main that does nothing but print all the errno translations
   if MAIN is defined and this file is compiled and linked. */

#ifdef MAIN

main ()
{
  int errn;
  int errnmax;
  char *name;
  char *msg;
  char *strerrno ();
  char *strerror ();

  errnmax = errno_max ();
  printf ("%d entries in names table.\n", num_error_names);
  printf ("%d entries in messages table.\n", sys_nerr);
  printf ("%d is max useful index.\n", errnmax);

  /* Keep printing values until we get to the end of *both* tables, not
     *either* table.  Note that knowing the maximum useful index does *not*
     relieve us of the responsibility of testing the return pointer for
     NULL. */

  for (errn = 0; errn <= errnmax; errn++)
    {
      name = strerrno (errn);
      name = (name == NULL) ? "<NULL>" : name;
      msg = strerror (errn);
      msg = (msg == NULL) ? "<NULL>" : msg;
      printf ("%-4d%-18s%s\n", errn, name, msg);
    }
}

#endif
