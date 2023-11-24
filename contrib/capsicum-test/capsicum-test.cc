#include "capsicum-test.h"

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libprocstat.h>
#endif

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <map>
#include <vector>
#include <string>

bool verbose = false;
bool tmpdir_on_tmpfs = false;
bool force_mt = false;
bool force_nofork = false;
uid_t other_uid = 0;

namespace {
std::map<std::string, std::string> tmp_paths;
}

const char *TmpFile(const char *p) {
  std::string pathname(p);
  if (tmp_paths.find(pathname) == tmp_paths.end()) {
    std::string fullname = tmpdir + "/" + pathname;
    tmp_paths[pathname] = fullname;
  }
  return tmp_paths[pathname].c_str();
}

char ProcessState(int pid) {
#ifdef __linux__
  // Open the process status file.
  char s[1024];
  snprintf(s, sizeof(s), "/proc/%d/status", pid);
  FILE *f = fopen(s, "r");
  if (f == NULL) return '\0';

  // Read the file line by line looking for the state line.
  const char *prompt = "State:\t";
  while (!feof(f)) {
    fgets(s, sizeof(s), f);
    if (!strncmp(s, prompt, strlen(prompt))) {
      fclose(f);
      return s[strlen(prompt)];
    }
  }
  fclose(f);
  return '?';
#endif
#ifdef __FreeBSD__
  // First check if the process exists/we have permission to see it. This
  // Avoids warning messages being printed to stderr by libprocstat.
  size_t len = 0;
  int name[4];
  name[0] = CTL_KERN;
  name[1] = KERN_PROC;
  name[2] = KERN_PROC_PID;
  name[3] = pid;
  if (sysctl(name, nitems(name), NULL, &len, NULL, 0) < 0 && errno == ESRCH) {
    if (verbose) fprintf(stderr, "Process %d does not exist\n", pid);
    return '\0'; // No such process.
  }
  unsigned int count = 0;
  struct procstat *prstat = procstat_open_sysctl();
  EXPECT_NE(nullptr, prstat) << "procstat_open_sysctl failed.";
  errno = 0;
  struct kinfo_proc *p = procstat_getprocs(prstat, KERN_PROC_PID, pid, &count);
  if (p == NULL || count == 0) {
    if (verbose) {
      fprintf(stderr, "procstat_getprocs failed with %p/%d: %s\n", (void *)p,
              count, strerror(errno));
    }
    procstat_close(prstat);
    return '\0';
  }
  char result = '\0';
  // See state() in bin/ps/print.c
  switch (p->ki_stat) {
  case SSTOP:
    result = 'T';
    break;
  case SSLEEP:
    if (p->ki_tdflags & TDF_SINTR) /* interruptable (long) */
      result = 'S';
    else
      result = 'D';
    break;
  case SRUN:
  case SIDL:
    result = 'R';
    break;
  case SWAIT:
  case SLOCK:
    // We treat SWAIT/SLOCK as 'S' here (instead of 'W'/'L').
    result = 'S';
    break;
  case SZOMB:
    result = 'Z';
    break;
  default:
    result = '?';
    break;
  }
  procstat_freeprocs(prstat, p);
  procstat_close(prstat);
  if (verbose) fprintf(stderr, "Process %d in state '%c'\n", pid, result);
  return result;
#endif
}
