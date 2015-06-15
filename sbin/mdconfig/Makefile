# $FreeBSD$

.include <src.opts.mk>

PROG=		mdconfig
MAN=		mdconfig.8

LIBADD=		util geom

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
