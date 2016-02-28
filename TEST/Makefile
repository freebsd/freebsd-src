# $NetBSD: Makefile,v 1.6 2016/02/15 21:38:07 christos Exp $

WIDECHAR ?= yes

NOMAN=1
PROG=wtc1
CPPFLAGS=-I${.CURDIR}/..
LDADD+=-ledit -ltermlib
DPADD+=${LIBEDIT} ${LIBTERMLIB}

.if "${WIDECHAR}" == "yes"
CPPFLAGS+=-DWIDECHAR
.endif

.ifdef DEBUG
CPPFLAGS+=-DDEBUG
.endif

.include <bsd.prog.mk>
