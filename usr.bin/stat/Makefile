# $FreeBSD$

.include <bsd.own.mk>

PROG=	stat

LINKS=	${BINDIR}/stat ${BINDIR}/readlink
MLINKS=	stat.1 readlink.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
