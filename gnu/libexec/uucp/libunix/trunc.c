/* trunc.c
   Truncate a file to zero length.  */

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

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

/* External functions.  */
#ifndef lseek
extern off_t lseek ();
#endif

/* Truncate a file to zero length.  If this fails, it closes and
   removes the file.  We support a number of different means of
   truncation, which is probably a waste of time since this function
   is currently only called when the 'f' protocol resends a file.  */

#if HAVE_FTRUNCATE
#undef HAVE_LTRUNC
#define HAVE_LTRUNC 0
#endif

#if ! HAVE_FTRUNCATE && ! HAVE_LTRUNC
#ifdef F_CHSIZE
#define HAVE_F_CHSIZE 1
#else /* ! defined (F_CHSIZE) */
#ifdef F_FREESP
#define HAVE_F_FREESP 1
#endif /* defined (F_FREESP) */
#endif /* ! defined (F_CHSIZE) */
#endif /* ! HAVE_FTRUNCATE && ! HAVE_LTRUNC */

openfile_t
esysdep_truncate (e, zname)
     openfile_t e;
     const char *zname;
{
  int o;

#if HAVE_FTRUNCATE || HAVE_LTRUNC || HAVE_F_CHSIZE || HAVE_F_FREESP
  int itrunc;

  if (! ffilerewind (e))
    {
      ulog (LOG_ERROR, "rewind: %s", strerror (errno));
      (void) ffileclose (e);
      (void) remove (zname);
      return EFILECLOSED;
    }

#if USE_STDIO
  o = fileno (e);
#else
  o = e;
#endif

#if HAVE_FTRUNCATE
  itrunc = ftruncate (o, 0);
#endif
#if HAVE_LTRUNC
  itrunc = ltrunc (o, (long) 0, SEEK_SET);
#endif
#if HAVE_F_CHSIZE
  itrunc = fcntl (o, F_CHSIZE, (off_t) 0);
#endif
#if HAVE_F_FREESP
  /* This selection is based on an implementation of ftruncate by
     kucharsk@Solbourne.com (William Kucharski).  */
  {
    struct flock fl;

    fl.l_whence = 0;
    fl.l_len = 0;
    fl.l_start = 0;
    fl.l_type = F_WRLCK;

    itrunc = fcntl (o, F_FREESP, &fl);
  }
#endif

  if (itrunc != 0)
    {
#if HAVE_FTRUNCATE
      ulog (LOG_ERROR, "ftruncate: %s", strerror (errno));
#endif
#ifdef HAVE_LTRUNC
      ulog (LOG_ERROR, "ltrunc: %s", strerror (errno));
#endif
#ifdef HAVE_F_CHSIZE
      ulog (LOG_ERROR, "fcntl (F_CHSIZE): %s", strerror (errno));
#endif
#ifdef HAVE_F_FREESP
      ulog (LOG_ERROR, "fcntl (F_FREESP): %s", strerror (errno));
#endif

      (void) ffileclose (e);
      (void) remove (zname);
      return EFILECLOSED;
    }

  return e;
#else /* ! (HAVE_FTRUNCATE || HAVE_LTRUNC || HAVE_F_CHSIZE || HAVE_F_FREESP) */
  (void) ffileclose (e);
  (void) remove (zname);

  o = creat ((char *) zname, IPRIVATE_FILE_MODE);

  if (o == -1)
    {
      ulog (LOG_ERROR, "creat (%s): %s", zname, strerror (errno));
      return EFILECLOSED;
    }

  if (fcntl (o, F_SETFD, fcntl (o, F_GETFD, 0) | FD_CLOEXEC) < 0)
    {
      ulog (LOG_ERROR, "fcntl (FD_CLOEXEC): %s", strerror (errno));
      (void) close (o);
      return EFILECLOSED;
    }

#if USE_STDIO
  e = fdopen (o, (char *) BINWRITE);

  if (e == NULL)
    {
      ulog (LOG_ERROR, "fdopen (%s): %s", zname, strerror (errno));
      (void) close (o);
      (void) remove (zname);
      return NULL;
    }
#else /* ! USE_STDIO */
  e = o;
#endif /* ! USE_STDIO */

  return e;
#endif /* ! (HAVE_FTRUNCATE || HAVE_LTRUNC || HAVE_F_CHSIZE || HAVE_F_FREESP) */
}
