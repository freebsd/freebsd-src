// Tests for POSIX message queue functionality.

#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>

#include <string>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

// Run a test case in a forked process, possibly cleaning up a
// message after completion
#define FORK_TEST_ON_MQ(test_case_name, test_name, test_mq)    \
    static void test_case_name##_##test_name##_ForkTest();     \
    TEST(test_case_name, test_name ## Forked) {                \
      _RUN_FORKED_FN(test_case_name##_##test_name##_ForkTest,  \
                     #test_case_name, #test_name);             \
      const char *mqname = test_mq;                            \
      if (mqname) mq_unlink_(mqname);                          \
    }                                                          \
    static void test_case_name##_##test_name##_ForkTest()

static bool invoked;
void seen_it_done_it(int) {
  invoked = true;
}

FORK_TEST_ON_MQ(PosixMqueue, CapModeIfMqOpenAvailable, "/cap_mq") {
  int mq = mq_open_("/cap_mq", O_RDWR|O_CREAT, 0644, NULL);
  // On FreeBSD, turn on message queue support with:
  //  - 'kldload mqueuefs'
  //  - 'options P1003_1B_MQUEUE' in kernel build config.
  if (mq < 0 && errno == ENOSYS) {
    GTEST_SKIP() << "mq_open -> -ENOSYS";
  }
  EXPECT_OK(mq);
  cap_rights_t r_read;
  cap_rights_init(&r_read, CAP_READ);
  cap_rights_t r_write;
  cap_rights_init(&r_write, CAP_WRITE);
  cap_rights_t r_poll;
  cap_rights_init(&r_poll, CAP_EVENT);

  int cap_read_mq = dup(mq);
  EXPECT_OK(cap_read_mq);
  EXPECT_OK(cap_rights_limit(cap_read_mq, &r_read));
  int cap_write_mq = dup(mq);
  EXPECT_OK(cap_write_mq);
  EXPECT_OK(cap_rights_limit(cap_write_mq, &r_write));
  int cap_poll_mq = dup(mq);
  EXPECT_OK(cap_poll_mq);
  EXPECT_OK(cap_rights_limit(cap_poll_mq, &r_poll));
  EXPECT_OK(mq_close_(mq));

  signal(SIGUSR2, seen_it_done_it);

  EXPECT_OK(cap_enter());  // Enter capability mode

  // Can no longer access the message queue via the POSIX IPC namespace.
  EXPECT_CAPMODE(mq_open_("/cap_mw", O_RDWR|O_CREAT, 0644, NULL));

  struct sigevent se;
  se.sigev_notify = SIGEV_SIGNAL;
  se.sigev_signo = SIGUSR2;
  EXPECT_OK(mq_notify_(cap_poll_mq, &se));
  EXPECT_NOTCAPABLE(mq_notify_(cap_read_mq, &se));
  EXPECT_NOTCAPABLE(mq_notify_(cap_write_mq, &se));

  const unsigned int kPriority = 10;
  const char* message = "xyzzy";
  struct timespec ts;
  ts.tv_sec = 1;
  ts.tv_nsec = 0;
  EXPECT_OK(mq_timedsend_(cap_write_mq, message, strlen(message) + 1, kPriority, &ts));
  EXPECT_NOTCAPABLE(mq_timedsend_(cap_read_mq, message, strlen(message) + 1, kPriority, &ts));

  sleep(1);  // Give the notification a chance to arrive.
  EXPECT_TRUE(invoked);

  struct mq_attr mqa;
  EXPECT_OK(mq_getattr_(cap_poll_mq, &mqa));
  EXPECT_OK(mq_setattr_(cap_poll_mq, &mqa, NULL));
  EXPECT_NOTCAPABLE(mq_getattr_(cap_write_mq, &mqa));

  char* buffer = (char *)malloc(mqa.mq_msgsize);
  unsigned int priority;
  EXPECT_NOTCAPABLE(mq_timedreceive_(cap_write_mq, buffer, mqa.mq_msgsize, &priority, &ts));
  EXPECT_OK(mq_timedreceive_(cap_read_mq, buffer, mqa.mq_msgsize, &priority, &ts));
  EXPECT_EQ(std::string(message), std::string(buffer));
  EXPECT_EQ(kPriority, priority);
  free(buffer);

  close(cap_read_mq);
  close(cap_write_mq);
  close(cap_poll_mq);
}
