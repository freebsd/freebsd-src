/* efopen.c
   Open a stdio file with appropriate permissions.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#ifndef O_APPEND
#ifdef FAPPEND
#define O_APPEND FAPPEND
#endif
#endif

#ifndef O_NOCTTY
#define O_NOCTTY 0
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

FILE *
esysdep_fopen (zfile, fpublic, fappend, fmkdirs)
     const char *zfile;
     boolean fpublic;
     boolean fappend;
     boolean fmkdirs;
{
  int imode;
  int o;
  FILE *e;

  if (fpublic)
    imode = IPUBLIC_FILE_MODE;
  else
    imode = IPRIVATE_FILE_MODE;

  if (! fappend)
    o = creat ((char *) zfile, imode);
  else
    {
#ifdef O_CREAT
      o = open ((char *) zfile,
		O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY,
		imode);
#else
      o = open ((char *) zfile, O_WRONLY | O_NOCTTY);
      if (o < 0 && errno == ENOENT)
	o = creat ((char *) zfile, imode);
#endif /* ! defined (O_CREAT) */
    }

  if (o < 0)
    {
      if (errno == ENOENT && fmkdirs)
	{
	  if (! fsysdep_make_dirs (zfile, fpublic))
	    return NULL;
	  if (! fappend)
	    o = creat ((char *) zfile, imode);
	  else
	    {
#ifdef O_CREAT
	      o = open ((char *) zfile,
			O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY,
			imode);
#else
	      o = creat ((char *) zfile, imode);
#endif
	    }
	}
      if (o < 0)
	{
	  ulog (LOG_ERROR, "open (%s): %s", zfile, strerror (errno));
	  return NULL;
	}
    }

#ifndef O_CREAT
#ifdef O_APPEND
  if (fappend)
    {
      if (fcntl (o, F_SETFL, O_APPEND) < 0)
	{
	  ulog (LOG_ERROR, "fcntl (%s, O_APPEND): %s", zfile,
		strerror (errno));
	  (void) close (o);
	  return NULL;
	}
    }
#endif /* defined (O_APPEND) */
#endif /* ! defined (O_CREAT) */

  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (%s, FD_CLOEXEC): %s", zfile,
	    strerror (errno));
      (void) close (o);
      return NULL;
    }

  if (fappend)
    e = fdopen (o, (char *) "a");
  else
    e = fdopen (o, (char *) "w");

  if (e == NULL)
    {
      ulog (LOG_ERROR, "fdopen: %s", strerror (errno));
      (void) close (o);
    }

  return e;
}
