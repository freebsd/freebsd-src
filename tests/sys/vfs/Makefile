# $FreeBSD$

PACKAGE=	tests

TESTSDIR=		${TESTSBASE}/sys/vfs

ATF_TESTS_C+=	lookup_cap_dotdot
CFLAGS.lookup_cap_dotdot.c+=	-I${SRCTOP}/tests

PLAIN_TESTS_SH+=	trailing_slash

WARNS?=		6

.include <bsd.test.mk>
