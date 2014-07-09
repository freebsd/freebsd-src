# $FreeBSD$

.include <src.opts.mk>

PROG=		truncate
DPADD=		${LIBUTIL}
LDADD=		-lutil

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
