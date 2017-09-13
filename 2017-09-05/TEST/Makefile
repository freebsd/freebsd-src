# $NetBSD: Makefile,v 1.7 2016/03/23 22:27:48 christos Exp $

NOMAN=1
PROG=wtc1
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermlib
DPADD+=${LIBEDIT} ${LIBTERMLIB}

.ifdef DEBUG
CPPFLAGS+=-DDEBUG
.endif

.include <bsd.prog.mk>
