# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/audit

ATF_TESTS_C=	file-create

SRCS.file-create+=	file-create.c
SRCS.file-create+=	utils.c

TEST_METADATA+= timeout="30"
TEST_METADATA+= required_user="root"

WARNS?=	6

LDFLAGS+=	-lbsm

.include <bsd.test.mk>
