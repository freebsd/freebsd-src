# $FreeBSD$

SRCTOP=		${.CURDIR:H:H:H}
OBJTOP=		${.OBJDIR:H:H:H}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/kernel

TESTSDIR=	${TESTSBASE}/sys/kern

ATF_TESTS_C+=	kern_descrip_test
ATF_TESTS_C+=	ptrace_test
TEST_METADATA.ptrace_test+=		timeout="15"
ATF_TESTS_C+=	reaper
ATF_TESTS_C+=	unix_passfd_test
ATF_TESTS_C+=	unix_seqpacket_test
TEST_METADATA.unix_seqpacket_test+=	timeout="15"
ATF_TESTS_C+=	unix_socketpair_test
ATF_TESTS_C+=	waitpid_nohang

LDADD.ptrace_test+=			-lpthread
LDADD.unix_seqpacket_test+=		-lpthread

NETBSD_ATF_TESTS_C+=	lockf_test
NETBSD_ATF_TESTS_C+=	mqueue_test

CFLAGS.mqueue_test+=	-I${SRCTOP}/tests
DPADD.mqueue_test+=	${LIBRT}
LDADD.mqueue_test+=	-lrt

WARNS?=	5

TESTS_SUBDIRS+=	acct
TESTS_SUBDIRS+=	execve
TESTS_SUBDIRS+=	pipe

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
