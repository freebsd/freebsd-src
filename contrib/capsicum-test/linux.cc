// Tests of Linux-specific functionality
#ifdef __linux__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/fanotify.h>
#include <sys/mman.h>
#include <sys/capability.h>  // Requires e.g. libcap-dev package for POSIX.1e capabilities headers
#include <linux/aio_abi.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/version.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

TEST(Linux, TimerFD) {
  int fd = timerfd_create(CLOCK_MONOTONIC, 0);

  cap_rights_t r_ro;
  cap_rights_init(&r_ro, CAP_READ);
  cap_rights_t r_wo;
  cap_rights_init(&r_wo, CAP_WRITE);
  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);
  cap_rights_t r_rwpoll;
  cap_rights_init(&r_rwpoll, CAP_READ, CAP_WRITE, CAP_EVENT);

  int cap_fd_ro = dup(fd);
  EXPECT_OK(cap_fd_ro);
  EXPECT_OK(cap_rights_limit(cap_fd_ro, &r_ro));
  int cap_fd_wo = dup(fd);
  EXPECT_OK(cap_fd_wo);
  EXPECT_OK(cap_rights_limit(cap_fd_wo, &r_wo));
  int cap_fd_rw = dup(fd);
  EXPECT_OK(cap_fd_rw);
  EXPECT_OK(cap_rights_limit(cap_fd_rw, &r_rw));
  int cap_fd_all = dup(fd);
  EXPECT_OK(cap_fd_all);
  EXPECT_OK(cap_rights_limit(cap_fd_all, &r_rwpoll));

  struct itimerspec old_ispec;
  struct itimerspec ispec;
  ispec.it_interval.tv_sec = 0;
  ispec.it_interval.tv_nsec = 0;
  ispec.it_value.tv_sec = 0;
  ispec.it_value.tv_nsec = 100000000;  // 100ms
  EXPECT_NOTCAPABLE(timerfd_settime(cap_fd_ro, 0, &ispec, NULL));
  EXPECT_NOTCAPABLE(timerfd_settime(cap_fd_wo, 0, &ispec, &old_ispec));
  EXPECT_OK(timerfd_settime(cap_fd_wo, 0, &ispec, NULL));
  EXPECT_OK(timerfd_settime(cap_fd_rw, 0, &ispec, NULL));
  EXPECT_OK(timerfd_settime(cap_fd_all, 0, &ispec, NULL));

  EXPECT_NOTCAPABLE(timerfd_gettime(cap_fd_wo, &old_ispec));
  EXPECT_OK(timerfd_gettime(cap_fd_ro, &old_ispec));
  EXPECT_OK(timerfd_gettime(cap_fd_rw, &old_ispec));
  EXPECT_OK(timerfd_gettime(cap_fd_all, &old_ispec));

  // To be able to poll() for the timer pop, still need CAP_EVENT.
  struct pollfd poll_fd;
  for (int ii = 0; ii < 3; ii++) {
    poll_fd.revents = 0;
    poll_fd.events = POLLIN;
    switch (ii) {
    case 0: poll_fd.fd = cap_fd_ro; break;
    case 1: poll_fd.fd = cap_fd_wo; break;
    case 2: poll_fd.fd = cap_fd_rw; break;
    }
    // Poll immediately returns with POLLNVAL
    EXPECT_OK(poll(&poll_fd, 1, 400));
    EXPECT_EQ(0, (poll_fd.revents & POLLIN));
    EXPECT_NE(0, (poll_fd.revents & POLLNVAL));
  }

  poll_fd.fd = cap_fd_all;
  EXPECT_OK(poll(&poll_fd, 1, 400));
  EXPECT_NE(0, (poll_fd.revents & POLLIN));
  EXPECT_EQ(0, (poll_fd.revents & POLLNVAL));

  EXPECT_OK(timerfd_gettime(cap_fd_all, &old_ispec));
  EXPECT_EQ(0, old_ispec.it_value.tv_sec);
  EXPECT_EQ(0, old_ispec.it_value.tv_nsec);
  EXPECT_EQ(0, old_ispec.it_interval.tv_sec);
  EXPECT_EQ(0, old_ispec.it_interval.tv_nsec);

  close(cap_fd_all);
  close(cap_fd_rw);
  close(cap_fd_wo);
  close(cap_fd_ro);
  close(fd);
}

FORK_TEST(Linux, SignalFDIfSingleThreaded) {
  if (force_mt) {
    GTEST_SKIP() << "multi-threaded run clashes with signals";
  }
  pid_t me = getpid();
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);

  // Block signals before registering against a new signal FD.
  EXPECT_OK(sigprocmask(SIG_BLOCK, &mask, NULL));
  int fd = signalfd(-1, &mask, 0);
  EXPECT_OK(fd);

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_sig;
  cap_rights_init(&r_sig, CAP_FSIGNAL);
  cap_rights_t r_rssig;
  cap_rights_init(&r_rssig, CAP_FSIGNAL, CAP_READ, CAP_SEEK);
  cap_rights_t r_rssig_poll;
  cap_rights_init(&r_rssig_poll, CAP_FSIGNAL, CAP_READ, CAP_SEEK, CAP_EVENT);

  // Various capability variants.
  int cap_fd_none = dup(fd);
  EXPECT_OK(cap_fd_none);
  EXPECT_OK(cap_rights_limit(cap_fd_none, &r_ws));
  int cap_fd_read = dup(fd);
  EXPECT_OK(cap_fd_read);
  EXPECT_OK(cap_rights_limit(cap_fd_read, &r_rs));
  int cap_fd_sig = dup(fd);
  EXPECT_OK(cap_fd_sig);
  EXPECT_OK(cap_rights_limit(cap_fd_sig, &r_sig));
  int cap_fd_sig_read = dup(fd);
  EXPECT_OK(cap_fd_sig_read);
  EXPECT_OK(cap_rights_limit(cap_fd_sig_read, &r_rssig));
  int cap_fd_all = dup(fd);
  EXPECT_OK(cap_fd_all);
  EXPECT_OK(cap_rights_limit(cap_fd_all, &r_rssig_poll));

  struct signalfd_siginfo fdsi;

  // Need CAP_READ to read the signal information
  kill(me, SIGUSR1);
  EXPECT_NOTCAPABLE(read(cap_fd_none, &fdsi, sizeof(struct signalfd_siginfo)));
  EXPECT_NOTCAPABLE(read(cap_fd_sig, &fdsi, sizeof(struct signalfd_siginfo)));
  int len = read(cap_fd_read, &fdsi, sizeof(struct signalfd_siginfo));
  EXPECT_OK(len);
  EXPECT_EQ(sizeof(struct signalfd_siginfo), (size_t)len);
  EXPECT_EQ(SIGUSR1, (int)fdsi.ssi_signo);

  // Need CAP_FSIGNAL to modify the signal mask.
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  EXPECT_OK(sigprocmask(SIG_BLOCK, &mask, NULL));
  EXPECT_NOTCAPABLE(signalfd(cap_fd_none, &mask, 0));
  EXPECT_NOTCAPABLE(signalfd(cap_fd_read, &mask, 0));
  EXPECT_EQ(cap_fd_sig, signalfd(cap_fd_sig, &mask, 0));

  // Need CAP_EVENT to get notification of a signal in poll(2).
  kill(me, SIGUSR2);

  struct pollfd poll_fd;
  poll_fd.revents = 0;
  poll_fd.events = POLLIN;
  poll_fd.fd = cap_fd_sig_read;
  EXPECT_OK(poll(&poll_fd, 1, 400));
  EXPECT_EQ(0, (poll_fd.revents & POLLIN));
  EXPECT_NE(0, (poll_fd.revents & POLLNVAL));

  poll_fd.fd = cap_fd_all;
  EXPECT_OK(poll(&poll_fd, 1, 400));
  EXPECT_NE(0, (poll_fd.revents & POLLIN));
  EXPECT_EQ(0, (poll_fd.revents & POLLNVAL));
}

TEST(Linux, EventFD) {
  int fd = eventfd(0, 0);
  EXPECT_OK(fd);

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rwspoll;
  cap_rights_init(&r_rwspoll, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_EVENT);

  int cap_ro = dup(fd);
  EXPECT_OK(cap_ro);
  EXPECT_OK(cap_rights_limit(cap_ro, &r_rs));
  int cap_wo = dup(fd);
  EXPECT_OK(cap_wo);
  EXPECT_OK(cap_rights_limit(cap_wo, &r_ws));
  int cap_rw = dup(fd);
  EXPECT_OK(cap_rw);
  EXPECT_OK(cap_rights_limit(cap_rw, &r_rws));
  int cap_all = dup(fd);
  EXPECT_OK(cap_all);
  EXPECT_OK(cap_rights_limit(cap_all, &r_rwspoll));

  pid_t child = fork();
  if (child == 0) {
    // Child: write counter to eventfd
    uint64_t u = 42;
    EXPECT_NOTCAPABLE(write(cap_ro, &u, sizeof(u)));
    EXPECT_OK(write(cap_wo, &u, sizeof(u)));
    exit(HasFailure());
  }

  sleep(1);  // Allow child to write

  struct pollfd poll_fd;
  poll_fd.revents = 0;
  poll_fd.events = POLLIN;
  poll_fd.fd = cap_rw;
  EXPECT_OK(poll(&poll_fd, 1, 400));
  EXPECT_EQ(0, (poll_fd.revents & POLLIN));
  EXPECT_NE(0, (poll_fd.revents & POLLNVAL));

  poll_fd.fd = cap_all;
  EXPECT_OK(poll(&poll_fd, 1, 400));
  EXPECT_NE(0, (poll_fd.revents & POLLIN));
  EXPECT_EQ(0, (poll_fd.revents & POLLNVAL));

  uint64_t u;
  EXPECT_NOTCAPABLE(read(cap_wo, &u, sizeof(u)));
  EXPECT_OK(read(cap_ro, &u, sizeof(u)));
  EXPECT_EQ(42, (int)u);

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(cap_all);
  close(cap_rw);
  close(cap_wo);
  close(cap_ro);
  close(fd);
}

FORK_TEST(Linux, epoll) {
  int sock_fds[2];
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds));
  // Queue some data.
  char buffer[4] = {1, 2, 3, 4};
  EXPECT_OK(write(sock_fds[1], buffer, sizeof(buffer)));

  EXPECT_OK(cap_enter());  // Enter capability mode.

  int epoll_fd = epoll_create(1);
  EXPECT_OK(epoll_fd);

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rwspoll;
  cap_rights_init(&r_rwspoll, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_EVENT);
  cap_rights_t r_epoll;
  cap_rights_init(&r_epoll, CAP_EPOLL_CTL);

  int cap_epoll_wo = dup(epoll_fd);
  EXPECT_OK(cap_epoll_wo);
  EXPECT_OK(cap_rights_limit(cap_epoll_wo, &r_ws));
  int cap_epoll_ro = dup(epoll_fd);
  EXPECT_OK(cap_epoll_ro);
  EXPECT_OK(cap_rights_limit(cap_epoll_ro, &r_rs));
  int cap_epoll_rw = dup(epoll_fd);
  EXPECT_OK(cap_epoll_rw);
  EXPECT_OK(cap_rights_limit(cap_epoll_rw, &r_rws));
  int cap_epoll_poll = dup(epoll_fd);
  EXPECT_OK(cap_epoll_poll);
  EXPECT_OK(cap_rights_limit(cap_epoll_poll, &r_rwspoll));
  int cap_epoll_ctl = dup(epoll_fd);
  EXPECT_OK(cap_epoll_ctl);
  EXPECT_OK(cap_rights_limit(cap_epoll_ctl, &r_epoll));

  // Can only modify the FDs being monitored if the CAP_EPOLL_CTL right is present.
  struct epoll_event eev;
  memset(&eev, 0, sizeof(eev));
  eev.events = EPOLLIN|EPOLLOUT|EPOLLPRI;
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_ro, EPOLL_CTL_ADD, sock_fds[0], &eev));
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_wo, EPOLL_CTL_ADD, sock_fds[0], &eev));
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_rw, EPOLL_CTL_ADD, sock_fds[0], &eev));
  EXPECT_OK(epoll_ctl(cap_epoll_ctl, EPOLL_CTL_ADD, sock_fds[0], &eev));
  eev.events = EPOLLIN|EPOLLOUT;
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_ro, EPOLL_CTL_MOD, sock_fds[0], &eev));
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_wo, EPOLL_CTL_MOD, sock_fds[0], &eev));
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_rw, EPOLL_CTL_MOD, sock_fds[0], &eev));
  EXPECT_OK(epoll_ctl(cap_epoll_ctl, EPOLL_CTL_MOD, sock_fds[0], &eev));

  // Running epoll_pwait(2) requires CAP_EVENT.
  eev.events = 0;
  EXPECT_NOTCAPABLE(epoll_pwait(cap_epoll_ro, &eev, 1, 100, NULL));
  EXPECT_NOTCAPABLE(epoll_pwait(cap_epoll_wo, &eev, 1, 100, NULL));
  EXPECT_NOTCAPABLE(epoll_pwait(cap_epoll_rw, &eev, 1, 100, NULL));
  EXPECT_OK(epoll_pwait(cap_epoll_poll, &eev, 1, 100, NULL));
  EXPECT_EQ(EPOLLIN, eev.events & EPOLLIN);

  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_ro, EPOLL_CTL_DEL, sock_fds[0], &eev));
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_wo, EPOLL_CTL_DEL, sock_fds[0], &eev));
  EXPECT_NOTCAPABLE(epoll_ctl(cap_epoll_rw, EPOLL_CTL_DEL, sock_fds[0], &eev));
  EXPECT_OK(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock_fds[0], &eev));

  close(cap_epoll_ctl);
  close(cap_epoll_poll);
  close(cap_epoll_rw);
  close(cap_epoll_ro);
  close(cap_epoll_wo);
  close(epoll_fd);
  close(sock_fds[1]);
  close(sock_fds[0]);
}

TEST(Linux, fstatat) {
  int fd = open(TmpFile("cap_fstatat"), O_CREAT|O_RDWR, 0644);
  EXPECT_OK(fd);
  unsigned char buffer[] = {1, 2, 3, 4};
  EXPECT_OK(write(fd, buffer, sizeof(buffer)));
  cap_rights_t rights;
  int cap_rf = dup(fd);
  EXPECT_OK(cap_rf);
  EXPECT_OK(cap_rights_limit(cap_rf, cap_rights_init(&rights, CAP_READ, CAP_FSTAT)));
  int cap_ro = dup(fd);
  EXPECT_OK(cap_ro);
  EXPECT_OK(cap_rights_limit(cap_ro, cap_rights_init(&rights, CAP_READ)));

  struct stat info;
  EXPECT_OK(fstatat(fd, "", &info, AT_EMPTY_PATH));
  EXPECT_NOTCAPABLE(fstatat(cap_ro, "", &info, AT_EMPTY_PATH));
  EXPECT_OK(fstatat(cap_rf, "", &info, AT_EMPTY_PATH));

  close(cap_ro);
  close(cap_rf);
  close(fd);

  int dir = open(tmpdir.c_str(), O_RDONLY);
  EXPECT_OK(dir);
  int dir_rf = dup(dir);
  EXPECT_OK(dir_rf);
  EXPECT_OK(cap_rights_limit(dir_rf, cap_rights_init(&rights, CAP_READ, CAP_FSTAT)));
  int dir_ro = dup(fd);
  EXPECT_OK(dir_ro);
  EXPECT_OK(cap_rights_limit(dir_ro, cap_rights_init(&rights, CAP_READ)));

  EXPECT_OK(fstatat(dir, "cap_fstatat", &info, AT_EMPTY_PATH));
  EXPECT_NOTCAPABLE(fstatat(dir_ro, "cap_fstatat", &info, AT_EMPTY_PATH));
  EXPECT_OK(fstatat(dir_rf, "cap_fstatat", &info, AT_EMPTY_PATH));

  close(dir_ro);
  close(dir_rf);
  close(dir);

  unlink(TmpFile("cap_fstatat"));
}

// fanotify support may not be available at compile-time
#ifdef __NR_fanotify_init
TEST(Linux, FanotifyIfRoot) {
  GTEST_SKIP_IF_NOT_ROOT();
  int fa_fd = fanotify_init(FAN_CLASS_NOTIF, O_RDWR);
  EXPECT_OK(fa_fd);
  if (fa_fd < 0) return;  // May not be enabled

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rwspoll;
  cap_rights_init(&r_rwspoll, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_EVENT);
  cap_rights_t r_rwsnotify;
  cap_rights_init(&r_rwsnotify, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_NOTIFY);
  cap_rights_t r_rsl;
  cap_rights_init(&r_rsl, CAP_READ, CAP_SEEK, CAP_LOOKUP);
  cap_rights_t r_rslstat;
  cap_rights_init(&r_rslstat, CAP_READ, CAP_SEEK, CAP_LOOKUP, CAP_FSTAT);
  cap_rights_t r_rsstat;
  cap_rights_init(&r_rsstat, CAP_READ, CAP_SEEK, CAP_FSTAT);

  int cap_fd_ro = dup(fa_fd);
  EXPECT_OK(cap_fd_ro);
  EXPECT_OK(cap_rights_limit(cap_fd_ro, &r_rs));
  int cap_fd_wo = dup(fa_fd);
  EXPECT_OK(cap_fd_wo);
  EXPECT_OK(cap_rights_limit(cap_fd_wo, &r_ws));
  int cap_fd_rw = dup(fa_fd);
  EXPECT_OK(cap_fd_rw);
  EXPECT_OK(cap_rights_limit(cap_fd_rw, &r_rws));
  int cap_fd_poll = dup(fa_fd);
  EXPECT_OK(cap_fd_poll);
  EXPECT_OK(cap_rights_limit(cap_fd_poll, &r_rwspoll));
  int cap_fd_not = dup(fa_fd);
  EXPECT_OK(cap_fd_not);
  EXPECT_OK(cap_rights_limit(cap_fd_not, &r_rwsnotify));

  int rc = mkdir(TmpFile("cap_notify"), 0755);
  EXPECT_TRUE(rc == 0 || errno == EEXIST);
  int dfd = open(TmpFile("cap_notify"), O_RDONLY);
  EXPECT_OK(dfd);
  int fd = open(TmpFile("cap_notify/file"), O_CREAT|O_RDWR, 0644);
  close(fd);
  int cap_dfd = dup(dfd);
  EXPECT_OK(cap_dfd);
  EXPECT_OK(cap_rights_limit(cap_dfd, &r_rslstat));
  EXPECT_OK(cap_dfd);
  int cap_dfd_rs = dup(dfd);
  EXPECT_OK(cap_dfd_rs);
  EXPECT_OK(cap_rights_limit(cap_dfd_rs, &r_rs));
  EXPECT_OK(cap_dfd_rs);
  int cap_dfd_rsstat = dup(dfd);
  EXPECT_OK(cap_dfd_rsstat);
  EXPECT_OK(cap_rights_limit(cap_dfd_rsstat, &r_rsstat));
  EXPECT_OK(cap_dfd_rsstat);
  int cap_dfd_rsl = dup(dfd);
  EXPECT_OK(cap_dfd_rsl);
  EXPECT_OK(cap_rights_limit(cap_dfd_rsl, &r_rsl));
  EXPECT_OK(cap_dfd_rsl);

  // Need CAP_NOTIFY to change what's monitored.
  EXPECT_NOTCAPABLE(fanotify_mark(cap_fd_ro, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY|FAN_EVENT_ON_CHILD, cap_dfd, NULL));
  EXPECT_NOTCAPABLE(fanotify_mark(cap_fd_wo, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY|FAN_EVENT_ON_CHILD, cap_dfd, NULL));
  EXPECT_NOTCAPABLE(fanotify_mark(cap_fd_rw, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY|FAN_EVENT_ON_CHILD, cap_dfd, NULL));
  EXPECT_OK(fanotify_mark(cap_fd_not, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY|FAN_EVENT_ON_CHILD, cap_dfd, NULL));

  // Need CAP_FSTAT on the thing monitored.
  EXPECT_NOTCAPABLE(fanotify_mark(cap_fd_not, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY|FAN_EVENT_ON_CHILD, cap_dfd_rs, NULL));
  EXPECT_OK(fanotify_mark(cap_fd_not, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY|FAN_EVENT_ON_CHILD, cap_dfd_rsstat, NULL));

  // Too add monitoring of a file under a dfd, need CAP_LOOKUP|CAP_FSTAT on the dfd.
  EXPECT_NOTCAPABLE(fanotify_mark(cap_fd_not, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY, cap_dfd_rsstat, "file"));
  EXPECT_NOTCAPABLE(fanotify_mark(cap_fd_not, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY, cap_dfd_rsl, "file"));
  EXPECT_OK(fanotify_mark(cap_fd_not, FAN_MARK_ADD, FAN_OPEN|FAN_MODIFY, cap_dfd, "file"));

  pid_t child = fork();
  if (child == 0) {
    // Child: Perform activity in the directory under notify.
    sleep(1);
    unlink(TmpFile("cap_notify/temp"));
    int fd = open(TmpFile("cap_notify/temp"), O_CREAT|O_RDWR, 0644);
    close(fd);
    exit(0);
  }

  // Need CAP_EVENT to poll.
  struct pollfd poll_fd;
  poll_fd.revents = 0;
  poll_fd.events = POLLIN;
  poll_fd.fd = cap_fd_rw;
  EXPECT_OK(poll(&poll_fd, 1, 1400));
  EXPECT_EQ(0, (poll_fd.revents & POLLIN));
  EXPECT_NE(0, (poll_fd.revents & POLLNVAL));

  poll_fd.fd = cap_fd_not;
  EXPECT_OK(poll(&poll_fd, 1, 1400));
  EXPECT_EQ(0, (poll_fd.revents & POLLIN));
  EXPECT_NE(0, (poll_fd.revents & POLLNVAL));

  poll_fd.fd = cap_fd_poll;
  EXPECT_OK(poll(&poll_fd, 1, 1400));
  EXPECT_NE(0, (poll_fd.revents & POLLIN));
  EXPECT_EQ(0, (poll_fd.revents & POLLNVAL));

  // Need CAP_READ to read.
  struct fanotify_event_metadata ev;
  memset(&ev, 0, sizeof(ev));
  EXPECT_NOTCAPABLE(read(cap_fd_wo, &ev, sizeof(ev)));
  rc = read(fa_fd, &ev, sizeof(ev));
  EXPECT_OK(rc);
  EXPECT_EQ((int)sizeof(struct fanotify_event_metadata), rc);
  EXPECT_EQ(child, ev.pid);
  EXPECT_NE(0, ev.fd);

  // TODO(drysdale): reinstate if/when capsicum-linux propagates rights
  // to fanotify-generated FDs.
#ifdef OMIT
  // fanotify(7) gives us a FD for the changed file.  This should
  // only have rights that are a subset of those for the original
  // monitored directory file descriptor.
  cap_rights_t rights;
  CAP_SET_ALL(&rights);
  EXPECT_OK(cap_rights_get(ev.fd, &rights));
  EXPECT_RIGHTS_IN(&rights, &r_rslstat);
#endif

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(cap_dfd_rsstat);
  close(cap_dfd_rsl);
  close(cap_dfd_rs);
  close(cap_dfd);
  close(dfd);
  unlink(TmpFile("cap_notify/file"));
  unlink(TmpFile("cap_notify/temp"));
  rmdir(TmpFile("cap_notify"));
  close(cap_fd_not);
  close(cap_fd_poll);
  close(cap_fd_rw);
  close(cap_fd_wo);
  close(cap_fd_ro);
  close(fa_fd);
}
#endif

TEST(Linux, inotify) {
  int i_fd = inotify_init();
  EXPECT_OK(i_fd);

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rws;
  cap_rights_init(&r_rws, CAP_READ, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rwsnotify;
  cap_rights_init(&r_rwsnotify, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_NOTIFY);

  int cap_fd_ro = dup(i_fd);
  EXPECT_OK(cap_fd_ro);
  EXPECT_OK(cap_rights_limit(cap_fd_ro, &r_rs));
  int cap_fd_wo = dup(i_fd);
  EXPECT_OK(cap_fd_wo);
  EXPECT_OK(cap_rights_limit(cap_fd_wo, &r_ws));
  int cap_fd_rw = dup(i_fd);
  EXPECT_OK(cap_fd_rw);
  EXPECT_OK(cap_rights_limit(cap_fd_rw, &r_rws));
  int cap_fd_all = dup(i_fd);
  EXPECT_OK(cap_fd_all);
  EXPECT_OK(cap_rights_limit(cap_fd_all, &r_rwsnotify));

  int fd = open(TmpFile("cap_inotify"), O_CREAT|O_RDWR, 0644);
  EXPECT_NOTCAPABLE(inotify_add_watch(cap_fd_rw, TmpFile("cap_inotify"), IN_ACCESS|IN_MODIFY));
  int wd = inotify_add_watch(i_fd, TmpFile("cap_inotify"), IN_ACCESS|IN_MODIFY);
  EXPECT_OK(wd);

  unsigned char buffer[] = {1, 2, 3, 4};
  EXPECT_OK(write(fd, buffer, sizeof(buffer)));

  struct inotify_event iev;
  memset(&iev, 0, sizeof(iev));
  EXPECT_NOTCAPABLE(read(cap_fd_wo, &iev, sizeof(iev)));
  int rc = read(cap_fd_ro, &iev, sizeof(iev));
  EXPECT_OK(rc);
  EXPECT_EQ((int)sizeof(iev), rc);
  EXPECT_EQ(wd, iev.wd);

  EXPECT_NOTCAPABLE(inotify_rm_watch(cap_fd_wo, wd));
  EXPECT_OK(inotify_rm_watch(cap_fd_all, wd));

  close(fd);
  close(cap_fd_all);
  close(cap_fd_rw);
  close(cap_fd_wo);
  close(cap_fd_ro);
  close(i_fd);
  unlink(TmpFile("cap_inotify"));
}

TEST(Linux, ArchChangeIfAvailable) {
  const char* prog_candidates[] = {"./mini-me.32", "./mini-me.x32", "./mini-me.64"};
  const char* progs[] = {NULL, NULL, NULL};
  char* argv_pass[] = {(char*)"to-come", (char*)"--capmode", NULL};
  char* null_envp[] = {NULL};
  int fds[3];
  int count = 0;

  for (int ii = 0; ii < 3; ii++) {
    fds[count] = open(prog_candidates[ii], O_RDONLY);
    if (fds[count] >= 0) {
      progs[count] = prog_candidates[ii];
      count++;
    }
  }
  if (count == 0) {
    GTEST_SKIP() << "no different-architecture programs available";
  }

  for (int ii = 0; ii < count; ii++) {
    // Fork-and-exec a binary of this architecture.
    pid_t child = fork();
    if (child == 0) {
      EXPECT_OK(cap_enter());  // Enter capability mode
      if (verbose) fprintf(stderr, "[%d] call fexecve(%s, %s)\n",
                           getpid_(), progs[ii], argv_pass[1]);
      argv_pass[0] = (char *)progs[ii];
      int rc = fexecve_(fds[ii], argv_pass, null_envp);
      fprintf(stderr, "fexecve(%s) returned %d errno %d\n", progs[ii], rc, errno);
      exit(99);  // Should not reach here.
    }
    int status;
    EXPECT_EQ(child, waitpid(child, &status, 0));
    int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    EXPECT_EQ(0, rc);
    close(fds[ii]);
  }
}

FORK_TEST(Linux, NamespaceIfRoot) {
  GTEST_SKIP_IF_NOT_ROOT();
  pid_t me = getpid_();

  // Create a new UTS namespace.
  EXPECT_OK(unshare(CLONE_NEWUTS));
  // Open an FD to its symlink.
  char buffer[256];
  sprintf(buffer, "/proc/%d/ns/uts", me);
  int ns_fd = open(buffer, O_RDONLY);

  cap_rights_t r_rwlstat;
  cap_rights_init(&r_rwlstat, CAP_READ, CAP_WRITE, CAP_LOOKUP, CAP_FSTAT);
  cap_rights_t r_rwlstatns;
  cap_rights_init(&r_rwlstatns, CAP_READ, CAP_WRITE, CAP_LOOKUP, CAP_FSTAT, CAP_SETNS);

  int cap_fd = dup(ns_fd);
  EXPECT_OK(cap_fd);
  EXPECT_OK(cap_rights_limit(cap_fd, &r_rwlstat));
  int cap_fd_setns = dup(ns_fd);
  EXPECT_OK(cap_fd_setns);
  EXPECT_OK(cap_rights_limit(cap_fd_setns, &r_rwlstatns));
  EXPECT_NOTCAPABLE(setns(cap_fd, CLONE_NEWUTS));
  EXPECT_OK(setns(cap_fd_setns, CLONE_NEWUTS));

  EXPECT_OK(cap_enter());  // Enter capability mode.

  // No setns(2) but unshare(2) is allowed.
  EXPECT_CAPMODE(setns(ns_fd, CLONE_NEWUTS));
  EXPECT_OK(unshare(CLONE_NEWUTS));
}

static void SendFD(int fd, int over) {
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
  mh.msg_controllen = CMSG_LEN(sizeof(int));
  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&mh);
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  *(int *)CMSG_DATA(cmptr) = fd;
  buffer1[0] = 0;
  iov[0].iov_len = 1;
  int rc = sendmsg(over, &mh, 0);
  EXPECT_OK(rc);
}

static int ReceiveFD(int over) {
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
  int rc = recvmsg(over, &mh, 0);
  EXPECT_OK(rc);
  EXPECT_LE(CMSG_LEN(sizeof(int)), mh.msg_controllen);
  struct cmsghdr *cmptr = CMSG_FIRSTHDR(&mh);
  int fd = *(int*)CMSG_DATA(cmptr);
  EXPECT_EQ(CMSG_LEN(sizeof(int)), cmptr->cmsg_len);
  cmptr = CMSG_NXTHDR(&mh, cmptr);
  EXPECT_TRUE(cmptr == NULL);
  return fd;
}

static int shared_pd = -1;
static int shared_sock_fds[2];

static int ChildFunc(void *arg) {
  // This function is running in a new PID namespace, and so is pid 1.
  if (verbose) fprintf(stderr, "    ChildFunc: pid=%d, ppid=%d\n", getpid_(), getppid());
  EXPECT_EQ(1, getpid_());
  EXPECT_EQ(0, getppid());

  // The shared process descriptor is outside our namespace, so we cannot
  // get its pid.
  if (verbose) fprintf(stderr, "    ChildFunc: shared_pd=%d\n", shared_pd);
  pid_t shared_child = -1;
  EXPECT_OK(pdgetpid(shared_pd, &shared_child));
  if (verbose) fprintf(stderr, "    ChildFunc: corresponding pid=%d\n", shared_child);
  EXPECT_EQ(0, shared_child);

  // But we can pdkill() it even so.
  if (verbose) fprintf(stderr, "    ChildFunc: call pdkill(pd=%d)\n", shared_pd);
  EXPECT_OK(pdkill(shared_pd, SIGINT));

  int pd;
  pid_t child = pdfork(&pd, 0);
  EXPECT_OK(child);
  if (child == 0) {
    // Child: expect pid 2.
    if (verbose) fprintf(stderr, "      child of ChildFunc: pid=%d, ppid=%d\n", getpid_(), getppid());
    EXPECT_EQ(2, getpid_());
    EXPECT_EQ(1, getppid());
    while (true) {
      if (verbose) fprintf(stderr, "      child of ChildFunc: \"I aten't dead\"\n");
      sleep(1);
    }
    exit(0);
  }
  EXPECT_EQ(2, child);
  EXPECT_PID_ALIVE(child);
  if (verbose) fprintf(stderr, "    ChildFunc: pdfork() -> pd=%d, corresponding pid=%d state='%c'\n",
                       pd, child, ProcessState(child));

  pid_t pid;
  EXPECT_OK(pdgetpid(pd, &pid));
  EXPECT_EQ(child, pid);

  sleep(2);

  // Send the process descriptor over UNIX domain socket back to parent.
  SendFD(pd, shared_sock_fds[1]);

  // Wait for death of (grand)child, killed by our parent.
  if (verbose) fprintf(stderr, "    ChildFunc: wait on pid=%d\n", child);
  int status;
  EXPECT_EQ(child, wait4(child, &status, __WALL, NULL));

  if (verbose) fprintf(stderr, "    ChildFunc: return 0\n");
  return 0;
}

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

// TODO(drysdale): fork into a user namespace first so GTEST_SKIP_IF_NOT_ROOT can be removed.
TEST(Linux, PidNamespacePdForkIfRoot) {
  GTEST_SKIP_IF_NOT_ROOT();
  // Pass process descriptors in both directions across a PID namespace boundary.
  // pdfork() off a child before we start, holding its process descriptor in a global
  // variable that's accessible to children.
  pid_t firstborn = pdfork(&shared_pd, 0);
  EXPECT_OK(firstborn);
  if (firstborn == 0) {
    while (true) {
      if (verbose) fprintf(stderr, "  Firstborn: \"I aten't dead\"\n");
      sleep(1);
    }
    exit(0);
  }
  EXPECT_PID_ALIVE(firstborn);
  if (verbose) fprintf(stderr, "Parent: pre-pdfork()ed pd=%d, pid=%d state='%c'\n",
                       shared_pd, firstborn, ProcessState(firstborn));
  sleep(2);

  // Prepare sockets to communicate with child process.
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, shared_sock_fds));

  // Clone into a child process with a new pid namespace.
  pid_t child = clone(ChildFunc, child_stack + STACK_SIZE,
                      CLONE_FILES|CLONE_NEWPID|SIGCHLD, NULL);
  EXPECT_OK(child);
  EXPECT_PID_ALIVE(child);
  if (verbose) fprintf(stderr, "Parent: child is %d state='%c'\n", child, ProcessState(child));

  // Ensure the child runs.  First thing it does is to kill our firstborn, using shared_pd.
  sleep(1);
  EXPECT_PID_DEAD(firstborn);

  // But we can still retrieve firstborn's PID, as it's not been reaped yet.
  pid_t child0;
  EXPECT_OK(pdgetpid(shared_pd, &child0));
  EXPECT_EQ(firstborn, child0);
  if (verbose) fprintf(stderr, "Parent: check on firstborn: pdgetpid(pd=%d) -> child=%d state='%c'\n",
                       shared_pd, child0, ProcessState(child0));

  // Now reap it.
  int status;
  EXPECT_EQ(firstborn, waitpid(firstborn, &status, __WALL));

  // Get the process descriptor of the child-of-child via socket transfer.
  int grandchild_pd = ReceiveFD(shared_sock_fds[0]);

  // Our notion of the pid associated with the grandchild is in the main PID namespace.
  pid_t grandchild;
  EXPECT_OK(pdgetpid(grandchild_pd, &grandchild));
  EXPECT_NE(2, grandchild);
  if (verbose) fprintf(stderr, "Parent: pre-pdkill:  pdgetpid(grandchild_pd=%d) -> grandchild=%d state='%c'\n",
                       grandchild_pd, grandchild, ProcessState(grandchild));
  EXPECT_PID_ALIVE(grandchild);

  // Kill the grandchild via the process descriptor.
  EXPECT_OK(pdkill(grandchild_pd, SIGINT));
  usleep(10000);
  if (verbose) fprintf(stderr, "Parent: post-pdkill: pdgetpid(grandchild_pd=%d) -> grandchild=%d state='%c'\n",
                       grandchild_pd, grandchild, ProcessState(grandchild));
  EXPECT_PID_DEAD(grandchild);

  sleep(2);

  // Wait for the child.
  EXPECT_EQ(child, waitpid(child, &status, WNOHANG));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(shared_sock_fds[0]);
  close(shared_sock_fds[1]);
  close(shared_pd);
  close(grandchild_pd);
}

int NSInit(void *data) {
  // This function is running in a new PID namespace, and so is pid 1.
  if (verbose) fprintf(stderr, "  NSInit: pid=%d, ppid=%d\n", getpid_(), getppid());
  EXPECT_EQ(1, getpid_());
  EXPECT_EQ(0, getppid());

  int pd;
  pid_t child = pdfork(&pd, 0);
  EXPECT_OK(child);
  if (child == 0) {
    // Child: loop forever until terminated.
    if (verbose) fprintf(stderr, "    child of NSInit: pid=%d, ppid=%d\n", getpid_(), getppid());
    while (true) {
      if (verbose) fprintf(stderr, "    child of NSInit: \"I aten't dead\"\n");
      usleep(100000);
    }
    exit(0);
  }
  EXPECT_EQ(2, child);
  EXPECT_PID_ALIVE(child);
  if (verbose) fprintf(stderr, "  NSInit: pdfork() -> pd=%d, corresponding pid=%d state='%c'\n",
                       pd, child, ProcessState(child));
  sleep(1);

  // Send the process descriptor over UNIX domain socket back to parent.
  SendFD(pd, shared_sock_fds[1]);
  close(pd);

  // Wait for a byte back in the other direction.
  int value;
  if (verbose) fprintf(stderr, "  NSInit: block waiting for value\n");
  read(shared_sock_fds[1], &value, sizeof(value));

  if (verbose) fprintf(stderr, "  NSInit: return 0\n");
  return 0;
}

TEST(Linux, DeadNSInitIfRoot) {
  GTEST_SKIP_IF_NOT_ROOT();

  // Prepare sockets to communicate with child process.
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, shared_sock_fds));

  // Clone into a child process with a new pid namespace.
  pid_t child = clone(NSInit, child_stack + STACK_SIZE,
                      CLONE_FILES|CLONE_NEWPID|SIGCHLD, NULL);
  usleep(10000);
  EXPECT_OK(child);
  EXPECT_PID_ALIVE(child);
  if (verbose) fprintf(stderr, "Parent: child is %d state='%c'\n", child, ProcessState(child));

  // Get the process descriptor of the child-of-child via socket transfer.
  int grandchild_pd = ReceiveFD(shared_sock_fds[0]);
  pid_t grandchild;
  EXPECT_OK(pdgetpid(grandchild_pd, &grandchild));
  if (verbose) fprintf(stderr, "Parent: grandchild is %d state='%c'\n", grandchild, ProcessState(grandchild));

  // Send an int to the child to trigger its termination.  Grandchild should also
  // go, as its init process is gone.
  int zero = 0;
  if (verbose) fprintf(stderr, "Parent: write 0 to pipe\n");
  write(shared_sock_fds[0], &zero, sizeof(zero));
  EXPECT_PID_ZOMBIE(child);
  EXPECT_PID_GONE(grandchild);

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, WNOHANG));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);
  EXPECT_PID_GONE(child);

  close(shared_sock_fds[0]);
  close(shared_sock_fds[1]);
  close(grandchild_pd);

  if (verbose) {
    fprintf(stderr, "Parent: child %d in state='%c'\n", child, ProcessState(child));
    fprintf(stderr, "Parent: grandchild %d in state='%c'\n", grandchild, ProcessState(grandchild));
  }
}

TEST(Linux, DeadNSInit2IfRoot) {
  GTEST_SKIP_IF_NOT_ROOT();

  // Prepare sockets to communicate with child process.
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, shared_sock_fds));

  // Clone into a child process with a new pid namespace.
  pid_t child = clone(NSInit, child_stack + STACK_SIZE,
                      CLONE_FILES|CLONE_NEWPID|SIGCHLD, NULL);
  usleep(10000);
  EXPECT_OK(child);
  EXPECT_PID_ALIVE(child);
  if (verbose) fprintf(stderr, "Parent: child is %d state='%c'\n", child, ProcessState(child));

  // Get the process descriptor of the child-of-child via socket transfer.
  int grandchild_pd = ReceiveFD(shared_sock_fds[0]);
  pid_t grandchild;
  EXPECT_OK(pdgetpid(grandchild_pd, &grandchild));
  if (verbose) fprintf(stderr, "Parent: grandchild is %d state='%c'\n", grandchild, ProcessState(grandchild));

  // Kill the grandchild
  EXPECT_OK(pdkill(grandchild_pd, SIGINT));
  usleep(10000);
  EXPECT_PID_ZOMBIE(grandchild);
  // Close the process descriptor, so there are now no procdesc references to grandchild.
  close(grandchild_pd);

  // Send an int to the child to trigger its termination.  Grandchild should also
  // go, as its init process is gone.
  int zero = 0;
  if (verbose) fprintf(stderr, "Parent: write 0 to pipe\n");
  write(shared_sock_fds[0], &zero, sizeof(zero));
  EXPECT_PID_ZOMBIE(child);
  EXPECT_PID_GONE(grandchild);

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, WNOHANG));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(shared_sock_fds[0]);
  close(shared_sock_fds[1]);

  if (verbose) {
    fprintf(stderr, "Parent: child %d in state='%c'\n", child, ProcessState(child));
    fprintf(stderr, "Parent: grandchild %d in state='%c'\n", grandchild, ProcessState(grandchild));
  }
}

#ifdef __x86_64__
FORK_TEST(Linux, CheckHighWord) {
  EXPECT_OK(cap_enter());  // Enter capability mode.

  int rc = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(1, rc);  // no_new_privs = 1

  // Set some of the high 32-bits of argument zero.
  uint64_t big_cmd = PR_GET_NO_NEW_PRIVS | 0x100000000LL;
  EXPECT_CAPMODE(syscall(__NR_prctl, big_cmd, 0, 0, 0, 0));
}
#endif

FORK_TEST(Linux, PrctlOpenatBeneath) {
  // Set no_new_privs = 1
  EXPECT_OK(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
  int rc = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(1, rc);  // no_new_privs = 1

  // Set openat-beneath mode
  EXPECT_OK(prctl(PR_SET_OPENAT_BENEATH, 1, 0, 0, 0));
  rc = prctl(PR_GET_OPENAT_BENEATH, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(1, rc);  // openat_beneath = 1

  // Clear openat-beneath mode
  EXPECT_OK(prctl(PR_SET_OPENAT_BENEATH, 0, 0, 0, 0));
  rc = prctl(PR_GET_OPENAT_BENEATH, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(0, rc);  // openat_beneath = 0

  EXPECT_OK(cap_enter());  // Enter capability mode

  // Expect to be in openat_beneath mode
  rc = prctl(PR_GET_OPENAT_BENEATH, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(1, rc);  // openat_beneath = 1

  // Expect this to be immutable.
  EXPECT_CAPMODE(prctl(PR_SET_OPENAT_BENEATH, 0, 0, 0, 0));
  rc = prctl(PR_GET_OPENAT_BENEATH, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(1, rc);  // openat_beneath = 1

}

FORK_TEST(Linux, NoNewPrivs) {
  if (getuid() == 0) {
    // If root, drop CAP_SYS_ADMIN POSIX.1e capability.
    struct __user_cap_header_struct hdr;
    hdr.version = _LINUX_CAPABILITY_VERSION_3;
    hdr.pid = getpid_();
    struct __user_cap_data_struct data[3];
    EXPECT_OK(capget(&hdr, &data[0]));
    data[0].effective &= ~(1 << CAP_SYS_ADMIN);
    data[0].permitted &= ~(1 << CAP_SYS_ADMIN);
    data[0].inheritable &= ~(1 << CAP_SYS_ADMIN);
    EXPECT_OK(capset(&hdr, &data[0]));
  }
  int rc = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(0, rc);  // no_new_privs == 0

  // Can't enter seccomp-bpf mode with no_new_privs == 0
  struct sock_filter filter[] = {
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)
  };
  struct sock_fprog bpf;
  bpf.len = (sizeof(filter) / sizeof(filter[0]));
  bpf.filter = filter;
  rc = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &bpf, 0, 0);
  EXPECT_EQ(-1, rc);
  EXPECT_EQ(EACCES, errno);

  // Set no_new_privs = 1
  EXPECT_OK(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
  rc = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
  EXPECT_OK(rc);
  EXPECT_EQ(1, rc);  // no_new_privs = 1

  // Can now turn on seccomp mode
  EXPECT_OK(prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &bpf, 0, 0));
}

/* Macros for BPF generation */
#define BPF_RETURN_ERRNO(err) \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ERRNO | (err & 0xFFFF))
#define BPF_KILL_PROCESS \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL)
#define BPF_ALLOW \
  BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)
#define EXAMINE_SYSCALL \
  BPF_STMT(BPF_LD+BPF_W+BPF_ABS, offsetof(struct seccomp_data, nr))
#define ALLOW_SYSCALL(name) \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
  BPF_ALLOW
#define KILL_SYSCALL(name) \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
  BPF_KILL_PROCESS
#define FAIL_SYSCALL(name, err) \
  BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
  BPF_RETURN_ERRNO(err)

TEST(Linux, CapModeWithBPF) {
  pid_t child = fork();
  EXPECT_OK(child);
  if (child == 0) {
    int fd = open(TmpFile("cap_bpf_capmode"), O_CREAT|O_RDWR, 0644);
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_FSYNC);
    EXPECT_OK(cap_rights_limit(fd, &rights));

    struct sock_filter filter[] = { EXAMINE_SYSCALL,
                                    FAIL_SYSCALL(fchmod, ENOMEM),
                                    FAIL_SYSCALL(fstat, ENOEXEC),
                                    ALLOW_SYSCALL(close),
                                    KILL_SYSCALL(fsync),
                                    BPF_ALLOW };
    struct sock_fprog bpf = {.len = (sizeof(filter) / sizeof(filter[0])),
                             .filter = filter};
    // Set up seccomp-bpf first.
    EXPECT_OK(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
    EXPECT_OK(prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &bpf, 0, 0));

    EXPECT_OK(cap_enter());  // Enter capability mode.

    // fchmod is allowed by Capsicum, but failed by BPF.
    EXPECT_SYSCALL_FAIL(ENOMEM, fchmod(fd, 0644));
    // open is allowed by BPF, but failed by Capsicum
    EXPECT_SYSCALL_FAIL(ECAPMODE, open(TmpFile("cap_bpf_capmode"), O_RDONLY));
    // fstat is failed by both BPF and Capsicum; tie-break is on errno
    struct stat buf;
    EXPECT_SYSCALL_FAIL(ENOEXEC, fstat(fd, &buf));
    // fsync is allowed by Capsicum, but BPF's SIGSYS generation take precedence
    fsync(fd);  // terminate with unhandled SIGSYS
    exit(0);
  }
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  EXPECT_TRUE(WIFSIGNALED(status));
  EXPECT_EQ(SIGSYS, WTERMSIG(status));
  unlink(TmpFile("cap_bpf_capmode"));
}

TEST(Linux, AIO) {
  int fd = open(TmpFile("cap_aio"), O_CREAT|O_RDWR, 0644);
  EXPECT_OK(fd);

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);
  cap_rights_t r_rwssync;
  cap_rights_init(&r_rwssync, CAP_READ, CAP_WRITE, CAP_SEEK, CAP_FSYNC);

  int cap_ro = dup(fd);
  EXPECT_OK(cap_ro);
  EXPECT_OK(cap_rights_limit(cap_ro, &r_rs));
  EXPECT_OK(cap_ro);
  int cap_wo = dup(fd);
  EXPECT_OK(cap_wo);
  EXPECT_OK(cap_rights_limit(cap_wo, &r_ws));
  EXPECT_OK(cap_wo);
  int cap_all = dup(fd);
  EXPECT_OK(cap_all);
  EXPECT_OK(cap_rights_limit(cap_all, &r_rwssync));
  EXPECT_OK(cap_all);

  // Linux: io_setup, io_submit, io_getevents, io_cancel, io_destroy
  aio_context_t ctx = 0;
  EXPECT_OK(syscall(__NR_io_setup, 10, &ctx));

  unsigned char buffer[32] = {1, 2, 3, 4};
  struct iocb req;
  memset(&req, 0, sizeof(req));
  req.aio_reqprio = 0;
  req.aio_fildes = fd;
  uintptr_t bufaddr = (uintptr_t)buffer;
  req.aio_buf = (__u64)bufaddr;
  req.aio_nbytes = 4;
  req.aio_offset = 0;
  struct iocb* reqs[1] = {&req};

  // Write operation
  req.aio_lio_opcode = IOCB_CMD_PWRITE;
  req.aio_fildes = cap_ro;
  EXPECT_NOTCAPABLE(syscall(__NR_io_submit, ctx, 1,  reqs));
  req.aio_fildes = cap_wo;
  EXPECT_OK(syscall(__NR_io_submit, ctx, 1,  reqs));

  // Sync operation
  req.aio_lio_opcode = IOCB_CMD_FSYNC;
  EXPECT_NOTCAPABLE(syscall(__NR_io_submit, ctx, 1, reqs));
  req.aio_lio_opcode = IOCB_CMD_FDSYNC;
  EXPECT_NOTCAPABLE(syscall(__NR_io_submit, ctx, 1, reqs));
  // Even with CAP_FSYNC, turns out fsync/fdsync aren't implemented
  req.aio_fildes = cap_all;
  EXPECT_FAIL_NOT_NOTCAPABLE(syscall(__NR_io_submit, ctx, 1, reqs));
  req.aio_lio_opcode = IOCB_CMD_FSYNC;
  EXPECT_FAIL_NOT_NOTCAPABLE(syscall(__NR_io_submit, ctx, 1, reqs));

  // Read operation
  req.aio_lio_opcode = IOCB_CMD_PREAD;
  req.aio_fildes = cap_wo;
  EXPECT_NOTCAPABLE(syscall(__NR_io_submit, ctx, 1,  reqs));
  req.aio_fildes = cap_ro;
  EXPECT_OK(syscall(__NR_io_submit, ctx, 1,  reqs));

  EXPECT_OK(syscall(__NR_io_destroy, ctx));

  close(cap_all);
  close(cap_wo);
  close(cap_ro);
  close(fd);
  unlink(TmpFile("cap_aio"));
}

#ifndef KCMP_FILE
#define KCMP_FILE 0
#endif
TEST(Linux, KcmpIfAvailable) {
  // This requires CONFIG_CHECKPOINT_RESTORE in kernel config.
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);
  pid_t parent = getpid_();

  errno = 0;
  int rc = syscall(__NR_kcmp, parent, parent, KCMP_FILE, fd, fd);
  if (rc == -1 && errno == ENOSYS) {
    GTEST_SKIP() << "kcmp(2) gives -ENOSYS";
  }

  pid_t child = fork();
  if (child == 0) {
    // Child: limit rights on FD.
    child = getpid_();
    EXPECT_OK(syscall(__NR_kcmp, parent, child, KCMP_FILE, fd, fd));
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_READ, CAP_WRITE);
    EXPECT_OK(cap_rights_limit(fd, &rights));
    // A capability wrapping a normal FD is different (from a kcmp(2) perspective)
    // than the original file.
    EXPECT_NE(0, syscall(__NR_kcmp, parent, child, KCMP_FILE, fd, fd));
    exit(HasFailure());
  }
  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(fd);
}

TEST(Linux, ProcFS) {
  cap_rights_t rights;
  cap_rights_init(&rights, CAP_READ, CAP_SEEK);
  int fd = open("/etc/passwd", O_RDONLY);
  EXPECT_OK(fd);
  lseek(fd, 4, SEEK_SET);
  int cap = dup(fd);
  EXPECT_OK(cap);
  EXPECT_OK(cap_rights_limit(cap, &rights));
  pid_t me = getpid_();

  char buffer[1024];
  sprintf(buffer, "/proc/%d/fdinfo/%d", me, cap);
  int procfd = open(buffer, O_RDONLY);
  EXPECT_OK(procfd) << " failed to open " << buffer;
  if (procfd < 0) return;
  int proccap = dup(procfd);
  EXPECT_OK(proccap);
  EXPECT_OK(cap_rights_limit(proccap, &rights));

  EXPECT_OK(read(proccap, buffer, sizeof(buffer)));
  // The fdinfo should include the file pos of the underlying file
  EXPECT_NE((char*)NULL, strstr(buffer, "pos:\t4"));
  // ...and the rights of the Capsicum capability.
  EXPECT_NE((char*)NULL, strstr(buffer, "rights:\t0x"));

  close(procfd);
  close(proccap);
  close(cap);
  close(fd);
}

FORK_TEST(Linux, ProcessClocks) {
  pid_t self = getpid_();
  pid_t child = fork();
  EXPECT_OK(child);
  if (child == 0) {
    child = getpid_();
    usleep(100000);
    exit(0);
  }

  EXPECT_OK(cap_enter());  // Enter capability mode.

  // Nefariously build a clock ID for the child's CPU time.
  // This relies on knowledge of the internal layout of clock IDs.
  clockid_t child_clock;
  child_clock = ((~child) << 3) | 0x0;
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));

  // TODO(drysdale): Should not be possible to retrieve info about a
  // different process, as the PID global namespace should be locked
  // down.
  EXPECT_OK(clock_gettime(child_clock, &ts));
  if (verbose) fprintf(stderr, "[parent: %d] clock_gettime(child=%d->0x%08x) is %ld.%09ld \n",
                       self, child, child_clock, (long)ts.tv_sec, (long)ts.tv_nsec);

  child_clock = ((~1) << 3) | 0x0;
  memset(&ts, 0, sizeof(ts));
  EXPECT_OK(clock_gettime(child_clock, &ts));
  if (verbose) fprintf(stderr, "[parent: %d] clock_gettime(init=1->0x%08x) is %ld.%09ld \n",
                       self, child_clock, (long)ts.tv_sec, (long)ts.tv_nsec);

  // Orphan the child.
}

TEST(Linux, SetLease) {
  int fd_all = open(TmpFile("cap_lease"), O_CREAT|O_RDWR, 0644);
  EXPECT_OK(fd_all);
  int fd_rw = dup(fd_all);
  EXPECT_OK(fd_rw);

  cap_rights_t r_all;
  cap_rights_init(&r_all, CAP_READ, CAP_WRITE, CAP_FLOCK, CAP_FSIGNAL);
  EXPECT_OK(cap_rights_limit(fd_all, &r_all));

  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);
  EXPECT_OK(cap_rights_limit(fd_rw, &r_rw));

  EXPECT_NOTCAPABLE(fcntl(fd_rw, F_SETLEASE, F_WRLCK));
  EXPECT_NOTCAPABLE(fcntl(fd_rw, F_GETLEASE));

  if (!tmpdir_on_tmpfs) {  // tmpfs doesn't support leases
    EXPECT_OK(fcntl(fd_all, F_SETLEASE, F_WRLCK));
    EXPECT_EQ(F_WRLCK, fcntl(fd_all, F_GETLEASE));

    EXPECT_OK(fcntl(fd_all, F_SETLEASE, F_UNLCK, 0));
    EXPECT_EQ(F_UNLCK, fcntl(fd_all, F_GETLEASE));
  }
  close(fd_all);
  close(fd_rw);
  unlink(TmpFile("cap_lease"));
}

TEST(Linux, InvalidRightsSyscall) {
  int fd = open(TmpFile("cap_invalid_rights"), O_RDONLY|O_CREAT, 0644);
  EXPECT_OK(fd);

  cap_rights_t rights;
  cap_rights_init(&rights, CAP_READ, CAP_WRITE, CAP_FCHMOD, CAP_FSTAT);

  // Use the raw syscall throughout.
  EXPECT_EQ(0, syscall(__NR_cap_rights_limit, fd, &rights, 0, 0, NULL, 0));

  // Directly access the syscall, and find all unseemly manner of use for it.
  //  - Invalid flags
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, 0, 0, NULL, 1));
  EXPECT_EQ(EINVAL, errno);
  //  - Specify an fcntl subright, but no CAP_FCNTL set
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, CAP_FCNTL_GETFL, 0, NULL, 0));
  EXPECT_EQ(EINVAL, errno);
  //  - Specify an ioctl subright, but no CAP_IOCTL set
  unsigned int ioctl1 = 1;
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, 0, 1, &ioctl1, 0));
  EXPECT_EQ(EINVAL, errno);
  //  - N ioctls, but null pointer passed
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, 0, 1, NULL, 0));
  EXPECT_EQ(EINVAL, errno);
  //  - Invalid nioctls
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, 0, -2, NULL, 0));
  EXPECT_EQ(EINVAL, errno);
  //  - Null primary rights
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, NULL, 0, 0, NULL, 0));
  EXPECT_EQ(EFAULT, errno);
  //  - Invalid index bitmask
  rights.cr_rights[0] |= 3ULL << 57;
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, 0, 0, NULL, 0));
  EXPECT_EQ(EINVAL, errno);
  //  - Invalid version
  rights.cr_rights[0] |= 2ULL << 62;
  EXPECT_EQ(-1, syscall(__NR_cap_rights_limit, fd, &rights, 0, 0, NULL, 0));
  EXPECT_EQ(EINVAL, errno);

  close(fd);
  unlink(TmpFile("cap_invalid_rights"));
}

FORK_TEST_ON(Linux, OpenByHandleAtIfRoot, TmpFile("cap_openbyhandle_testfile")) {
  GTEST_SKIP_IF_NOT_ROOT();
  int dir = open(tmpdir.c_str(), O_RDONLY);
  EXPECT_OK(dir);
  int fd = openat(dir, "cap_openbyhandle_testfile", O_RDWR|O_CREAT, 0644);
  EXPECT_OK(fd);
  const char* message = "Saved text";
  EXPECT_OK(write(fd, message, strlen(message)));
  close(fd);

  struct file_handle* fhandle = (struct file_handle*)malloc(sizeof(struct file_handle) + MAX_HANDLE_SZ);
  fhandle->handle_bytes = MAX_HANDLE_SZ;
  int mount_id;
  EXPECT_OK(name_to_handle_at(dir, "cap_openbyhandle_testfile", fhandle,  &mount_id, 0));

  fd = open_by_handle_at(dir, fhandle, O_RDONLY);
  EXPECT_OK(fd);
  char buffer[200];
  ssize_t len = read(fd, buffer, 199);
  EXPECT_OK(len);
  EXPECT_EQ(std::string(message), std::string(buffer, len));
  close(fd);

  // Cannot issue open_by_handle_at after entering capability mode.
  cap_enter();
  EXPECT_CAPMODE(open_by_handle_at(dir, fhandle, O_RDONLY));

  close(dir);
}

int getrandom_(void *buf, size_t buflen, unsigned int flags) {
#ifdef __NR_getrandom
  return syscall(__NR_getrandom, buf, buflen, flags);
#else
  errno = ENOSYS;
  return -1;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
#include <linux/random.h>  // Requires 3.17 kernel
FORK_TEST(Linux, GetRandom) {
  EXPECT_OK(cap_enter());
  unsigned char buffer[1024];
  unsigned char buffer2[1024];
  EXPECT_OK(getrandom_(buffer, sizeof(buffer), GRND_NONBLOCK));
  EXPECT_OK(getrandom_(buffer2, sizeof(buffer2), GRND_NONBLOCK));
  EXPECT_NE(0, memcmp(buffer, buffer2, sizeof(buffer)));
}
#endif

int memfd_create_(const char *name, unsigned int flags) {
#ifdef __NR_memfd_create
  return syscall(__NR_memfd_create, name, flags);
#else
  errno = ENOSYS;
  return -1;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
#include <linux/memfd.h>  // Requires 3.17 kernel
TEST(Linux, MemFDDeathTestIfAvailable) {
  int memfd = memfd_create_("capsicum-test", MFD_ALLOW_SEALING);
  if (memfd == -1 && errno == ENOSYS) {
    GTEST_SKIP() << "memfd_create(2) gives -ENOSYS";
  }
  const int LEN = 16;
  EXPECT_OK(ftruncate(memfd, LEN));
  int memfd_ro = dup(memfd);
  int memfd_rw = dup(memfd);
  EXPECT_OK(memfd_ro);
  EXPECT_OK(memfd_rw);
  cap_rights_t rights;
  EXPECT_OK(cap_rights_limit(memfd_ro, cap_rights_init(&rights, CAP_MMAP_R, CAP_FSTAT)));
  EXPECT_OK(cap_rights_limit(memfd_rw, cap_rights_init(&rights, CAP_MMAP_RW, CAP_FCHMOD)));

  unsigned char *p_ro = (unsigned char *)mmap(NULL, LEN, PROT_READ, MAP_SHARED, memfd_ro, 0);
  EXPECT_NE((unsigned char *)MAP_FAILED, p_ro);
  unsigned char *p_rw = (unsigned char *)mmap(NULL, LEN, PROT_READ|PROT_WRITE, MAP_SHARED, memfd_rw, 0);
  EXPECT_NE((unsigned char *)MAP_FAILED, p_rw);
  EXPECT_EQ(MAP_FAILED,
            mmap(NULL, LEN, PROT_READ|PROT_WRITE, MAP_SHARED, memfd_ro, 0));

  *p_rw = 42;
  EXPECT_EQ(42, *p_ro);
  EXPECT_DEATH(*p_ro = 42, "");

#ifndef F_ADD_SEALS
  // Hack for when libc6 does not yet include the updated linux/fcntl.h from kernel 3.17
#define _F_LINUX_SPECIFIC_BASE F_SETLEASE
#define F_ADD_SEALS	(_F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS	(_F_LINUX_SPECIFIC_BASE + 10)
#define F_SEAL_SEAL	0x0001	/* prevent further seals from being set */
#define F_SEAL_SHRINK	0x0002	/* prevent file from shrinking */
#define F_SEAL_GROW	0x0004	/* prevent file from growing */
#define F_SEAL_WRITE	0x0008	/* prevent writes */
#endif

  // Reading the seal information requires CAP_FSTAT.
  int seals = fcntl(memfd, F_GET_SEALS);
  EXPECT_OK(seals);
  if (verbose) fprintf(stderr, "seals are %08x on base fd\n", seals);
  int seals_ro = fcntl(memfd_ro, F_GET_SEALS);
  EXPECT_EQ(seals, seals_ro);
  if (verbose) fprintf(stderr, "seals are %08x on read-only fd\n", seals_ro);
  int seals_rw = fcntl(memfd_rw, F_GET_SEALS);
  EXPECT_NOTCAPABLE(seals_rw);

  // Fail to seal as a writable mapping exists.
  EXPECT_EQ(-1, fcntl(memfd_rw, F_ADD_SEALS, F_SEAL_WRITE));
  EXPECT_EQ(EBUSY, errno);
  *p_rw = 42;

  // Seal the rw version; need to unmap first.
  munmap(p_rw, LEN);
  munmap(p_ro, LEN);
  EXPECT_OK(fcntl(memfd_rw, F_ADD_SEALS, F_SEAL_WRITE));

  seals = fcntl(memfd, F_GET_SEALS);
  EXPECT_OK(seals);
  if (verbose) fprintf(stderr, "seals are %08x on base fd\n", seals);
  seals_ro = fcntl(memfd_ro, F_GET_SEALS);
  EXPECT_EQ(seals, seals_ro);
  if (verbose) fprintf(stderr, "seals are %08x on read-only fd\n", seals_ro);

  // Remove the CAP_FCHMOD right, can no longer add seals.
  EXPECT_OK(cap_rights_limit(memfd_rw, cap_rights_init(&rights, CAP_MMAP_RW)));
  EXPECT_NOTCAPABLE(fcntl(memfd_rw, F_ADD_SEALS, F_SEAL_WRITE));

  close(memfd);
  close(memfd_ro);
  close(memfd_rw);
}
#endif

#else
void noop() {}
#endif
