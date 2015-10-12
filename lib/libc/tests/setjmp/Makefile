# $FreeBSD$

NETBSD_ATF_TESTS_C=	t_setjmp
NETBSD_ATF_TESTS_C+=	t_threadjmp

DPADD.t_threadjmp+= ${LIBPTHREAD}
LDADD.t_threadjmp+= -lpthread

WARNS?=	4

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
