#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

namespace {

int AddFDToSet(fd_set* fset, int fd, int maxfd) {
  FD_SET(fd, fset);
  if (fd > maxfd) maxfd = fd;
  return maxfd;
}

int InitFDSet(fd_set* fset, int *fds, int fdcount) {
  FD_ZERO(fset);
  int maxfd = -1;
  for (int ii = 0; ii < fdcount; ii++) {
    maxfd = AddFDToSet(fset, fds[ii], maxfd);
  }
  return maxfd;
}

}  // namespace

FORK_TEST_ON(Select, LotsOFileDescriptors, TmpFile("cap_select")) {
  int fd = open(TmpFile("cap_select"), O_RDWR | O_CREAT, 0644);
  EXPECT_OK(fd);
  if (fd < 0) return;

  // Create many POLL_EVENT capabilities.
  const int kCapCount = 64;
  int cap_fd[kCapCount];
  cap_rights_t r_poll;
  cap_rights_init(&r_poll, CAP_EVENT);
  for (int ii = 0; ii < kCapCount; ii++) {
    cap_fd[ii] = dup(fd);
    EXPECT_OK(cap_fd[ii]);
    EXPECT_OK(cap_rights_limit(cap_fd[ii], &r_poll));
  }
  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE, CAP_SEEK);
  int cap_rw = dup(fd);
  EXPECT_OK(cap_rw);
  EXPECT_OK(cap_rights_limit(cap_rw, &r_rw));

  EXPECT_OK(cap_enter());  // Enter capability mode

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100;
  // Add normal file descriptor and all CAP_EVENT capabilities
  fd_set rset;
  fd_set wset;
  int maxfd = InitFDSet(&rset, cap_fd, kCapCount);
  maxfd = AddFDToSet(&rset, fd, maxfd);
  InitFDSet(&wset, cap_fd, kCapCount);
  AddFDToSet(&rset, fd, 0);
  int ret = select(maxfd+1, &rset, &wset, NULL, &tv);
  EXPECT_OK(ret);

  // Now also include the capability with no CAP_EVENT.
  InitFDSet(&rset, cap_fd, kCapCount);
  AddFDToSet(&rset, fd, maxfd);
  maxfd = AddFDToSet(&rset, cap_rw, maxfd);
  InitFDSet(&wset, cap_fd, kCapCount);
  AddFDToSet(&wset, fd, maxfd);
  AddFDToSet(&wset, cap_rw, maxfd);
  ret = select(maxfd+1, &rset, &wset, NULL, &tv);
  EXPECT_NOTCAPABLE(ret);

#ifdef HAVE_PSELECT
  // And again with pselect
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 100000;
  maxfd = InitFDSet(&rset, cap_fd, kCapCount);
  maxfd = AddFDToSet(&rset, fd, maxfd);
  InitFDSet(&wset, cap_fd, kCapCount);
  AddFDToSet(&rset, fd, 0);
  ret = pselect(maxfd+1, &rset, &wset, NULL, &ts, NULL);
  EXPECT_OK(ret);

  InitFDSet(&rset, cap_fd, kCapCount);
  AddFDToSet(&rset, fd, maxfd);
  maxfd = AddFDToSet(&rset, cap_rw, maxfd);
  InitFDSet(&wset, cap_fd, kCapCount);
  AddFDToSet(&wset, fd, maxfd);
  AddFDToSet(&wset, cap_rw, maxfd);
  ret = pselect(maxfd+1, &rset, &wset, NULL, &ts, NULL);
  EXPECT_NOTCAPABLE(ret);
#endif
}

FORK_TEST_ON(Poll, LotsOFileDescriptors, TmpFile("cap_poll")) {
  int fd = open(TmpFile("cap_poll"), O_RDWR | O_CREAT, 0644);
  EXPECT_OK(fd);
  if (fd < 0) return;

  // Create many POLL_EVENT capabilities.
  const int kCapCount = 64;
  struct pollfd cap_fd[kCapCount + 2];
  cap_rights_t r_poll;
  cap_rights_init(&r_poll, CAP_EVENT);
  for (int ii = 0; ii < kCapCount; ii++) {
    cap_fd[ii].fd = dup(fd);
    EXPECT_OK(cap_fd[ii].fd);
    EXPECT_OK(cap_rights_limit(cap_fd[ii].fd, &r_poll));
    cap_fd[ii].events = POLLIN|POLLOUT;
  }
  cap_fd[kCapCount].fd = fd;
  cap_fd[kCapCount].events = POLLIN|POLLOUT;
  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE, CAP_SEEK);
  int cap_rw = dup(fd);
  EXPECT_OK(cap_rw);
  EXPECT_OK(cap_rights_limit(cap_rw, &r_rw));
  cap_fd[kCapCount + 1].fd = cap_rw;
  cap_fd[kCapCount + 1].events = POLLIN|POLLOUT;

  EXPECT_OK(cap_enter());  // Enter capability mode

  EXPECT_OK(poll(cap_fd, kCapCount + 1, 10));
  // Now also include the capability with no CAP_EVENT.
  EXPECT_OK(poll(cap_fd, kCapCount + 2, 10));
  EXPECT_NE(0, (cap_fd[kCapCount + 1].revents & POLLNVAL));

#ifdef HAVE_PPOLL
  // And again with ppoll
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 100000;
  EXPECT_OK(ppoll(cap_fd, kCapCount + 1, &ts, NULL));
  // Now also include the capability with no CAP_EVENT.
  EXPECT_OK(ppoll(cap_fd, kCapCount + 2, &ts, NULL));
  EXPECT_NE(0, (cap_fd[kCapCount + 1].revents & POLLNVAL));
#endif
}
