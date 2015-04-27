# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/aio

PLAIN_TESTS_C+=	aio_kqueue_test
PLAIN_TESTS_C+=	lio_kqueue_test
ATF_TESTS_C+=	aio_test

DPADD.aio_test+=	${LIBUTIL}
LDADD.aio_test+=	-lutil

WARNS?=	6

.include <bsd.test.mk>
