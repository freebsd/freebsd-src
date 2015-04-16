# $FreeBSD$
#
# Some of these tests fail on 11.0-CURRENT @ r280355 with DEBUG bits on

TESTSDIR=	${TESTSBASE}/sys/socket

BINDIR=		${TESTSDIR}

# See comment below
#PROGS+=		unix_cmsg

TAP_TESTS_C+=	accept_fd_leak_test
ATF_TESTS_C+=	accf_data_attach_test
PLAIN_TESTS_C+=	fstat_test
PLAIN_TESTS_C+=	kqueue_test
PLAIN_TESTS_C+=	listen_backlog_test
PLAIN_TESTS_C+=	listenclose_test
PLAIN_TESTS_C+=	pr_atomic_test
PLAIN_TESTS_C+=	reconnect_test
PLAIN_TESTS_C+=	rtsocket_test
PLAIN_TESTS_C+=	sblock_test
TAP_TESTS_C+=	sendfile_test
PLAIN_TESTS_C+=	shutdown_test
PLAIN_TESTS_C+=	sigpipe_test
TAP_TESTS_C+=	so_setfib_test
PLAIN_TESTS_C+=	socketpair_test
PLAIN_TESTS_C+=	unix_bindconnect_test
PLAIN_TESTS_C+=	unix_close_race_test
# Lots of failures; see https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=199478
#TAP_TESTS_SH+=	unix_cmsg_test
PLAIN_TESTS_C+=	unix_gc_test
ATF_TESTS_C+=	unix_passfd_test
PLAIN_TESTS_C+=	unix_sendto_race_test
PLAIN_TESTS_C+=	unix_socket_test
PLAIN_TESTS_C+=	unix_sorflush_test
ATF_TESTS_C+=	zerosend_test

DPADD.sendfile_test+=	${LIBMD}
LDADD.sendfile_test+=	-lmd

# XXX: unix_cmsg_test and unix_passfd_test need to be fixed
NO_WCAST_ALIGN.clang=

WARNS?=	6

.include <bsd.test.mk>
