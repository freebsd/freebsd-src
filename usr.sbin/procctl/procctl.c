/*
 * procctl -- clear the event mask, and continue, any specified processes.
 * This is largely an example of how to use the procfs interface; however,
 * for now, it is also sometimes necessary, as a stopped process will not
 * otherwise continue.  (This will be fixed in a later version of the
 * procfs code, almost certainly; however, this program will still be useful
 * for some annoying circumstances.)
 */
/*
 * $Id: procctl.c,v 1.1 1997/12/06 04:19:09 sef Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/pioctl.h>

main(int ac, char **av) {
  int fd;
  int i;
  unsigned int mask;
  char **command;
  struct procfs_status pfs;

  for (i = 1; i < ac; i++) {
    char buf[32];

    sprintf(buf, "/proc/%s/mem", av[i]);
    fd = open(buf, O_RDWR);
    if (fd == -1) {
      if (errno == ENOENT)
	continue;
      fprintf(stderr, "%s:  cannot open pid %s:  %s\n",
	      av[0], av[i], strerror(errno));
      continue;
    }
    if (ioctl(fd, PIOCBIC, ~0) == -1) {
      fprintf(stderr, "%s:  cannot clear process %s's event mask: %s\n",
	      av[0], av[i], strerror(errno));
    }
    if (ioctl(fd, PIOCCONT, 0) == -1 && errno != EINVAL) {
      fprintf(stderr, "%s:  cannot continue process %s:  %s\n",
	      av[0], av[i], strerror(errno));
    }
    close(fd);
  }
  return 0;
}
