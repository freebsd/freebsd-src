// Tests for socket functionality.
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <string>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

TEST(Socket, UnixDomain) {
  const char* socketName = TmpFile("capsicum-test.socket");
  unlink(socketName);
  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);
  cap_rights_t r_all;
  cap_rights_init(&r_all, CAP_READ, CAP_WRITE, CAP_SOCK_CLIENT, CAP_SOCK_SERVER);

  pid_t child = fork();
  if (child == 0) {
    // Child process: wait for server setup
    sleep(1);

    // Create sockets
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    EXPECT_OK(sock);
    if (sock < 0) return;

    int cap_sock_rw = dup(sock);
    EXPECT_OK(cap_sock_rw);
    EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
    int cap_sock_all = dup(sock);
    EXPECT_OK(cap_sock_all);
    EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
    EXPECT_OK(close(sock));

    // Connect socket
    struct sockaddr_un un;
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, socketName);
    socklen_t len = sizeof(un);
    EXPECT_NOTCAPABLE(connect_(cap_sock_rw, (struct sockaddr *)&un, len));
    EXPECT_OK(connect_(cap_sock_all, (struct sockaddr *)&un, len));

    exit(HasFailure());
  }

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  EXPECT_OK(sock);
  if (sock < 0) return;

  int cap_sock_rw = dup(sock);
  EXPECT_OK(cap_sock_rw);
  EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
  int cap_sock_all = dup(sock);
  EXPECT_OK(cap_sock_all);
  EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
  EXPECT_OK(close(sock));

  struct sockaddr_un un;
  memset(&un, 0, sizeof(un));
  un.sun_family = AF_UNIX;
  strcpy(un.sun_path, socketName);
  socklen_t len =  (sizeof(un) - sizeof(un.sun_path) + strlen(un.sun_path));

  // Can only bind the fully-capable socket.
  EXPECT_NOTCAPABLE(bind_(cap_sock_rw, (struct sockaddr *)&un, len));
  EXPECT_OK(bind_(cap_sock_all, (struct sockaddr *)&un, len));

  // Can only listen on the fully-capable socket.
  EXPECT_NOTCAPABLE(listen(cap_sock_rw, 3));
  EXPECT_OK(listen(cap_sock_all, 3));

  // Can only do socket operations on the fully-capable socket.
  len = sizeof(un);
  EXPECT_NOTCAPABLE(getsockname(cap_sock_rw, (struct sockaddr*)&un, &len));
  int value = 0;
  EXPECT_NOTCAPABLE(setsockopt(cap_sock_rw, SOL_SOCKET, SO_DEBUG, &value, sizeof(value)));
  len = sizeof(value);
  EXPECT_NOTCAPABLE(getsockopt(cap_sock_rw, SOL_SOCKET, SO_DEBUG, &value, &len));

  len = sizeof(un);
  memset(&un, 0, sizeof(un));
  EXPECT_OK(getsockname(cap_sock_all, (struct sockaddr*)&un, &len));
  EXPECT_EQ(AF_UNIX, un.sun_family);
  EXPECT_EQ(std::string(socketName), std::string(un.sun_path));
  value = 0;
  EXPECT_OK(setsockopt(cap_sock_all, SOL_SOCKET, SO_DEBUG, &value, sizeof(value)));
  len = sizeof(value);
  EXPECT_OK(getsockopt(cap_sock_all, SOL_SOCKET, SO_DEBUG, &value, &len));

  // Accept the incoming connection
  len = sizeof(un);
  memset(&un, 0, sizeof(un));
  EXPECT_NOTCAPABLE(accept(cap_sock_rw, (struct sockaddr *)&un, &len));
  int conn_fd = accept(cap_sock_all, (struct sockaddr *)&un, &len);
  EXPECT_OK(conn_fd);

#ifdef CAP_FROM_ACCEPT
  // New connection should also be a capability.
  cap_rights_t rights;
  cap_rights_init(&rights, 0);
  EXPECT_OK(cap_rights_get(conn_fd, &rights));
  EXPECT_RIGHTS_IN(&rights, &r_all);
#endif

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(conn_fd);
  close(cap_sock_rw);
  close(cap_sock_all);
  unlink(socketName);
}

TEST(Socket, TCP) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_OK(sock);
  if (sock < 0) return;

  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);
  cap_rights_t r_all;
  cap_rights_init(&r_all, CAP_READ, CAP_WRITE, CAP_SOCK_CLIENT, CAP_SOCK_SERVER);

  int cap_sock_rw = dup(sock);
  EXPECT_OK(cap_sock_rw);
  EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
  int cap_sock_all = dup(sock);
  EXPECT_OK(cap_sock_all);
  EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
  close(sock);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  socklen_t len = sizeof(addr);

  // Can only bind the fully-capable socket.
  EXPECT_NOTCAPABLE(bind_(cap_sock_rw, (struct sockaddr *)&addr, len));
  EXPECT_OK(bind_(cap_sock_all, (struct sockaddr *)&addr, len));

  getsockname(cap_sock_all, (struct sockaddr *)&addr, &len);
  int port = ntohs(addr.sin_port);

  // Now we know the port involved, fork off a child.
  pid_t child = fork();
  if (child == 0) {
    // Child process: wait for server setup
    sleep(1);

    // Create sockets
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_OK(sock);
    if (sock < 0) return;
    int cap_sock_rw = dup(sock);
    EXPECT_OK(cap_sock_rw);
    EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
    int cap_sock_all = dup(sock);
    EXPECT_OK(cap_sock_all);
    EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
    close(sock);

    // Connect socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);  // Pick unused port
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    socklen_t len = sizeof(addr);
    EXPECT_NOTCAPABLE(connect_(cap_sock_rw, (struct sockaddr *)&addr, len));
    EXPECT_OK(connect_(cap_sock_all, (struct sockaddr *)&addr, len));

    exit(HasFailure());
  }

  // Can only listen on the fully-capable socket.
  EXPECT_NOTCAPABLE(listen(cap_sock_rw, 3));
  EXPECT_OK(listen(cap_sock_all, 3));

  // Can only do socket operations on the fully-capable socket.
  len = sizeof(addr);
  EXPECT_NOTCAPABLE(getsockname(cap_sock_rw, (struct sockaddr*)&addr, &len));
  int value = 1;
  EXPECT_NOTCAPABLE(setsockopt(cap_sock_rw, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value)));
  len = sizeof(value);
  EXPECT_NOTCAPABLE(getsockopt(cap_sock_rw, SOL_SOCKET, SO_REUSEPORT, &value, &len));

  len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  EXPECT_OK(getsockname(cap_sock_all, (struct sockaddr*)&addr, &len));
  EXPECT_EQ(AF_INET, addr.sin_family);
  EXPECT_EQ(htons(port), addr.sin_port);
  value = 0;
  EXPECT_OK(setsockopt(cap_sock_all, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value)));
  len = sizeof(value);
  EXPECT_OK(getsockopt(cap_sock_all, SOL_SOCKET, SO_REUSEPORT, &value, &len));

  // Accept the incoming connection
  len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  EXPECT_NOTCAPABLE(accept(cap_sock_rw, (struct sockaddr *)&addr, &len));
  int conn_fd = accept(cap_sock_all, (struct sockaddr *)&addr, &len);
  EXPECT_OK(conn_fd);

#ifdef CAP_FROM_ACCEPT
  // New connection should also be a capability.
  cap_rights_t rights;
  cap_rights_init(&rights, 0);
  EXPECT_OK(cap_rights_get(conn_fd, &rights));
  EXPECT_RIGHTS_IN(&rights, &r_all);
#endif

  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(conn_fd);
  close(cap_sock_rw);
  close(cap_sock_all);
}

TEST(Socket, UDP) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  EXPECT_OK(sock);
  if (sock < 0) return;

  cap_rights_t r_rw;
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);
  cap_rights_t r_all;
  cap_rights_init(&r_all, CAP_READ, CAP_WRITE, CAP_SOCK_CLIENT, CAP_SOCK_SERVER);
  cap_rights_t r_connect;
  cap_rights_init(&r_connect, CAP_READ, CAP_WRITE, CAP_CONNECT);

  int cap_sock_rw = dup(sock);
  EXPECT_OK(cap_sock_rw);
  EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
  int cap_sock_all = dup(sock);
  EXPECT_OK(cap_sock_all);
  EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
  close(sock);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  socklen_t len = sizeof(addr);

  // Can only bind the fully-capable socket.
  EXPECT_NOTCAPABLE(bind_(cap_sock_rw, (struct sockaddr *)&addr, len));
  EXPECT_OK(bind_(cap_sock_all, (struct sockaddr *)&addr, len));
  getsockname(cap_sock_all, (struct sockaddr *)&addr, &len);
  int port = ntohs(addr.sin_port);

  // Can only do socket operations on the fully-capable socket.
  len = sizeof(addr);
  EXPECT_NOTCAPABLE(getsockname(cap_sock_rw, (struct sockaddr*)&addr, &len));
  int value = 1;
  EXPECT_NOTCAPABLE(setsockopt(cap_sock_rw, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value)));
  len = sizeof(value);
  EXPECT_NOTCAPABLE(getsockopt(cap_sock_rw, SOL_SOCKET, SO_REUSEPORT, &value, &len));

  len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  EXPECT_OK(getsockname(cap_sock_all, (struct sockaddr*)&addr, &len));
  EXPECT_EQ(AF_INET, addr.sin_family);
  EXPECT_EQ(htons(port), addr.sin_port);
  value = 1;
  EXPECT_OK(setsockopt(cap_sock_all, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value)));
  len = sizeof(value);
  EXPECT_OK(getsockopt(cap_sock_all, SOL_SOCKET, SO_REUSEPORT, &value, &len));

  pid_t child = fork();
  if (child == 0) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    EXPECT_OK(sock);
    int cap_sock_rw = dup(sock);
    EXPECT_OK(cap_sock_rw);
    EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
    int cap_sock_connect = dup(sock);
    EXPECT_OK(cap_sock_connect);
    EXPECT_OK(cap_rights_limit(cap_sock_connect, &r_connect));
    close(sock);

    // Can only sendmsg(2) to an address over a socket with CAP_CONNECT.
    unsigned char buffer[256];
    struct iovec iov;
    memset(&iov, 0, sizeof(iov));
    iov.iov_base = buffer;
    iov.iov_len = sizeof(buffer);

    struct msghdr mh;
    memset(&mh, 0, sizeof(mh));
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    mh.msg_name = &addr;
    mh.msg_namelen = sizeof(addr);

    EXPECT_NOTCAPABLE(sendmsg(cap_sock_rw, &mh, 0));
    EXPECT_OK(sendmsg(cap_sock_connect, &mh, 0));

#ifdef HAVE_SEND_RECV_MMSG
    struct mmsghdr mv;
    memset(&mv, 0, sizeof(mv));
    memcpy(&mv.msg_hdr, &mh, sizeof(struct msghdr));
    EXPECT_NOTCAPABLE(sendmmsg(cap_sock_rw, &mv, 1, 0));
    EXPECT_OK(sendmmsg(cap_sock_connect, &mv, 1, 0));
#endif
    close(cap_sock_rw);
    close(cap_sock_connect);
    exit(HasFailure());
  }
  // Wait for the child.
  int status;
  EXPECT_EQ(child, waitpid(child, &status, 0));
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(cap_sock_rw);
  close(cap_sock_all);
}
