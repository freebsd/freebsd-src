// Tests of SCTP functionality
// Requires: libsctp-dev package on Debian Linux, CONFIG_IP_SCTP in kernel config
#ifdef HAVE_SCTP
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "syscalls.h"
#include "capsicum.h"
#include "capsicum-test.h"

static cap_rights_t r_ro;
static cap_rights_t r_wo;
static cap_rights_t r_rw;
static cap_rights_t r_all;
static cap_rights_t r_all_nopeel;
#define DO_PEELOFF 0x1A
#define DO_TERM    0x1B

static int SctpClient(int port, unsigned char byte) {
  // Create sockets
  int sock = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  EXPECT_OK(sock);
  if (sock < 0) return sock;
  int cap_sock_ro = dup(sock);
  EXPECT_OK(cap_sock_ro);
  EXPECT_OK(cap_rights_limit(cap_sock_ro, &r_rw));
  int cap_sock_rw = dup(sock);
  EXPECT_OK(cap_sock_rw);
  EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
  int cap_sock_all = dup(sock);
  EXPECT_OK(cap_sock_all);
  EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
  close(sock);

  // Send a message.  Requires CAP_WRITE and CAP_CONNECT.
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(port);

  EXPECT_NOTCAPABLE(sctp_sendmsg(cap_sock_ro, &byte, 1,
                                 (struct sockaddr*)&serv_addr, sizeof(serv_addr),
                                 0, 0, 1, 0, 0));
  EXPECT_NOTCAPABLE(sctp_sendmsg(cap_sock_rw, &byte, 1,
                                 (struct sockaddr*)&serv_addr, sizeof(serv_addr),
                                 0, 0, 1, 0, 0));
  if (verbose) fprintf(stderr, "  [%d]sctp_sendmsg(%02x)\n", getpid_(), byte);
  EXPECT_OK(sctp_sendmsg(cap_sock_all, &byte, 1,
                         (struct sockaddr*)&serv_addr, sizeof(serv_addr),
                         0, 0, 1, 0, 0));
  close(cap_sock_ro);
  close(cap_sock_rw);
  return cap_sock_all;
}


TEST(Sctp, Socket) {
  int sock = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
  EXPECT_OK(sock);
  if (sock < 0) return;

  cap_rights_init(&r_ro, CAP_READ);
  cap_rights_init(&r_wo, CAP_WRITE);
  cap_rights_init(&r_rw, CAP_READ, CAP_WRITE);
  cap_rights_init(&r_all, CAP_READ, CAP_WRITE, CAP_SOCK_CLIENT, CAP_SOCK_SERVER);
  cap_rights_init(&r_all_nopeel, CAP_READ, CAP_WRITE, CAP_SOCK_CLIENT, CAP_SOCK_SERVER);
  cap_rights_clear(&r_all_nopeel, CAP_PEELOFF);

  int cap_sock_wo = dup(sock);
  EXPECT_OK(cap_sock_wo);
  EXPECT_OK(cap_rights_limit(cap_sock_wo, &r_wo));
  int cap_sock_rw = dup(sock);
  EXPECT_OK(cap_sock_rw);
  EXPECT_OK(cap_rights_limit(cap_sock_rw, &r_rw));
  int cap_sock_all = dup(sock);
  EXPECT_OK(cap_sock_all);
  EXPECT_OK(cap_rights_limit(cap_sock_all, &r_all));
  int cap_sock_all_nopeel = dup(sock);
  EXPECT_OK(cap_sock_all_nopeel);
  EXPECT_OK(cap_rights_limit(cap_sock_all_nopeel, &r_all_nopeel));
  close(sock);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  socklen_t len = sizeof(addr);

  // Can only bind the fully-capable socket.
  EXPECT_NOTCAPABLE(bind(cap_sock_rw, (struct sockaddr *)&addr, len));
  EXPECT_OK(bind(cap_sock_all, (struct sockaddr *)&addr, len));

  EXPECT_OK(getsockname(cap_sock_all, (struct sockaddr *)&addr, &len));
  int port = ntohs(addr.sin_port);

  // Now we know the port involved, fork off children to run clients.
  pid_t child1 = fork();
  if (child1 == 0) {
    // Child process 1: wait for server setup
    sleep(1);
    // Send a message that triggers peeloff
    int client_sock = SctpClient(port, DO_PEELOFF);
    sleep(1);
    close(client_sock);
    exit(HasFailure());
  }

  pid_t child2 = fork();
  if (child2 == 0) {
    // Child process 2: wait for server setup
    sleep(2);
    // Send a message that triggers server exit
    int client_sock = SctpClient(port, DO_TERM);
    close(client_sock);
    exit(HasFailure());
  }

  // Can only listen on the fully-capable socket.
  EXPECT_NOTCAPABLE(listen(cap_sock_rw, 3));
  EXPECT_OK(listen(cap_sock_all, 3));

  // Can only do socket operations on the fully-capable socket.
  len = sizeof(addr);
  EXPECT_NOTCAPABLE(getsockname(cap_sock_rw, (struct sockaddr*)&addr, &len));

  struct sctp_event_subscribe events;
  memset(&events, 0, sizeof(events));
  events.sctp_association_event = 1;
  events.sctp_data_io_event = 1;
  EXPECT_NOTCAPABLE(setsockopt(cap_sock_rw, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)));
  len = sizeof(events);
  EXPECT_NOTCAPABLE(getsockopt(cap_sock_rw, IPPROTO_SCTP, SCTP_EVENTS, &events, &len));
  memset(&events, 0, sizeof(events));
  events.sctp_association_event = 1;
  events.sctp_data_io_event = 1;
  EXPECT_OK(setsockopt(cap_sock_all, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)));
  len = sizeof(events);
  EXPECT_OK(getsockopt(cap_sock_all, IPPROTO_SCTP, SCTP_EVENTS, &events, &len));

  len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  EXPECT_OK(getsockname(cap_sock_all, (struct sockaddr*)&addr, &len));
  EXPECT_EQ(AF_INET, addr.sin_family);
  EXPECT_EQ(htons(port), addr.sin_port);

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  unsigned char buffer[1024];
  struct sctp_sndrcvinfo sri;
  memset(&sri, 0, sizeof(sri));
  int flags = 0;
  EXPECT_NOTCAPABLE(sctp_recvmsg(cap_sock_wo, buffer, sizeof(buffer),
                                 (struct sockaddr*)&client_addr, &addr_len,
                                 &sri, &flags));
  while (true) {
  retry:
    memset(&sri, 0, sizeof(sri));
    int len = sctp_recvmsg(cap_sock_rw, buffer, sizeof(buffer),
                           (struct sockaddr*)&client_addr, &addr_len,
                           &sri, &flags);
    if (len < 0 && errno == EAGAIN) goto retry;
    EXPECT_OK(len);
    if (len > 0) {
      if (verbose) fprintf(stderr, "[%d]sctp_recvmsg(%02x..)", getpid_(), (unsigned)buffer[0]);
      if (buffer[0] == DO_PEELOFF) {
        if (verbose) fprintf(stderr, "..peeling off association %08lx\n", (long)sri.sinfo_assoc_id);
        // Peel off the association.  Needs CAP_PEELOFF.
        int rc1 = sctp_peeloff(cap_sock_all_nopeel, sri.sinfo_assoc_id);
        EXPECT_NOTCAPABLE(rc1);
        int rc2 = sctp_peeloff(cap_sock_all, sri.sinfo_assoc_id);
        EXPECT_OK(rc2);
        int peeled = std::max(rc1, rc2);
        if (peeled > 0) {
#ifdef CAP_FROM_PEELOFF
          // Peeled off FD should have same rights as original socket.
          cap_rights_t rights;
          EXPECT_OK(cap_rights_get(peeled, &rights));
          EXPECT_RIGHTS_EQ(&r_all, &rights);
#endif
          close(peeled);
        }
      } else if (buffer[0] == DO_TERM) {
        if (verbose) fprintf(stderr, "..terminating server\n");
        break;
      }
    } else if (len < 0) {
      break;
    }
  }

  // Wait for the children.
  int status;
  int rc;
  EXPECT_EQ(child1, waitpid(child1, &status, 0));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);
  EXPECT_EQ(child2, waitpid(child2, &status, 0));
  rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  EXPECT_EQ(0, rc);

  close(cap_sock_wo);
  close(cap_sock_rw);
  close(cap_sock_all);
  close(cap_sock_all_nopeel);
}
#endif
