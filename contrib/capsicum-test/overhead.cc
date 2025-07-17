#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

#ifdef HAVE_SYSCALL
double RepeatSyscall(int count, int nr, long arg1, long arg2, long arg3) {
  const clock_t t0 = clock(); // or gettimeofday or whatever
  for (int ii = 0; ii < count; ii++) {
    syscall(nr, arg1, arg2, arg3);
  }
  const clock_t t1 = clock();
  return (t1 - t0) / (double)CLOCKS_PER_SEC;
}

typedef int (*EntryFn)(void);

double CompareSyscall(EntryFn entry_fn, int count, int nr,
                      long arg1, long arg2, long arg3) {
  double bare = RepeatSyscall(count, nr, arg1, arg2, arg3);
  EXPECT_OK(entry_fn());
  double capmode = RepeatSyscall(count, nr, arg1, arg2, arg3);
  if (verbose) fprintf(stderr, "%d iterations bare=%fs capmode=%fs ratio=%.2f%%\n",
                       count, bare, capmode, 100.0*capmode/bare);
  if (bare==0.0) {
    if (capmode==0.0) return 1.0;
    return 999.0;
  }
  return capmode/bare;
}

FORK_TEST(Overhead, GetTid) {
  EXPECT_GT(10, CompareSyscall(&cap_enter, 10000, __NR_gettid, 0, 0, 0));
}
FORK_TEST(Overhead, Seek) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_GT(50, CompareSyscall(&cap_enter, 10000, __NR_lseek, fd, 0, SEEK_SET));
  close(fd);
}
#endif
