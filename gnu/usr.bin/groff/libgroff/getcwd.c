/* Partial emulation of getcwd in terms of getwd. */

#include <sys/param.h>
#include <string.h>
#include <errno.h>
#ifndef errno
extern int errno;
#endif

char *getwd();

char *getcwd(buf, size)
     char *buf;
     int size;			/* POSIX says this should be size_t */
{
  if (size <= 0) {
    errno = EINVAL;
    return 0;
  }
  else {
    char mybuf[MAXPATHLEN];
    int saved_errno = errno;

    errno = 0;
    if (!getwd(mybuf)) {
      if (errno == 0)
	;       /* what to do? */
      return 0;
    }
    errno = saved_errno;
    if (strlen(mybuf) + 1 > size) {
      errno = ERANGE;
      return 0;
    }
    strcpy(buf, mybuf);
    return buf;
  }
}
