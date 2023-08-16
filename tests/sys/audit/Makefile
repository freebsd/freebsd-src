
TESTSDIR=	${TESTSBASE}/sys/audit

ATF_TESTS_C=	file-attribute-access
ATF_TESTS_C+=	file-attribute-modify
ATF_TESTS_C+=	file-create
ATF_TESTS_C+=	file-delete
ATF_TESTS_C+=	file-close
ATF_TESTS_C+=	file-write
ATF_TESTS_C+=	file-read
ATF_TESTS_C+=	open
ATF_TESTS_C+=	ioctl
ATF_TESTS_C+=	network
ATF_TESTS_C+=	inter-process
ATF_TESTS_C+=	administrative
ATF_TESTS_C+=	process-control
ATF_TESTS_C+=	miscellaneous

SRCS.file-attribute-access+=	file-attribute-access.c
SRCS.file-attribute-access+=	utils.c
SRCS.file-attribute-modify+=	file-attribute-modify.c
SRCS.file-attribute-modify+=	utils.c
SRCS.file-create+=	file-create.c
SRCS.file-create+=	utils.c
SRCS.file-delete+=	file-delete.c
SRCS.file-delete+=	utils.c
SRCS.file-close+=	file-close.c
SRCS.file-close+=	utils.c
SRCS.file-write+=	file-write.c
SRCS.file-write+=	utils.c
SRCS.file-read+=	file-read.c
SRCS.file-read+=	utils.c
SRCS.open+=		open.c
SRCS.open+=		utils.c
SRCS.ioctl+=		ioctl.c
SRCS.ioctl+=		utils.c
SRCS.network+=		network.c
SRCS.network+=		utils.c
SRCS.inter-process+=		inter-process.c
SRCS.inter-process+=		utils.c
SRCS.administrative+=		administrative.c
SRCS.administrative+=		utils.c
SRCS.process-control+=		process-control.c
SRCS.process-control+=		utils.c
SRCS.miscellaneous+=		miscellaneous.c
SRCS.miscellaneous+=		utils.c

TEST_METADATA+= timeout="30"
TEST_METADATA+= required_user="root"
# Only one process can be auditing, if we attempt to run these tests in parallel
# some of them will fail to start auditing.
# TODO: it would be nice to be able to run them in parallel with other non-audit
# tests using some internal form of synchronization.
# TODO: In addititon to test failures, running in parallel can trigger a kernel
# panic: https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=253616
TEST_METADATA+= is_exclusive="true"
TEST_METADATA+=	required_files="/etc/rc.d/auditd /dev/auditpipe"

MK_PIE:=	no	# XXX libprivateauditd.a is not PIE
LDFLAGS+=	-lbsm -lutil
OPENBSMDIR=${SRCTOP}/contrib/openbsm
CFLAGS+=	-I${OPENBSMDIR}
LDADD+=	${LIBAUDITD}

CFLAGS.process-control.c+=	-I${SRCTOP}/tests

.include <bsd.test.mk>
