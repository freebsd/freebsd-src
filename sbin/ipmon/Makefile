# $FreeBSD: src/sbin/ipmon/Makefile,v 1.11 2002/03/20 09:04:34 darrenr Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN=	ipl.4 ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter
WARNS=	0

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
