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
