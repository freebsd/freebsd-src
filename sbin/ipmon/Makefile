# $FreeBSD: src/sbin/ipmon/Makefile,v 1.13 2003/06/06 11:30:49 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN=	ipl.4 ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DLOGFAC=LOG_SECURITY
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter
WARNS=	0

MAINTAINER=	darrenr@freebsd.org

.include <bsd.prog.mk>
