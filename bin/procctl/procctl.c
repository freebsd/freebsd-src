/*
 * procctl -- clear the event mask, and continue, any specified processes.
 * This is largely an example of how to use the procfs interface; however,
 * for now, it is also sometimes necessary, as a stopped process will not
 * otherwise continue.  (This will be fixed in a later version of the
 * procfs code, almost certainly; however, this program will still be useful
 * for some annoying circumstances.)
 */
/*
 * $Id$
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
    mask = ~0;
    if (ioctl(fd, PIOCBIC, &mask) == -1) {
      fprintf(stderr, "%s:  cannot clear process %s's event mask: %s\n",
	      av[0], av[i], strerror(errno));
    }
    mask = 0;
    if (ioctl(fd, PIOCCONT, &mask) == -1 && errno != EINVAL) {
      fprintf(stderr, "%s:  cannot continue process %s:  %s\n",
	      av[0], av[i], strerror(errno));
    }
    close(fd);
  }
  return 0;
}
