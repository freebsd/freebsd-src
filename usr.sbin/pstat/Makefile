#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	pstat
WARNS?=	2
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MAN=	pstat.8
MLINKS=	pstat.8 swapinfo.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
