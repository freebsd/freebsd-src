# $FreeBSD$

TESTSDIR=	${TESTSBASE}/sys/posixshm

TAP_TESTS_C+=	posixshm_test
PLAIN_TESTS_C+=	posixshm_test2

SRCS.posixshm_test=	posixshm.c test.c
SRCS.posixshm_test2=	shm_test.c

.include <bsd.test.mk>
