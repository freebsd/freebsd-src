# $FreeBSD$

.include <src.opts.mk>

PROG=	ident

LIBADD=	sbuf

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
