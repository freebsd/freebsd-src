#	@(#)Makefile	8.2 (Berkeley) 5/31/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	ln
MAN=	ln.1 symlink.7

LINKS=	${BINDIR}/ln ${BINDIR}/link
MLINKS=	ln.1 link.1

.if ${MK_TESTS} != "no"
SUBDIR+= 	tests
.endif

.include <bsd.prog.mk>
