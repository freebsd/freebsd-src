# $FreeBSD$

TESTSRC=	${SRCTOP}/contrib/netbsd-tests/kernel
.PATH:		${SRCTOP}/sys/kern

TESTSDIR=	${TESTSBASE}/sys/kern

ATF_TESTS_C+=	kern_copyin
ATF_TESTS_C+=	kern_descrip_test
ATF_TESTS_C+=	ptrace_test
PLAIN_TESTS_C+=	subr_unit_test
ATF_TESTS_C+=	unix_seqpacket_test
ATF_TESTS_C+=	unix_passfd_test
TEST_METADATA.unix_seqpacket_test+=	timeout="15"
ATF_TESTS_C+=	waitpid_nohang

LIBADD.ptrace_test+=			pthread
LIBADD.unix_seqpacket_test+=		pthread

NETBSD_ATF_TESTS_C+=	lockf_test
NETBSD_ATF_TESTS_C+=	mqueue_test

CFLAGS.mqueue_test+=	-I${SRCTOP}/tests
LIBADD.mqueue_test+=	rt

# subr_unit.c contains functions whose prototypes lie in headers that cannot be
# included in userland.  But as far as subr_unit_test goes, they're effectively
# static.  So it's ok to disable -Wmissing-prototypes for this program.
CFLAGS.subr_unit.c+=	-Wno-missing-prototypes
SRCS.subr_unit_test+=	subr_unit.c

WARNS?=	5

TESTS_SUBDIRS+=	acct
TESTS_SUBDIRS+=	execve
TESTS_SUBDIRS+=	pipe

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
