# $FreeBSD: src/sbin/ipmon/Makefile,v 1.6.2.3 2002/04/27 18:13:36 darrenr Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN=	ipl.4 ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I- -I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
