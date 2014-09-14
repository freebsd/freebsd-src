# $FreeBSD$

TESTSDIR=	${TESTSBASE}/lib/libmp

TAP_TESTS_C+=	legacy_test

DPADD+=		${LIBCRYPTO} ${LIBMP}
LDADD+=		-lcrypto -lmp

.include <bsd.test.mk>
