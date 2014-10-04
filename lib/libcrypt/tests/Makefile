# $FreeBSD$

# exercise libcrypt

TESTSDIR= ${TESTSBASE}/lib/libcrypt

ATF_TESTS_C= crypt_tests

CFLAGS+= -I${.CURDIR:H}
DPADD+=	${LIBCRYPT}
LDADD+= -lcrypt

.include <bsd.test.mk>
