#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "system.h"
#include "wait.h"

#include <stdio.h>

struct unreaped {
  pid_t pid;
  int status;
};
static struct unreaped *unreaped;
static int n;

static struct unreaped *ualloc (oldptr, n)
     struct unreaped *oldptr;
     int n;
{
  n *= sizeof (struct unreaped);
  if (n == 0)
    n = 1;
  if (oldptr)
    oldptr = (struct unreaped *) realloc ((char *) oldptr, n);
  else
    oldptr = (struct unreaped *) malloc (n);
  if (oldptr == 0)
    {
      fprintf (stderr, "cannot allocate %d bytes\n", n);
      exit (1);
    }
  return oldptr;
}

pid_t waitpid (pid, status, options)
     pid_t pid;
     int *status;
     int options;
{
  int i;

  /* initialize */
  if (unreaped == 0)
    {
      unreaped = ualloc (unreaped, 1);
      unreaped[0].pid = 0;
      n = 1;
    }

  for (i = 0; unreaped[i].pid; i++)
    if (unreaped[i].pid == pid)
      {
	*status = unreaped[i].status;
	while (unreaped[i].pid)
	  {
	    unreaped[i] = unreaped[i+1];
	    i++;
	  }
	n--;
	return pid;
      }

  while (1)
    {
#ifdef HAVE_WAIT3
      pid_t p = wait3 (status, options, (struct rusage *) 0);
#else
      pid_t p = wait (status);
#endif

      if (p == 0 || p == -1 || p == pid)
	return p;

      n++;
      unreaped = ualloc (unreaped, n);
      unreaped[n-1].pid = p;
      unreaped[n-1].status = *status;
    }
}
