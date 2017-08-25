# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/aio

ATF_TESTS_C+=	aio_test
TEST_METADATA.aio_test+= timeout="30"

PLAIN_TESTS_C+=	aio_kqueue_test
PLAIN_TESTS_C+=	lio_kqueue_test

LIBADD.aio_test+=	util rt

CFLAGS+=	-I${.CURDIR:H:H}

WARNS?=	6

.include <bsd.test.mk>
