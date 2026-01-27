#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __FreeBSD__
#include <sys/procdesc.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
int pdfork(int *fd, int flags) {
  return syscall(__NR_pdfork, fd, flags);
}
#endif

int main() {
  int procfd;
  int rc =  pdfork(&procfd, 0);
  if (rc < 0) {
    fprintf(stderr, "pdfork() failed rc=%d errno=%d %s\n", rc, errno, strerror(errno));
    exit(1);
  }
  if (rc == 0) { // Child process
    sleep(1);
    exit(123);
  }
  fprintf(stderr, "pdfork()ed child pid=%ld procfd=%d\n", (long)rc, procfd);
  sleep(2);  // Allow child to complete
  pid_t child = waitpid(-1, &rc, WNOHANG);
  if (child == 0) {
    fprintf(stderr, "waitpid(): no completed child found\n");
  } else if (child < 0) {
    fprintf(stderr, "waitpid(): failed errno=%d %s\n", errno, strerror(errno));
  } else {
    fprintf(stderr, "waitpid(): found completed child %ld\n", (long)child);
  }
  return 0;
}
