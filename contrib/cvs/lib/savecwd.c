#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif

#ifdef HAVE_DIRECT_H
# include <direct.h>
#endif

#ifdef HAVE_IO_H
# include <io.h>
#endif

#include <errno.h>
# ifndef errno
extern int errno;
#endif

#include "savecwd.h"
#include "error.h"

char *xgetwd __PROTO((void));

/* Record the location of the current working directory in CWD so that
   the program may change to other directories and later use restore_cwd
   to return to the recorded location.  This function may allocate
   space using malloc (via xgetwd) or leave a file descriptor open;
   use free_cwd to perform the necessary free or close.  Upon failure,
   no memory is allocated, any locally opened file descriptors are
   closed;  return non-zero -- in that case, free_cwd need not be
   called, but doing so is ok.  Otherwise, return zero.  */

int
save_cwd (cwd)
     struct saved_cwd *cwd;
{
  static int have_working_fchdir = 1;

  cwd->desc = -1;
  cwd->name = NULL;

  if (have_working_fchdir)
    {
#ifdef HAVE_FCHDIR
      cwd->desc = open (".", O_RDONLY);
      if (cwd->desc < 0)
	{
	  error (0, errno, "cannot open current directory");
	  return 1;
	}

# if __sun__ || sun
      /* On SunOS 4, fchdir returns EINVAL if accounting is enabled,
	 so we have to fall back to chdir.  */
      if (fchdir (cwd->desc))
	{
	  if (errno == EINVAL)
	    {
	      close (cwd->desc);
	      cwd->desc = -1;
	      have_working_fchdir = 0;
	    }
	  else
	    {
	      error (0, errno, "current directory");
	      close (cwd->desc);
	      cwd->desc = -1;
	      return 1;
	    }
	}
# endif /* __sun__ || sun */
#else
#define fchdir(x) (abort (), 0)
      have_working_fchdir = 0;
#endif
    }

  if (!have_working_fchdir)
    {
      cwd->name = xgetwd ();
      if (cwd->name == NULL)
	{
	  error (0, errno, "cannot get current directory");
	  return 1;
	}
    }
  return 0;
}

/* Change to recorded location, CWD, in directory hierarchy.
   If "saved working directory", NULL))
   */

int
restore_cwd (cwd, dest)
     const struct saved_cwd *cwd;
     const char *dest;
{
  int fail = 0;
  if (cwd->desc >= 0)
    {
      if (fchdir (cwd->desc))
	{
	  error (0, errno, "cannot return to %s",
		 (dest ? dest : "saved working directory"));
	  fail = 1;
	}
    }
  else if (chdir (cwd->name) < 0)
    {
      error (0, errno, "%s", cwd->name);
      fail = 1;
    }
  return fail;
}

void
free_cwd (cwd)
     struct saved_cwd *cwd;
{
  if (cwd->desc >= 0)
    close (cwd->desc);
  if (cwd->name)
    free (cwd->name);
}

