# $FreeBSD: src/usr.sbin/authpf/Makefile,v 1.4.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../contrib/pf/authpf
.PATH:	${.CURDIR}/../../contrib/pf/pfctl

PROG=	authpf
MAN=	authpf.8
BINOWN=	root
BINGRP= authpf
BINMODE= 6555

SRCS=   authpf.c

CFLAGS+= -I${.CURDIR}/../../contrib/pf/pfctl

# XXX ALTQ:
CFLAGS+= -DENABLE_ALTQ

LDADD+= -lm -lmd -lutil
DPADD+= ${LIBM} ${LIBMD} ${LIBUTIL}

.include <bsd.prog.mk>
