/* Small standalone test program to check the existence of Capsicum syscalls */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#include "capsicum.h"

#ifdef __linux__
// glibc on Linux caches getpid() return value.
int getpid_(void) { return syscall(__NR_getpid); }
#else
#define getpid_ getpid
#endif

static int seen_sigchld = 0;
static void handle_signal(int x) {
  fprintf(stderr, "[%d] received SIGCHLD\n", getpid_());
  seen_sigchld = 1;
}

int main(int argc, char *argv[]) {
  signal(SIGCHLD, handle_signal);
  int lifetime = 4; /* seconds */
  if (1 < argc) {
    lifetime = atoi(argv[1]);
  }

  /* cap_rights_limit() available? */
  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  int cap_fd = dup(STDOUT_FILENO);
  int rc = cap_rights_limit(cap_fd, &r_rws);
  fprintf(stderr, "[%d] cap_fd=%d\n", getpid_(), cap_fd);
  if (rc < 0) fprintf(stderr, "*** cap_rights_limit() failed: errno=%d %s\n", errno, strerror(errno));

  /* cap_rights_get() available? */
  cap_rights_t rights;
  cap_rights_init(&rights, 0);
  rc = cap_rights_get(cap_fd, &rights);
  char buffer[256];
  cap_rights_describe(&rights, buffer);
  fprintf(stderr, "[%d] cap_rights_get(cap_fd=%d) rc=%d rights=%s\n", getpid_(), cap_fd, rc, buffer);
  if (rc < 0) fprintf(stderr, "*** cap_rights_get() failed: errno=%d %s\n", errno, strerror(errno));

  /* fstat() policed? */
  struct stat buf;
  rc = fstat(cap_fd, &buf);
  fprintf(stderr, "[%d] fstat(cap_fd=%d) rc=%d errno=%d\n", getpid_(), cap_fd, rc, errno);
  if (rc != -1) fprintf(stderr, "*** fstat() unexpectedly succeeded\n");

  /* pdfork() available? */
  int pd = -1;
  rc = pdfork(&pd, 0);
  if (rc < 0) fprintf(stderr, "*** pdfork() failed: errno=%d %s\n", errno, strerror(errno));

  if (rc == 0) { /* child */
    int count = 0;
    while (count < 20) {
      fprintf(stderr, "  [%d] child alive, parent is ppid=%d\n", getpid_(), getppid());
      sleep(1);
    }
    fprintf(stderr, "  [%d] child exit(0)\n", getpid_());
    exit(0);
  }
  fprintf(stderr, "[%d] pdfork() rc=%d pd=%d\n", getpid_(), rc, pd);

  /* pdgetpid() available? */
  pid_t actual_pid = rc;
  pid_t got_pid = -1;
  rc = pdgetpid(pd, &got_pid);
  if (rc < 0) fprintf(stderr, "*** pdgetpid(pd=%d) failed: errno=%d %s\n", pd, errno, strerror(errno));
  fprintf(stderr, "[%d] pdgetpid(pd=%d)=%d, pdfork returned %d\n", getpid_(), pd, got_pid, actual_pid);

  sleep(lifetime);

  /* pdkill() available? */
  rc = pdkill(pd, SIGKILL);
  fprintf(stderr, "[%d] pdkill(pd=%d, SIGKILL) -> rc=%d\n", getpid_(), pd, rc);
  if (rc < 0) fprintf(stderr, "*** pdkill() failed: errno=%d %s\n", errno, strerror(errno));
  usleep(50000);  /* Allow time for death and signals */

  /* Death of a pdforked child should be invisible */
  if (seen_sigchld) fprintf(stderr, "*** SIGCHLD emitted\n");
  int status;
  rc = wait4(-1, &status, WNOHANG, NULL);
  if (rc > 0) fprintf(stderr, "*** wait4(-1, ...) unexpectedly found child %d\n", rc);

  fprintf(stderr, "[%d] forking off a child process to check cap_enter()\n", getpid_());
  pid_t child = fork();
  if (child == 0) { /* child */
    /* cap_getmode() / cap_enter() available? */
    unsigned int cap_mode = -1;
    rc = cap_getmode(&cap_mode);
    fprintf(stderr, "  [%d] cap_getmode() -> rc=%d, cap_mode=%d\n", getpid_(), rc, cap_mode);
    if (rc < 0) fprintf(stderr, "*** cap_getmode() failed: errno=%d %s\n", errno, strerror(errno));

    rc = cap_enter();
    fprintf(stderr, "  [%d] cap_enter() -> rc=%d\n", getpid_(), rc);
    if (rc < 0) fprintf(stderr, "*** cap_enter() failed: errno=%d %s\n", errno, strerror(errno));

    rc = cap_getmode(&cap_mode);
    fprintf(stderr, "  [%d] cap_getmode() -> rc=%d, cap_mode=%d\n", getpid_(), rc, cap_mode);
    if (rc < 0) fprintf(stderr, "*** cap_getmode() failed: errno=%d %s\n", errno, strerror(errno));

    /* open disallowed? */
    rc = open("/etc/passwd", O_RDONLY);
    fprintf(stderr, "  [%d] open('/etc/passwd') -> rc=%d, errno=%d\n", getpid_(), rc, errno);
    if (rc != -1) fprintf(stderr, "*** open() unexpectedly succeeded\n");
#ifdef ECAPMODE
    if (errno != ECAPMODE) fprintf(stderr, "*** open() failed with errno %d not ECAPMODE\n", errno);
#endif
    exit(0);
  }
  rc = wait4(child, &status, 0, NULL);
  fprintf(stderr, "[%d] child %d exited with status %x\n", getpid_(), child, status);

  /* fexecve() available? */
  char* argv_pass[] = {(char*)"/bin/ls", "-l", "smoketest", NULL};
  char* null_envp[] = {NULL};
  int ls_bin = open("/bin/ls", O_RDONLY);
  fprintf(stderr, "[%d] about to fexecve('/bin/ls', '-l', 'smoketest')\n", getpid_());
  rc = fexecve(ls_bin, argv_pass, null_envp);
  /* should never reach here */
  fprintf(stderr, "*** fexecve(fd=%d) failed: rc=%d errno=%d %s\n", ls_bin, rc, errno, strerror(errno));

  return 0;
}
