/* unixproc.c -

   Unix implementation of run_process().

     Written by James Clark (jjc@jclark.com).
*/

#include "config.h"

#ifdef SUPPORT_SUBDOC

#ifdef POSIX

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#endif /* POSIX */

#include "std.h"
#include "entity.h"
#include "appl.h"

#ifndef POSIX

#define WIFSTOPPED(s) (((s) & 0377) == 0177)
#define WIFSIGNALED(s) (((s) & 0377) != 0 && ((s) & 0377 != 0177))
#define WIFEXITED(s) (((s) & 0377) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0377)
#define WTERMSIG(s) ((s) & 0177)
#define WSTOPSIG(s) (((s) >> 8) & 0377)
#define _SC_OPEN_MAX 0
#define sysconf(name) (20)
typedef int pid_t;

#endif /* not POSIX */

#ifndef HAVE_VFORK
#define vfork() fork()
#endif /* not HAVE_VFORK */

#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif /* HAVE_VFORK_H */

int run_process(argv)
char **argv;
{
     pid_t pid;
     int status;
     int ret;

     /* Can't trust Unix implementations to support fflush(NULL). */
     fflush(stderr);
     fflush(stdout);

     pid = vfork();
     if (pid == 0) {
	  /* child */
	  int i;
	  int open_max = (int)sysconf(_SC_OPEN_MAX);

	  for (i = 3; i < open_max; i++)
	       (void)close(i);
	  execvp(argv[0], argv);
	  appl_error(E_EXEC, argv[0], strerror(errno));
	  fflush(stderr);
	  _exit(127);
     }
     if (pid < 0) {
	  appl_error(E_FORK, strerror(errno));
	  return -1;
     }
     /* parent */
     while ((ret = wait(&status)) != pid)
	  if (ret < 0) {
	       appl_error(E_WAIT, strerror(errno));
	       return -1;
	  }
     if (WIFSIGNALED(status)) {
	  appl_error(E_SIGNAL, argv[0], WTERMSIG(status));
	  return -1;
     }
     /* Must have exited normally. */
     return WEXITSTATUS(status);
}

#endif /* SUPPORT_SUBDOC */

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
