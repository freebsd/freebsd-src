/* ftruncate emulations that work on some System V's.
   This file is in the public domain.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <fcntl.h>

#ifdef F_CHSIZE

int
ftruncate (int fd, off_t length)
{
  return fcntl (fd, F_CHSIZE, length);
}

#else /* not F_CHSIZE */
# ifdef F_FREESP

/* By William Kucharski <kucharsk@netcom.com>.  */

#  include <sys/stat.h>
#  include <errno.h>
#  if HAVE_UNISTD_H
#   include <unistd.h>
#  endif

int
ftruncate (int fd, off_t length)
{
  struct flock fl;
  struct stat filebuf;

  if (fstat (fd, &filebuf) < 0)
    return -1;

  if (filebuf.st_size < length)
    {
      /* Extend file length. */
      if (lseek (fd, (length - 1), SEEK_SET) < 0)
	return -1;

      /* Write a "0" byte. */
      if (write (fd, "", 1) != 1)
	return -1;
    }
  else
    {

      /* Truncate length. */

      fl.l_whence = 0;
      fl.l_len = 0;
      fl.l_start = length;
      fl.l_type = F_WRLCK;	/* write lock on file space */

      /* This relies on the *undocumented* F_FREESP argument to fcntl,
	 which truncates the file so that it ends at the position
	 indicated by fl.l_start.  Will minor miracles never cease?  */

      if (fcntl (fd, F_FREESP, &fl) < 0)
	return -1;
    }

  return 0;
}

# else /* not F_CHSIZE nor F_FREESP */
#  if HAVE_CHSIZE

int
ftruncate (int fd, off_t length)
{
  return chsize (fd, length);
}

#  else /* not F_CHSIZE nor F_FREESP nor HAVE_CHSIZE */

#   include <errno.h>
#   ifndef errno
extern int errno;
#   endif

int
ftruncate (int fd, off_t length)
{
  errno = EIO;
  return -1;
}

#  endif /* not HAVE_CHSIZE */
# endif /* not F_FREESP */
#endif /* not F_CHSIZE */
