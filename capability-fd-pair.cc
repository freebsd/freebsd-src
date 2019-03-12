// Tests involving 2 capability file descriptors.
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

TEST(CapabilityPair, sendfile) {
  int in_fd = open(TmpFile("cap_sendfile_in"), O_CREAT|O_RDWR, 0644);
  EXPECT_OK(write(in_fd, "1234", 4));
  // Output fd for sendfile must be a stream socket in FreeBSD.
  int sock_fds[2];
  EXPECT_OK(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fds));

  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);

  int cap_in_ro = dup(in_fd);
  EXPECT_OK(cap_in_ro);
  EXPECT_OK(cap_rights_limit(cap_in_ro, &r_rs));
  int cap_in_wo = dup(in_fd);
  EXPECT_OK(cap_in_wo);
  EXPECT_OK(cap_rights_limit(cap_in_wo, &r_ws));
  int cap_out_ro = dup(sock_fds[0]);
  EXPECT_OK(cap_out_ro);
  EXPECT_OK(cap_rights_limit(cap_out_ro, &r_rs));
  int cap_out_wo = dup(sock_fds[0]);
  EXPECT_OK(cap_out_wo);
  EXPECT_OK(cap_rights_limit(cap_out_wo, &r_ws));

  off_t offset = 0;
  EXPECT_NOTCAPABLE(sendfile_(cap_out_ro, cap_in_ro, &offset, 4));
  EXPECT_NOTCAPABLE(sendfile_(cap_out_wo, cap_in_wo, &offset, 4));
  EXPECT_OK(sendfile_(cap_out_wo, cap_in_ro, &offset, 4));

  close(cap_in_ro);
  close(cap_in_wo);
  close(cap_out_ro);
  close(cap_out_wo);
  close(in_fd);
  close(sock_fds[0]);
  close(sock_fds[1]);
  unlink(TmpFile("cap_sendfile_in"));
}

#ifdef HAVE_TEE
TEST(CapabilityPair, tee) {
  int pipe1_fds[2];
  EXPECT_OK(pipe2(pipe1_fds, O_NONBLOCK));
  int pipe2_fds[2];
  EXPECT_OK(pipe2(pipe2_fds, O_NONBLOCK));

  // Put some data into pipe1.
  unsigned char buffer[4] = {1, 2, 3, 4};
  EXPECT_OK(write(pipe1_fds[1], buffer, 4));

  cap_rights_t r_ro;
  cap_rights_init(&r_ro, CAP_READ);
  cap_rights_t r_wo;
  cap_rights_init(&r_wo, CAP_WRITE);
  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);

  // Various attempts to tee into pipe2.
  int cap_in_wo = dup(pipe1_fds[0]);
  EXPECT_OK(cap_in_wo);
  EXPECT_OK(cap_rights_limit(cap_in_wo, &r_wo));
  int cap_in_rw = dup(pipe1_fds[0]);
  EXPECT_OK(cap_in_rw);
  EXPECT_OK(cap_rights_limit(cap_in_rw, &r_rw));
  int cap_out_ro = dup(pipe2_fds[1]);
  EXPECT_OK(cap_out_ro);
  EXPECT_OK(cap_rights_limit(cap_out_ro, &r_ro));
  int cap_out_rw = dup(pipe2_fds[1]);
  EXPECT_OK(cap_out_rw);
  EXPECT_OK(cap_rights_limit(cap_out_rw, &r_rw));

  EXPECT_NOTCAPABLE(tee(cap_in_wo, cap_out_rw, 4, SPLICE_F_NONBLOCK));
  EXPECT_NOTCAPABLE(tee(cap_in_rw, cap_out_ro, 4, SPLICE_F_NONBLOCK));
  EXPECT_OK(tee(cap_in_rw, cap_out_rw, 4, SPLICE_F_NONBLOCK));

  close(cap_in_wo);
  close(cap_in_rw);
  close(cap_out_ro);
  close(cap_out_rw);
  close(pipe1_fds[0]);
  close(pipe1_fds[1]);
  close(pipe2_fds[0]);
  close(pipe2_fds[1]);
}
#endif

#ifdef HAVE_SPLICE
TEST(CapabilityPair, splice) {
  int pipe1_fds[2];
  EXPECT_OK(pipe2(pipe1_fds, O_NONBLOCK));
  int pipe2_fds[2];
  EXPECT_OK(pipe2(pipe2_fds, O_NONBLOCK));

  // Put some data into pipe1.
  unsigned char buffer[4] = {1, 2, 3, 4};
  EXPECT_OK(write(pipe1_fds[1], buffer, 4));

  cap_rights_t r_ro;
  cap_rights_init(&r_ro, CAP_READ);
  cap_rights_t r_wo;
  cap_rights_init(&r_wo, CAP_WRITE);
  cap_rights_t r_rs;
  cap_rights_init(&r_rs, CAP_READ, CAP_SEEK);
  cap_rights_t r_ws;
  cap_rights_init(&r_ws, CAP_WRITE, CAP_SEEK);

  // Various attempts to splice.
  int cap_in_wo = dup(pipe1_fds[0]);
  EXPECT_OK(cap_in_wo);
  EXPECT_OK(cap_rights_limit(cap_in_wo, &r_wo));
  int cap_in_ro = dup(pipe1_fds[0]);
  EXPECT_OK(cap_in_ro);
  EXPECT_OK(cap_rights_limit(cap_in_ro, &r_ro));
  int cap_in_ro_seek = dup(pipe1_fds[0]);
  EXPECT_OK(cap_in_ro_seek);
  EXPECT_OK(cap_rights_limit(cap_in_ro_seek, &r_rs));
  int cap_out_wo = dup(pipe2_fds[1]);
  EXPECT_OK(cap_out_wo);
  EXPECT_OK(cap_rights_limit(cap_out_wo, &r_wo));
  int cap_out_ro = dup(pipe2_fds[1]);
  EXPECT_OK(cap_out_ro);
  EXPECT_OK(cap_rights_limit(cap_out_ro, &r_ro));
  int cap_out_wo_seek = dup(pipe2_fds[1]);
  EXPECT_OK(cap_out_wo_seek);
  EXPECT_OK(cap_rights_limit(cap_out_wo_seek, &r_ws));

  EXPECT_NOTCAPABLE(splice(cap_in_ro, NULL, cap_out_wo_seek, NULL, 4, SPLICE_F_NONBLOCK));
  EXPECT_NOTCAPABLE(splice(cap_in_wo, NULL, cap_out_wo_seek, NULL, 4, SPLICE_F_NONBLOCK));
  EXPECT_NOTCAPABLE(splice(cap_in_ro_seek, NULL, cap_out_ro, NULL, 4, SPLICE_F_NONBLOCK));
  EXPECT_NOTCAPABLE(splice(cap_in_ro_seek, NULL, cap_out_wo, NULL, 4, SPLICE_F_NONBLOCK));
  EXPECT_OK(splice(cap_in_ro_seek, NULL, cap_out_wo_seek, NULL, 4, SPLICE_F_NONBLOCK));

  close(cap_in_wo);
  close(cap_in_ro);
  close(cap_in_ro_seek);
  close(cap_out_wo);
  close(cap_out_ro);
  close(cap_out_wo_seek);
  close(pipe1_fds[0]);
  close(pipe1_fds[1]);
  close(pipe2_fds[0]);
  close(pipe2_fds[1]);
}
#endif

#ifdef HAVE_VMSPLICE
// Although it only involves a single file descriptor, test vmsplice(2) here too.
TEST(CapabilityPair, vmsplice) {
  int pipe_fds[2];
  EXPECT_OK(pipe2(pipe_fds, O_NONBLOCK));

  cap_rights_t r_ro;
  cap_rights_init(&r_ro, CAP_READ);
  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);

  int cap_ro = dup(pipe_fds[1]);
  EXPECT_OK(cap_ro);
  EXPECT_OK(cap_rights_limit(cap_ro, &r_ro));
  int cap_rw = dup(pipe_fds[1]);
  EXPECT_OK(cap_rw);
  EXPECT_OK(cap_rights_limit(cap_rw, &r_rw));

  unsigned char buffer[4] = {1, 2, 3, 4};
  struct iovec iov;
  memset(&iov, 0, sizeof(iov));
  iov.iov_base = buffer;
  iov.iov_len = sizeof(buffer);

  EXPECT_NOTCAPABLE(vmsplice(cap_ro, &iov, 1, SPLICE_F_NONBLOCK));
  EXPECT_OK(vmsplice(cap_rw, &iov, 1, SPLICE_F_NONBLOCK));

  close(cap_ro);
  close(cap_rw);
  close(pipe_fds[0]);
  close(pipe_fds[1]);
}
#endif
