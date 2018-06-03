# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/audit

ATF_TESTS_C=	file-create
ATF_TESTS_C+=	file-delete
ATF_TESTS_C+=	file-close
ATF_TESTS_C+=	file-write
ATF_TESTS_C+=	file-read

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

TEST_METADATA+= timeout="30"
TEST_METADATA+= required_user="root"
TEST_METADATA+= is_exclusive="true"

WARNS?=	6

LDFLAGS+=	-lbsm -lutil

.include <bsd.test.mk>
