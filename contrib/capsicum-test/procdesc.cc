// Tests for the process descriptor API for Linux.
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <iomanip>
#include <map>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

#ifndef __WALL
// Linux requires __WALL in order for waitpid(specific_pid,...) to
// see and reap any specific pid.  Define this to nothing for platforms
// (FreeBSD) where it doesn't exist, to reduce macroing.
#define __WALL 0
#endif

// TODO(drysdale): it would be nice to use proper synchronization between
// processes, rather than synchronization-via-sleep; faster too.


//------------------------------------------------
// Utilities for the tests.

static pid_t pdwait4_(int pd, int *status, int options, struct rusage *ru) {
#ifdef HAVE_PDWAIT4
  return pdwait4(pd, status, options, ru);
#else
  // Simulate pdwait4() with wait4(pdgetpid()); this won't work in capability mode.
  pid_t pid = -1;
  int rc = pdgetpid(pd, &pid);
  if (rc < 0) {
    return rc;
  }
  options |= __WALL;
  return wait4(pid, status, options, ru);
#endif
}

static void print_rusage(FILE *f, struct rusage *ru) {
  fprintf(f, "  User CPU time=%ld.%06ld\n", (long)ru->ru_utime.tv_sec, (long)ru->ru_utime.tv_usec);
  fprintf(f, "  System CPU time=%ld.%06ld\n", (long)ru->ru_stime.tv_sec, (long)ru->ru_stime.tv_usec);
  fprintf(f, "  Max RSS=%ld\n", ru->ru_maxrss);
}

static void print_stat(FILE *f, const struct stat *stat) {
  fprintf(f,
          "{ .st_dev=%ld, st_ino=%ld, st_mode=%04o, st_nlink=%ld, st_uid=%d, st_gid=%d,\n"
          "  .st_rdev=%ld, .st_size=%ld, st_blksize=%ld, .st_block=%ld,\n  "
#ifdef HAVE_STAT_BIRTHTIME
          ".st_birthtime=%ld, "
#endif
          ".st_atime=%ld, .st_mtime=%ld, .st_ctime=%ld}\n",
          (long)stat->st_dev, (long)stat->st_ino, stat->st_mode,
          (long)stat->st_nlink, stat->st_uid, stat->st_gid,
          (long)stat->st_rdev, (long)stat->st_size, (long)stat->st_blksize,
          (long)stat->st_blocks,
#ifdef HAVE_STAT_BIRTHTIME
          (long)stat->st_birthtime,
#endif
          (long)stat->st_atime, (long)stat->st_mtime, (long)stat->st_ctime);
}

static std::map<int,bool> had_signal;
static void handle_signal(int x) {
  had_signal[x] = true;
}

// Check that the given child process terminates as expected.
void CheckChildFinished(pid_t pid, bool signaled=false) {
  // Wait for the child to finish.
  int rc;
  int status = 0;
  do {
    rc = waitpid(pid, &status, __WALL);
    if (rc < 0) {
      fprintf(stderr, "Warning: waitpid error %s (%d)\n", strerror(errno), errno);
      ADD_FAILURE() << "Failed to wait for child";
      break;
    } else if (rc == pid) {
      break;
    }
  } while (true);
  EXPECT_EQ(pid, rc);
  if (rc == pid) {
    if (signaled) {
      EXPECT_TRUE(WIFSIGNALED(status));
    } else {
      EXPECT_TRUE(WIFEXITED(status)) << std::hex << status;
      EXPECT_EQ(0, WEXITSTATUS(status));
    }
  }
}

//------------------------------------------------
// Basic tests of process descriptor functionality

TEST(Pdfork, Simple) {
  int pd = -1;
  pid_t parent = getpid_();
  int pid = pdfork(&pd, 0);
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: check pid values.
    EXPECT_EQ(-1, pd);
    EXPECT_NE(parent, getpid_());
    EXPECT_EQ(parent, getppid());
    sleep(1);
    exit(0);
  }
  usleep(100);  // ensure the child has a chance to run
  EXPECT_NE(-1, pd);
  EXPECT_PID_ALIVE(pid);
  int pid_got;
  EXPECT_OK(pdgetpid(pd, &pid_got));
  EXPECT_EQ(pid, pid_got);

  // Wait long enough for the child to exit().
  sleep(2);
  EXPECT_PID_ZOMBIE(pid);

  // Wait for the the child.
  int status;
  struct rusage ru;
  memset(&ru, 0, sizeof(ru));
  int waitrc = pdwait4_(pd, &status, 0, &ru);
  EXPECT_EQ(pid, waitrc);
  if (verbose) {
    fprintf(stderr, "For pd %d pid %d:\n", pd, pid);
    print_rusage(stderr, &ru);
  }
  EXPECT_PID_GONE(pid);

  // Can only pdwait4(pd) once (as initial call reaps zombie).
  memset(&ru, 0, sizeof(ru));
  EXPECT_EQ(-1, pdwait4_(pd, &status, 0, &ru));
  EXPECT_EQ(ECHILD, errno);

  EXPECT_OK(close(pd));
}

TEST(Pdfork, InvalidFlag) {
  int pd = -1;
  int pid = pdfork(&pd, PD_DAEMON<<5);
  if (pid == 0) {
    exit(1);
  }
  EXPECT_EQ(-1, pid);
  EXPECT_EQ(EINVAL, errno);
  if (pid > 0) waitpid(pid, NULL, __WALL);
}

TEST(Pdfork, TimeCheck) {
  time_t now = time(NULL);  // seconds since epoch
  EXPECT_NE(-1, now);
  if (verbose) fprintf(stderr, "Calling pdfork around %ld\n", (long)(long)now);

  int pd = -1;
  pid_t pid = pdfork(&pd, 0);
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: check we didn't get a valid process descriptor then exit.
    EXPECT_EQ(-1, pdgetpid(pd, &pid));
    EXPECT_EQ(EBADF, errno);
    exit(HasFailure());
  }

#ifdef HAVE_PROCDESC_FSTAT
  // Parent process. Ensure that [acm]times have been set correctly.
  struct stat stat;
  memset(&stat, 0, sizeof(stat));
  EXPECT_OK(fstat(pd, &stat));
  if (verbose) print_stat(stderr, &stat);

#ifdef HAVE_STAT_BIRTHTIME
  EXPECT_GE(now, stat.st_birthtime);
  EXPECT_EQ(stat.st_birthtime, stat.st_atime);
#endif
  EXPECT_LT((now - stat.st_atime), 2);
  EXPECT_EQ(stat.st_atime, stat.st_ctime);
  EXPECT_EQ(stat.st_ctime, stat.st_mtime);
#endif

  // Wait for the child to finish.
  pid_t pd_pid = -1;
  EXPECT_OK(pdgetpid(pd, &pd_pid));
  EXPECT_EQ(pid, pd_pid);
  CheckChildFinished(pid);
}

TEST(Pdfork, UseDescriptor) {
  int pd = -1;
  pid_t pid = pdfork(&pd, 0);
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: immediately exit
    exit(0);
  }
  CheckChildFinished(pid);
}

TEST(Pdfork, NonProcessDescriptor) {
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);
  // pd*() operations should fail on a non-process descriptor.
  EXPECT_EQ(-1, pdkill(fd, SIGUSR1));
  int status;
  EXPECT_EQ(-1, pdwait4_(fd, &status, 0, NULL));
  pid_t pid;
  EXPECT_EQ(-1, pdgetpid(fd, &pid));
  close(fd);
}

static void *SubThreadMain(void *) {
  while (true) {
    if (verbose) fprintf(stderr, "      subthread: \"I aten't dead\"\n");
    usleep(100000);
  }
  return NULL;
}

static void *ThreadMain(void *) {
  int pd;
  pid_t child = pdfork(&pd, 0);
  if (child == 0) {
    // Child: start a subthread then loop
    pthread_t child_subthread;
    EXPECT_OK(pthread_create(&child_subthread, NULL, SubThreadMain, NULL));
    while (true) {
      if (verbose) fprintf(stderr, "    pdforked process %d: \"I aten't dead\"\n", getpid());
      usleep(100000);
    }
    exit(0);
  }
  if (verbose) fprintf(stderr, "  thread generated pd %d\n", pd);
  sleep(2);

  // Pass the process descriptor back to the main thread.
  return reinterpret_cast<void *>(pd);
}

TEST(Pdfork, FromThread) {
  // Fire off a new thread to do all of the creation work.
  pthread_t child_thread;
  EXPECT_OK(pthread_create(&child_thread, NULL, ThreadMain, NULL));
  void *data;
  EXPECT_OK(pthread_join(child_thread, &data));
  int pd = reinterpret_cast<intptr_t>(data);
  if (verbose) fprintf(stderr, "retrieved pd %d from terminated thread\n", pd);

  // Kill and reap.
  pid_t pid;
  EXPECT_OK(pdgetpid(pd, &pid));
  EXPECT_OK(pdkill(pd, SIGKILL));
  int status;
  EXPECT_EQ(pid, pdwait4_(pd, &status, 0, NULL));
  EXPECT_TRUE(WIFSIGNALED(status));
}

//------------------------------------------------
// More complicated tests.


// Test fixture that pdfork()s off a child process, which terminates
// when it receives anything on a pipe.
class PipePdforkBase : public ::testing::Test {
 public:
  PipePdforkBase(int pdfork_flags) : pd_(-1), pid_(-1) {
    had_signal.clear();
    int pipes[2];
    EXPECT_OK(pipe(pipes));
    pipe_ = pipes[1];
    int parent = getpid_();
    if (verbose) fprintf(stderr, "[%d] about to pdfork()\n", getpid_());
    int rc = pdfork(&pd_, pdfork_flags);
    EXPECT_OK(rc);
    if (rc == 0) {
      // Child process: blocking-read an int from the pipe then exit with that value.
      EXPECT_NE(parent, getpid_());
      EXPECT_EQ(parent, getppid());
      if (verbose) fprintf(stderr, "  [%d] child of %d waiting for value on pipe\n", getpid_(), getppid());
      read(pipes[0], &rc, sizeof(rc));
      if (verbose) fprintf(stderr, "  [%d] got value %d on pipe, exiting\n", getpid_(), rc);
      exit(rc);
    }
    pid_ = rc;
    usleep(100);  // ensure the child has a chance to run
  }
  ~PipePdforkBase() {
    // Terminate by any means necessary.
    if (pd_ > 0) {
      pdkill(pd_, SIGKILL);
      close(pd_);
    }
    if (pid_ > 0) {
      kill(pid_, SIGKILL);
      waitpid(pid_, NULL, __WALL|WNOHANG);
    }
    // Check signal expectations.
    EXPECT_FALSE(had_signal[SIGCHLD]);
  }
  int TerminateChild() {
    // Tell the child to exit.
    int zero = 0;
    if (verbose) fprintf(stderr, "[%d] write 0 to pipe\n", getpid_());
    return write(pipe_, &zero, sizeof(zero));
  }
 protected:
  int pd_;
  int pipe_;
  pid_t pid_;
};

class PipePdfork : public PipePdforkBase {
 public:
  PipePdfork() : PipePdforkBase(0) {}
};

class PipePdforkDaemon : public PipePdforkBase {
 public:
  PipePdforkDaemon() : PipePdforkBase(PD_DAEMON) {}
};

// Can we poll a process descriptor?
TEST_F(PipePdfork, Poll) {
  // Poll the process descriptor, nothing happening.
  struct pollfd fdp;
  fdp.fd = pd_;
  fdp.events = POLLIN | POLLERR | POLLHUP;
  fdp.revents = 0;
  EXPECT_EQ(0, poll(&fdp, 1, 0));

  TerminateChild();

  // Poll again, should have activity on the process descriptor.
  EXPECT_EQ(1, poll(&fdp, 1, 2000));
  EXPECT_TRUE(fdp.revents & POLLHUP);

  // Poll a third time, still have POLLHUP.
  fdp.revents = 0;
  EXPECT_EQ(1, poll(&fdp, 1, 0));
  EXPECT_TRUE(fdp.revents & POLLHUP);
}

// Can multiple processes poll on the same descriptor?
TEST_F(PipePdfork, PollMultiple) {
  int child = fork();
  EXPECT_OK(child);
  if (child == 0) {
    // Child: wait to give time for setup, then write to the pipe (which will
    // induce exit of the pdfork()ed process) and exit.
    sleep(1);
    TerminateChild();
    exit(0);
  }
  usleep(100);  // ensure the child has a chance to run

  // Fork again
  int doppel = fork();
  EXPECT_OK(doppel);
  // We now have:
  //   pid A: main process, here
  //   |--pid B: pdfork()ed process, blocked on read()
  //   |--pid C: fork()ed process, in sleep(1) above
  //   +--pid D: doppel process, here

  // Both A and D execute the following code.
  // First, check no activity on the process descriptor yet.
  struct pollfd fdp;
  fdp.fd = pd_;
  fdp.events = POLLIN | POLLERR | POLLHUP;
  fdp.revents = 0;
  EXPECT_EQ(0, poll(&fdp, 1, 0));

  // Now, wait (indefinitely) for activity on the process descriptor.
  // We expect:
  //  - pid C will finish its sleep, write to the pipe and exit
  //  - pid B will unblock from read(), and exit
  //  - this will generate an event on the process descriptor...
  //  - ...in both process A and process D.
  EXPECT_EQ(1, poll(&fdp, 1, 2000));
  EXPECT_TRUE(fdp.revents & POLLHUP);

  if (doppel == 0) {
    // Child: process D exits.
    exit(0);
  } else {
    // Parent: wait on process D.
    int rc = 0;
    waitpid(doppel, &rc, __WALL);
    EXPECT_TRUE(WIFEXITED(rc));
    EXPECT_EQ(0, WEXITSTATUS(rc));
    // Also wait on process B.
    CheckChildFinished(child);
  }
}

// Check that exit status/rusage for a dead pdfork()ed child can be retrieved
// via any process descriptor, multiple times.
TEST_F(PipePdfork, MultipleRetrieveExitStatus) {
  EXPECT_PID_ALIVE(pid_);
  int pd_copy = dup(pd_);
  EXPECT_LT(0, TerminateChild());

  int status;
  struct rusage ru;
  memset(&ru, 0, sizeof(ru));
  int waitrc = pdwait4_(pd_copy, &status, 0, &ru);
  EXPECT_EQ(pid_, waitrc);
  if (verbose) {
    fprintf(stderr, "For pd %d -> pid %d:\n", pd_, pid_);
    print_rusage(stderr, &ru);
  }
  EXPECT_PID_GONE(pid_);

#ifdef NOTYET
  // Child has been reaped, so original process descriptor dangles but
  // still has access to rusage information.
  memset(&ru, 0, sizeof(ru));
  EXPECT_EQ(0, pdwait4_(pd_, &status, 0, &ru));
#endif
  close(pd_copy);
}

TEST_F(PipePdfork, ChildExit) {
  EXPECT_PID_ALIVE(pid_);
  EXPECT_LT(0, TerminateChild());
  EXPECT_PID_DEAD(pid_);

  int status;
  int rc = pdwait4_(pd_, &status, 0, NULL);
  EXPECT_OK(rc);
  EXPECT_EQ(pid_, rc);
  pid_ = 0;
}

#ifdef HAVE_PROC_FDINFO
TEST_F(PipePdfork, FdInfo) {
  char buffer[1024];
  sprintf(buffer, "/proc/%d/fdinfo/%d", getpid_(), pd_);
  int procfd = open(buffer, O_RDONLY);
  EXPECT_OK(procfd);

  EXPECT_OK(read(procfd, buffer, sizeof(buffer)));
  // The fdinfo should include the file pos of the underlying file
  EXPECT_NE((char*)NULL, strstr(buffer, "pos:\t0")) << buffer;
  // ...and the underlying pid
  char pidline[256];
  sprintf(pidline, "pid:\t%d", pid_);
  EXPECT_NE((char*)NULL, strstr(buffer, pidline)) << buffer;
  close(procfd);
}
#endif

// Closing a normal process descriptor terminates the underlying process.
TEST_F(PipePdfork, Close) {
  sighandler_t original = signal(SIGCHLD, handle_signal);
  EXPECT_PID_ALIVE(pid_);
  int status;
  EXPECT_EQ(0, waitpid(pid_, &status, __WALL|WNOHANG));

  EXPECT_OK(close(pd_));
  pd_ = -1;
  EXPECT_FALSE(had_signal[SIGCHLD]);
  EXPECT_PID_DEAD(pid_);

#ifdef __FreeBSD__
  EXPECT_EQ(-1, waitpid(pid_, NULL, __WALL));
  EXPECT_EQ(errno, ECHILD);
#else
  // Having closed the process descriptor means that pdwait4(pd) now doesn't work.
  int rc = pdwait4_(pd_, &status, 0, NULL);
  EXPECT_EQ(-1, rc);
  EXPECT_EQ(EBADF, errno);

  // Closing all process descriptors means the the child can only be reaped via pid.
  EXPECT_EQ(pid_, waitpid(pid_, &status, __WALL|WNOHANG));
#endif
  signal(SIGCHLD, original);
}

TEST_F(PipePdfork, CloseLast) {
  sighandler_t original = signal(SIGCHLD, handle_signal);
  // Child should only die when last process descriptor is closed.
  EXPECT_PID_ALIVE(pid_);
  int pd_other = dup(pd_);

  EXPECT_OK(close(pd_));
  pd_ = -1;

  EXPECT_PID_ALIVE(pid_);
  int status;
  EXPECT_EQ(0, waitpid(pid_, &status, __WALL|WNOHANG));

  // Can no longer pdwait4() the closed process descriptor...
  EXPECT_EQ(-1, pdwait4_(pd_, &status, WNOHANG, NULL));
  EXPECT_EQ(EBADF, errno);
  // ...but can pdwait4() the still-open process descriptor.
  errno = 0;
  EXPECT_EQ(0, pdwait4_(pd_other, &status, WNOHANG, NULL));
  EXPECT_EQ(0, errno);

  EXPECT_OK(close(pd_other));
  EXPECT_PID_DEAD(pid_);

  EXPECT_FALSE(had_signal[SIGCHLD]);
  signal(SIGCHLD, original);
}

FORK_TEST(Pdfork, OtherUser) {
  REQUIRE_ROOT();
  int pd;
  pid_t pid = pdfork(&pd, 0);
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child process: loop forever.
    while (true) usleep(100000);
  }
  usleep(100);

  // Now that the second process has been pdfork()ed, change euid.
  setuid(other_uid);
  if (verbose) fprintf(stderr, "uid=%d euid=%d\n", getuid(), geteuid());

  // Fail to kill child with normal PID operation.
  EXPECT_EQ(-1, kill(pid, SIGKILL));
  EXPECT_EQ(EPERM, errno);
  EXPECT_PID_ALIVE(pid);

  // Succeed with pdkill though.
  EXPECT_OK(pdkill(pd, SIGKILL));
  EXPECT_PID_ZOMBIE(pid);

  int status;
  int rc = pdwait4_(pd, &status, WNOHANG, NULL);
  EXPECT_OK(rc);
  EXPECT_EQ(pid, rc);
  EXPECT_TRUE(WIFSIGNALED(status));
}

TEST_F(PipePdfork, WaitPidThenPd) {
  TerminateChild();
  int status;
  // If we waitpid(pid) first...
  int rc = waitpid(pid_, &status, __WALL);
  EXPECT_OK(rc);
  EXPECT_EQ(pid_, rc);

#ifdef NOTYET
  // ...the zombie is reaped but we can still subsequently pdwait4(pd).
  EXPECT_EQ(0, pdwait4_(pd_, &status, 0, NULL));
#endif
}

TEST_F(PipePdfork, WaitPdThenPid) {
  TerminateChild();
  int status;
  // If we pdwait4(pd) first...
  int rc = pdwait4_(pd_, &status, 0, NULL);
  EXPECT_OK(rc);
  EXPECT_EQ(pid_, rc);

  // ...the zombie is reaped and cannot subsequently waitpid(pid).
  EXPECT_EQ(-1, waitpid(pid_, &status, __WALL));
  EXPECT_EQ(ECHILD, errno);
}

// Setting PD_DAEMON prevents close() from killing the child.
TEST_F(PipePdforkDaemon, Close) {
  EXPECT_OK(close(pd_));
  pd_ = -1;
  EXPECT_PID_ALIVE(pid_);

  // Can still explicitly kill it via the pid.
  if (pid_ > 0) {
    EXPECT_OK(kill(pid_, SIGKILL));
    EXPECT_PID_DEAD(pid_);
  }
}

static void TestPdkill(pid_t pid, int pd) {
  EXPECT_PID_ALIVE(pid);
  // SIGCONT is ignored by default.
  EXPECT_OK(pdkill(pd, SIGCONT));
  EXPECT_PID_ALIVE(pid);

  // SIGINT isn't
  EXPECT_OK(pdkill(pd, SIGINT));
  EXPECT_PID_DEAD(pid);

  // pdkill() on zombie is no-op.
  errno = 0;
  EXPECT_EQ(0, pdkill(pd, SIGINT));
  EXPECT_EQ(0, errno);

  // pdkill() on reaped process gives -ESRCH.
  CheckChildFinished(pid, true);
  EXPECT_EQ(-1, pdkill(pd, SIGINT));
  EXPECT_EQ(ESRCH, errno);
}

TEST_F(PipePdfork, Pdkill) {
  TestPdkill(pid_, pd_);
}

TEST_F(PipePdforkDaemon, Pdkill) {
  TestPdkill(pid_, pd_);
}

TEST(Pdfork, PdkillOtherSignal) {
  int pd = -1;
  int pid = pdfork(&pd, 0);
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: watch for SIGUSR1 forever.
    had_signal.clear();
    signal(SIGUSR1, handle_signal);
    while (!had_signal[SIGUSR1]) {
      usleep(100000);
    }
    exit(123);
  }
  sleep(1);

  // Send an invalid signal.
  EXPECT_EQ(-1, pdkill(pd, 0xFFFF));
  EXPECT_EQ(EINVAL, errno);

  // Send an expected SIGUSR1 to the pdfork()ed child.
  EXPECT_PID_ALIVE(pid);
  pdkill(pd, SIGUSR1);
  EXPECT_PID_DEAD(pid);

  // Child's exit status confirms whether it received the signal.
  int status;
  int rc = waitpid(pid, &status, __WALL);
  EXPECT_OK(rc);
  EXPECT_EQ(pid, rc);
  EXPECT_TRUE(WIFEXITED(status)) << "0x" << std::hex << rc;
  EXPECT_EQ(123, WEXITSTATUS(status));
}

pid_t PdforkParentDeath(int pdfork_flags) {
  // Set up:
  //   pid A: main process, here
  //   +--pid B: fork()ed process, sleep(4)s then exits
  //      +--pid C: pdfork()ed process, looping forever
  int sock_fds[2];
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds));
  if (verbose) fprintf(stderr, "[%d] parent about to fork()...\n", getpid_());
  pid_t child = fork();
  EXPECT_OK(child);
  if (child == 0) {
    int pd;
    if (verbose) fprintf(stderr, "  [%d] child about to pdfork()...\n", getpid_());
    pid_t grandchild = pdfork(&pd, pdfork_flags);
    if (grandchild == 0) {
      while (true) {
        if (verbose) fprintf(stderr, "    [%d] grandchild: \"I aten't dead\"\n", getpid_());
        sleep(1);
      }
    }
    if (verbose) fprintf(stderr, "  [%d] pdfork()ed grandchild %d, sending ID to parent\n", getpid_(), grandchild);
    // send grandchild pid to parent
    write(sock_fds[1], &grandchild, sizeof(grandchild));
    sleep(4);
    if (verbose) fprintf(stderr, "  [%d] child terminating\n", getpid_());
    exit(0);
  }
  if (verbose) fprintf(stderr, "[%d] fork()ed child is %d\n", getpid_(), child);
  pid_t grandchild;
  read(sock_fds[0], &grandchild, sizeof(grandchild));
  if (verbose) fprintf(stderr, "[%d] receive grandchild id %d\n", getpid_(), grandchild);
  EXPECT_PID_ALIVE(child);
  EXPECT_PID_ALIVE(grandchild);
  sleep(6);
  // Child dies, closing its process descriptor for the grandchild.
  EXPECT_PID_DEAD(child);
  CheckChildFinished(child);
  return grandchild;
}

TEST(Pdfork, Bagpuss) {
  // "And of course when Bagpuss goes to sleep, all his friends go to sleep too"
  pid_t grandchild = PdforkParentDeath(0);
  // By default: child death => closed process descriptor => grandchild death.
  EXPECT_PID_DEAD(grandchild);
}

TEST(Pdfork, BagpussDaemon) {
  pid_t grandchild = PdforkParentDeath(PD_DAEMON);
  // With PD_DAEMON: child death => closed process descriptor => no effect on grandchild.
  EXPECT_PID_ALIVE(grandchild);
  if (grandchild > 0) {
    EXPECT_OK(kill(grandchild, SIGKILL));
  }
}

// The exit of a pdfork()ed process should not generate SIGCHLD.
TEST_F(PipePdfork, NoSigchld) {
  had_signal.clear();
  sighandler_t original = signal(SIGCHLD, handle_signal);
  TerminateChild();
  int rc = 0;
  // Can waitpid() for the specific pid of the pdfork()ed child.
  EXPECT_EQ(pid_, waitpid(pid_, &rc, __WALL));
  EXPECT_TRUE(WIFEXITED(rc)) << "0x" << std::hex << rc;
  EXPECT_FALSE(had_signal[SIGCHLD]);
  signal(SIGCHLD, original);
}

// The exit of a pdfork()ed process whose process descriptors have
// all been closed should generate SIGCHLD.  The child process needs
// PD_DAEMON to survive the closure of the process descriptors.
TEST_F(PipePdforkDaemon, NoPDSigchld) {
  had_signal.clear();
  sighandler_t original = signal(SIGCHLD, handle_signal);

  EXPECT_OK(close(pd_));
  TerminateChild();
#ifdef __FreeBSD__
  EXPECT_EQ(-1, waitpid(pid_, NULL, __WALL));
  EXPECT_EQ(errno, ECHILD);
#else
  int rc = 0;
  // Can waitpid() for the specific pid of the pdfork()ed child.
  EXPECT_EQ(pid_, waitpid(pid_, &rc, __WALL));
  EXPECT_TRUE(WIFEXITED(rc)) << "0x" << std::hex << rc;
#endif
  EXPECT_FALSE(had_signal[SIGCHLD]);
  signal(SIGCHLD, original);
}

#ifdef HAVE_PROCDESC_FSTAT
TEST_F(PipePdfork, ModeBits) {
  // Owner rwx bits indicate liveness of child
  struct stat stat;
  memset(&stat, 0, sizeof(stat));
  EXPECT_OK(fstat(pd_, &stat));
  if (verbose) print_stat(stderr, &stat);
  EXPECT_EQ(S_IRWXU, (long)(stat.st_mode & S_IRWXU));

  TerminateChild();
  usleep(100000);

  memset(&stat, 0, sizeof(stat));
  EXPECT_OK(fstat(pd_, &stat));
  if (verbose) print_stat(stderr, &stat);
  EXPECT_EQ(0, (int)(stat.st_mode & S_IRWXU));
}
#endif

TEST_F(PipePdfork, WildcardWait) {
  // TODO(FreeBSD): make wildcard wait ignore pdfork()ed children
  // https://bugs.freebsd.org/201054
  TerminateChild();
  sleep(1);  // Ensure child is truly dead.

  // Wildcard waitpid(-1) should not see the pdfork()ed child because
  // there is still a process descriptor for it.
  int rc;
  EXPECT_EQ(-1, waitpid(-1, &rc, WNOHANG));
  EXPECT_EQ(ECHILD, errno);

  EXPECT_OK(close(pd_));
  pd_ = -1;
}

FORK_TEST(Pdfork, Pdkill) {
  had_signal.clear();
  int pd;
  pid_t pid = pdfork(&pd, 0);
  EXPECT_OK(pid);

  if (pid == 0) {
    // Child: set a SIGINT handler and sleep.
    had_signal.clear();
    signal(SIGINT, handle_signal);
    if (verbose) fprintf(stderr, "[%d] child about to sleep(10)\n", getpid_());
    int left = sleep(10);
    if (verbose) fprintf(stderr, "[%d] child slept, %d sec left, had[SIGINT]=%d\n",
                         getpid_(), left, had_signal[SIGINT]);
    // Expect this sleep to be interrupted by the signal (and so left > 0).
    exit(left == 0);
  }

  // Parent: get child's PID.
  pid_t pd_pid;
  EXPECT_OK(pdgetpid(pd, &pd_pid));
  EXPECT_EQ(pid, pd_pid);

  // Interrupt the child after a second.
  sleep(1);
  EXPECT_OK(pdkill(pd, SIGINT));

  // Make sure the child finished properly (caught signal then exited).
  CheckChildFinished(pid);
}

FORK_TEST(Pdfork, PdkillSignal) {
  int pd;
  pid_t pid = pdfork(&pd, 0);
  EXPECT_OK(pid);

  if (pid == 0) {
    // Child: sleep.  No SIGINT handler.
    if (verbose) fprintf(stderr, "[%d] child about to sleep(10)\n", getpid_());
    int left = sleep(10);
    if (verbose) fprintf(stderr, "[%d] child slept, %d sec left\n", getpid_(), left);
    exit(99);
  }

  // Kill the child (as it doesn't handle SIGINT).
  sleep(1);
  EXPECT_OK(pdkill(pd, SIGINT));

  // Make sure the child finished properly (terminated by signal).
  CheckChildFinished(pid, true);
}

//------------------------------------------------
// Test interactions with other parts of Capsicum:
//  - capability mode
//  - capabilities

FORK_TEST(Pdfork, DaemonUnrestricted) {
  EXPECT_OK(cap_enter());
  int fd;

  // Capability mode leaves pdfork() available, with and without flag.
  int rc;
  rc = pdfork(&fd, PD_DAEMON);
  EXPECT_OK(rc);
  if (rc == 0) {
    // Child: immediately terminate.
    exit(0);
  }

  rc = pdfork(&fd, 0);
  EXPECT_OK(rc);
  if (rc == 0) {
    // Child: immediately terminate.
    exit(0);
  }
}

TEST(Pdfork, MissingRights) {
  pid_t parent = getpid_();
  int pd = -1;
  pid_t pid = pdfork(&pd, 0);
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: loop forever.
    EXPECT_NE(parent, getpid_());
    while (true) sleep(1);
  }
  // Create two capabilities from the process descriptor.
  cap_rights_t r_ro;
  cap_rights_init(&r_ro, CAP_READ, CAP_LOOKUP);
  int cap_incapable = dup(pd);
  EXPECT_OK(cap_incapable);
  EXPECT_OK(cap_rights_limit(cap_incapable, &r_ro));
  cap_rights_t r_pdall;
  cap_rights_init(&r_pdall, CAP_PDGETPID, CAP_PDWAIT, CAP_PDKILL);
  int cap_capable = dup(pd);
  EXPECT_OK(cap_capable);
  EXPECT_OK(cap_rights_limit(cap_capable, &r_pdall));

  pid_t other_pid;
  EXPECT_NOTCAPABLE(pdgetpid(cap_incapable, &other_pid));
  EXPECT_NOTCAPABLE(pdkill(cap_incapable, SIGINT));
  int status;
  EXPECT_NOTCAPABLE(pdwait4_(cap_incapable, &status, 0, NULL));

  EXPECT_OK(pdgetpid(cap_capable, &other_pid));
  EXPECT_EQ(pid, other_pid);
  EXPECT_OK(pdkill(cap_capable, SIGINT));
  int rc = pdwait4_(pd, &status, 0, NULL);
  EXPECT_OK(rc);
  EXPECT_EQ(pid, rc);
}


//------------------------------------------------
// Passing process descriptors between processes.

TEST_F(PipePdfork, PassProcessDescriptor) {
  int sock_fds[2];
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds));

  struct msghdr mh;
  mh.msg_name = NULL;  // No address needed
  mh.msg_namelen = 0;
  char buffer1[1024];
  struct iovec iov[1];
  iov[0].iov_base = buffer1;
  iov[0].iov_len = sizeof(buffer1);
  mh.msg_iov = iov;
  mh.msg_iovlen = 1;
  char buffer2[1024];
  mh.msg_control = buffer2;
  mh.msg_controllen = sizeof(buffer2);
  struct cmsghdr *cmptr;

  if (verbose) fprintf(stderr, "[%d] about to fork()\n", getpid_());
  pid_t child2 = fork();
  if (child2 == 0) {
    // Child: close our copy of the original process descriptor.
    close(pd_);

    // Child: wait to receive process descriptor over socket
    if (verbose) fprintf(stderr, "  [%d] child of %d waiting for process descriptor on socket\n", getpid_(), getppid());
    int rc = recvmsg(sock_fds[0], &mh, 0);
    EXPECT_OK(rc);
    EXPECT_LE(CMSG_LEN(sizeof(int)), mh.msg_controllen);
    cmptr = CMSG_FIRSTHDR(&mh);
    int pd = *(int*)CMSG_DATA(cmptr);
    EXPECT_EQ(CMSG_LEN(sizeof(int)), cmptr->cmsg_len);
    cmptr = CMSG_NXTHDR(&mh, cmptr);
    EXPECT_TRUE(cmptr == NULL);
    if (verbose) fprintf(stderr, "  [%d] got process descriptor %d on socket\n", getpid_(), pd);

    // Child: confirm we can do pd*() operations on the process descriptor
    pid_t other;
    EXPECT_OK(pdgetpid(pd, &other));
    if (verbose) fprintf(stderr, "  [%d] process descriptor %d is pid %d\n", getpid_(), pd, other);

    sleep(2);
    if (verbose) fprintf(stderr, "  [%d] close process descriptor %d\n", getpid_(), pd);
    close(pd);

    // Last process descriptor closed, expect death
    EXPECT_PID_DEAD(other);

    exit(HasFailure());
  }
  usleep(1000);  // Ensure subprocess runs

  // Send the process descriptor over the pipe to the sub-process
  mh.msg_controllen = CMSG_LEN(sizeof(int));
  cmptr = CMSG_FIRSTHDR(&mh);
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  *(int *)CMSG_DATA(cmptr) = pd_;
  buffer1[0] = 0;
  iov[0].iov_len = 1;
  sleep(1);
  if (verbose) fprintf(stderr, "[%d] send process descriptor %d on socket\n", getpid_(), pd_);
  int rc = sendmsg(sock_fds[1], &mh, 0);
  EXPECT_OK(rc);

  if (verbose) fprintf(stderr, "[%d] close process descriptor %d\n", getpid_(), pd_);
  close(pd_);  // Not last open process descriptor

  // wait for child2
  int status;
  EXPECT_EQ(child2, waitpid(child2, &status, __WALL));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  // confirm death all round
  EXPECT_PID_DEAD(child2);
  EXPECT_PID_DEAD(pid_);
}
