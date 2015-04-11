# $FreeBSD$
#
# Some of these tests fail on 11.0-CURRENT @ r280355 with DEBUG bits on

TESTSDIR=	${TESTSBASE}/sys/socket

BINDIR=		${TESTSDIR}

PROGS+=		unix_cmsg

TAP_TESTS_C+=	accept_fd_leak_test
# accf_data_attach: not ok 9 - setsockopt() after listen() failed with 2 (No such file or directory)
TAP_TESTS_C+=	accf_data_attach_test
PLAIN_TESTS_C+=	fstat_test
PLAIN_TESTS_C+=	kqueue_test
PLAIN_TESTS_C+=	listen_backlog_test
PLAIN_TESTS_C+=	listenclose_test
PLAIN_TESTS_C+=	pr_atomic_test
PLAIN_TESTS_C+=	reconnect_test
# rtsocket: socket(PF_ROUTE, SOCK_STREAM, 0): Protocol wrong type for socket
PLAIN_TESTS_C+=	rtsocket_test
PLAIN_TESTS_C+=	sblock_test
TAP_TESTS_C+=	sendfile_test
PLAIN_TESTS_C+=	shutdown_test
PLAIN_TESTS_C+=	sigpipe_test
TAP_TESTS_C+=	so_setfib_test
PLAIN_TESTS_C+=	socketpair_test
PLAIN_TESTS_C+=	unix_bindconnect_test
PLAIN_TESTS_C+=	unix_close_race_test
# Lots of failures
TAP_TESTS_SH+=	unix_cmsg_test
# unix_gc: twosome_drop1: sendfd: before 0 after 0
PLAIN_TESTS_C+=	unix_gc_test
# unix_passfd: test8-rights+creds+payload: recvmsg: 24 bytes received
PLAIN_TESTS_C+=	unix_passfd_test
PLAIN_TESTS_C+=	unix_sendtorace_test
# unix_socket: socket(PF_LOCAL, SOCK_RAW, 0): Protocol wrong type for socket
PLAIN_TESTS_C+=	unix_socket_test
PLAIN_TESTS_C+=	unix_sorflush_test
# zerosend: tcp_0write: bind(127.0.0.1, 10001): Address already in use
PLAIN_TESTS_C+=	zerosend_test

DPADD.sendfile_test+=	${LIBMD}
LDADD.sendfile_test+=	-lmd

# XXX: this doesn't work..?
#WARNS.unix_cmsg=	3
#WARNS.unix_gc=		3
#WARNS.unix_passfd=	3
#WARNS.zerosend=		2

WARNS=	2

.include <bsd.test.mk>
