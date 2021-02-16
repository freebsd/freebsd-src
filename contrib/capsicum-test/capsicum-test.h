/* -*- C++ -*- */
#ifndef CAPSICUM_TEST_H
#define CAPSICUM_TEST_H

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>

#include <ios>
#include <ostream>
#include <string>

#include "gtest/gtest.h"

extern bool verbose;
extern std::string tmpdir;
extern bool tmpdir_on_tmpfs;
extern bool force_mt;
extern bool force_nofork;
extern uid_t other_uid;

static inline void *WaitingThreadFn(void *) {
  // Loop until cancelled
  while (true) {
    usleep(10000);
    pthread_testcancel();
  }
  return NULL;
}

// If force_mt is set, run another thread in parallel with the test.  This forces
// the kernel into multi-threaded mode.
template <typename T, typename Function>
void MaybeRunWithThread(T *self, Function fn) {
  pthread_t subthread;
  if (force_mt) {
    pthread_create(&subthread, NULL, WaitingThreadFn, NULL);
  }
  (self->*fn)();
  if (force_mt) {
    pthread_cancel(subthread);
    pthread_join(subthread, NULL);
  }
}
template <typename Function>
void MaybeRunWithThread(Function fn) {
  pthread_t subthread;
  if (force_mt) {
    pthread_create(&subthread, NULL, WaitingThreadFn, NULL);
  }
  (fn)();
  if (force_mt) {
    pthread_cancel(subthread);
    pthread_join(subthread, NULL);
  }
}

// Return the absolute path of a filename in the temp directory, `tmpdir`,
// with the given pathname, e.g., "/tmp/<pathname>", if `tmpdir` was set to
// "/tmp".
const char *TmpFile(const char *pathname);

// Run the given test function in a forked process, so that trapdoor
// entry doesn't affect other tests, and watch out for hung processes.
// Implemented as a macro to allow access to the test case instance's
// HasFailure() method, which is reported as the forked process's
// exit status.
#define _RUN_FORKED(INNERCODE, TESTCASENAME, TESTNAME)         \
    pid_t pid = force_nofork ? 0 : fork();                     \
    if (pid == 0) {                                            \
      INNERCODE;                                               \
      if (!force_nofork) {                                     \
        exit(HasFailure());                                    \
      }                                                        \
    } else if (pid > 0) {                                      \
      int rc, status;                                          \
      int remaining_us = 30000000;                             \
      while (remaining_us > 0) {                               \
        status = 0;                                            \
        rc = waitpid(pid, &status, WNOHANG);                   \
        if (rc != 0) break;                                    \
        remaining_us -= 10000;                                 \
        usleep(10000);                                         \
      }                                                        \
      if (remaining_us <= 0) {                                 \
        fprintf(stderr, "Warning: killing unresponsive test "  \
                        "%s.%s (pid %d)\n",                    \
                        TESTCASENAME, TESTNAME, pid);          \
        kill(pid, SIGKILL);                                    \
        ADD_FAILURE() << "Test hung";                          \
      } else if (rc < 0) {                                     \
        fprintf(stderr, "Warning: waitpid error %s (%d)\n",    \
                        strerror(errno), errno);               \
        ADD_FAILURE() << "Failed to wait for child";           \
      } else {                                                 \
        int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1; \
        EXPECT_EQ(0, rc);                                      \
      }                                                        \
    }
#define _RUN_FORKED_MEM(THIS, TESTFN, TESTCASENAME, TESTNAME)  \
  _RUN_FORKED(MaybeRunWithThread(THIS, &TESTFN), TESTCASENAME, TESTNAME);
#define _RUN_FORKED_FN(TESTFN, TESTCASENAME, TESTNAME)   \
  _RUN_FORKED(MaybeRunWithThread(&TESTFN), TESTCASENAME, TESTNAME);

// Run a test case in a forked process, possibly cleaning up a
// test file after completion
#define FORK_TEST_ON(test_case_name, test_name, test_file)     \
    static void test_case_name##_##test_name##_ForkTest();     \
    TEST(test_case_name, test_name ## Forked) {                \
      _RUN_FORKED_FN(test_case_name##_##test_name##_ForkTest,  \
                     #test_case_name, #test_name);             \
      const char *filename = test_file;                        \
      if (filename) unlink(filename);                          \
    }                                                          \
    static void test_case_name##_##test_name##_ForkTest()

#define FORK_TEST(test_case_name, test_name) FORK_TEST_ON(test_case_name, test_name, NULL)

// Run a test case fixture in a forked process, so that trapdoors don't
// affect other tests.
#define ICLASS_NAME(test_case_name, test_name) Forked##test_case_name##_##test_name
#define FORK_TEST_F(test_case_name, test_name)                \
  class ICLASS_NAME(test_case_name, test_name) : public test_case_name { \
    public:                                                    \
      ICLASS_NAME(test_case_name, test_name)() {}              \
      void InnerTestBody();                                    \
    };                                                         \
    TEST_F(ICLASS_NAME(test_case_name, test_name), _) {        \
      _RUN_FORKED_MEM(this,                                    \
                      ICLASS_NAME(test_case_name, test_name)::InnerTestBody,  \
                      #test_case_name, #test_name);            \
    }                                                          \
    void ICLASS_NAME(test_case_name, test_name)::InnerTestBody()

// Emit errno information on failure
#define EXPECT_OK(v) EXPECT_LE(0, v) << "   errno " << errno << " " << strerror(errno)

// Expect a syscall to fail with the given error.
#define EXPECT_SYSCALL_FAIL(E, C) \
    do { \
      EXPECT_GT(0, C); \
      EXPECT_EQ(E, errno); \
    } while (0)

// Expect a syscall to fail with anything other than the given error.
#define EXPECT_SYSCALL_FAIL_NOT(E, C) \
    do { \
      EXPECT_GT(0, C); \
      EXPECT_NE(E, errno); \
    } while (0)

// Expect a void syscall to fail with anything other than the given error.
#define EXPECT_VOID_SYSCALL_FAIL_NOT(E, C)   \
    do { \
      errno = 0; \
      C; \
      EXPECT_NE(E, errno) << #C << " failed with ECAPMODE"; \
    } while (0)

// Expect a system call to fail due to path traversal; exact error
// code is OS-specific.
#ifdef O_BENEATH
#define EXPECT_OPENAT_FAIL_TRAVERSAL(fd, path, flags) \
    do { \
      const int result = openat((fd), (path), (flags)); \
      if (((flags) & O_BENEATH) == O_BENEATH) { \
        EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_O_BENEATH, result); \
      } else { \
        EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_CAPABILITY, result); \
      } \
      if (result >= 0) { close(result); } \
    } while (0)
#else
#define EXPECT_OPENAT_FAIL_TRAVERSAL(fd, path, flags) \
    do { \
      const int result = openat((fd), (path), (flags)); \
      EXPECT_SYSCALL_FAIL(E_NO_TRAVERSE_CAPABILITY, result); \
      if (result >= 0) { close(result); } \
    } while (0)
#endif

// Expect a system call to fail with ECAPMODE.
#define EXPECT_CAPMODE(C) EXPECT_SYSCALL_FAIL(ECAPMODE, C)

// Expect a system call to fail, but not with ECAPMODE.
#define EXPECT_FAIL_NOT_CAPMODE(C) EXPECT_SYSCALL_FAIL_NOT(ECAPMODE, C)
#define EXPECT_FAIL_VOID_NOT_CAPMODE(C) EXPECT_VOID_SYSCALL_FAIL_NOT(ECAPMODE, C)

// Expect a system call to fail with ENOTCAPABLE.
#define EXPECT_NOTCAPABLE(C) EXPECT_SYSCALL_FAIL(ENOTCAPABLE, C)

// Expect a system call to fail, but not with ENOTCAPABLE.
#define EXPECT_FAIL_NOT_NOTCAPABLE(C) EXPECT_SYSCALL_FAIL_NOT(ENOTCAPABLE, C)

// Expect a system call to fail with either ENOTCAPABLE or ECAPMODE.
#define EXPECT_CAPFAIL(C) \
    do { \
      int rc = C; \
      EXPECT_GT(0, rc); \
      EXPECT_TRUE(errno == ECAPMODE || errno == ENOTCAPABLE) \
        << #C << " did not fail with ECAPMODE/ENOTCAPABLE but " << errno; \
    } while (0)

// Ensure that 'rights' are a subset of 'max'.
#define EXPECT_RIGHTS_IN(rights, max) \
    EXPECT_TRUE(cap_rights_contains((max), (rights)))  \
    << "rights " << std::hex << *(rights) \
    << " not a subset of " << std::hex << *(max)

// Ensure rights are identical
#define EXPECT_RIGHTS_EQ(a, b) \
  do { \
    EXPECT_RIGHTS_IN((a), (b)); \
    EXPECT_RIGHTS_IN((b), (a)); \
  } while (0)

// Get the state of a process as a single character.
//  - 'D': disk wait
//  - 'R': runnable
//  - 'S': sleeping/idle
//  - 'T': stopped
//  - 'Z': zombie
// On error, return either '?' or '\0'.
char ProcessState(int pid);

// Check process state reaches a particular expected state (or two).
// Retries a few times to allow for timing issues.
#define EXPECT_PID_REACHES_STATES(pid, expected1, expected2) { \
  int counter = 5; \
  char state; \
  do { \
    state = ProcessState(pid); \
    if (state == expected1 || state == expected2) break; \
    usleep(100000); \
  } while (--counter > 0); \
  EXPECT_TRUE(state == expected1 || state == expected2) \
      << " pid " << pid << " in state " << state; \
}

#define EXPECT_PID_ALIVE(pid)   EXPECT_PID_REACHES_STATES(pid, 'R', 'S')
#define EXPECT_PID_DEAD(pid)    EXPECT_PID_REACHES_STATES(pid, 'Z', '\0')
#define EXPECT_PID_ZOMBIE(pid)  EXPECT_PID_REACHES_STATES(pid, 'Z', 'Z');
#define EXPECT_PID_GONE(pid)    EXPECT_PID_REACHES_STATES(pid, '\0', '\0');

// Mark a test that can only be run as root.
#define GTEST_SKIP_IF_NOT_ROOT() \
  if (getuid() != 0) { GTEST_SKIP() << "requires root"; }

extern std::string capsicum_test_bindir;

#endif  // CAPSICUM_TEST_H
