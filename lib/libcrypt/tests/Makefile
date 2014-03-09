# $FreeBSD$

# exercise libcrypt

TESTSDIR= ${TESTSBASE}/lib/libcrypt

ATF_TESTS_C= crypt_tests

CFLAGS+= -I${.CURDIR:H}
LDADD+= -L${.OBJDIR:H} -lcrypt

.include <atf.test.mk>
