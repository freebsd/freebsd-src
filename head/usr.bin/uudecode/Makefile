#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	uudecode
LINKS=	${BINDIR}/uudecode ${BINDIR}/b64decode
MAN=

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
