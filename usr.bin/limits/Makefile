# $FreeBSD$

.include <bsd.own.mk>

PROG=	limits
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
