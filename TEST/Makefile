# $NetBSD: Makefile,v 1.2 2003/12/05 13:37:48 lukem Exp $

NOMAN=1
PROG=test
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermcap
DPADD+=${LIBEDIT} ${LIBTERMCAP}

.ifdef DEBUG
CPPFLAGS+=-DDEBUG
.endif

.include <bsd.prog.mk>
