# $FreeBSD$

SRCTOP=		${.CURDIR:H:H:H}
OBJTOP=		${.OBJDIR:H:H:H}
TESTSRC=	${SRCTOP}/contrib/netbsd-tests/lib/libcrypt

NETBSD_ATF_TESTS_C+= crypt_test

CFLAGS+= -I${.CURDIR:H}
DPADD+=	${LIBCRYPT}
LDADD+= -lcrypt

.include <netbsd-tests.test.mk>

.include <bsd.test.mk>
