# $NetBSD: Makefile,v 1.8 2017/10/15 18:59:00 abhinav Exp $

NOMAN=1
PROG=wtc1 test_filecompletion
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermlib
DPADD+=${LIBEDIT} ${LIBTERMLIB}

.ifdef DEBUG
CPPFLAGS+=-DDEBUG
.endif

.include <bsd.prog.mk>
