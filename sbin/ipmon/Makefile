# $FreeBSD: src/sbin/ipmon/Makefile,v 1.6 2000/02/09 21:01:35 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN4=	ipl.4
MAN8=	ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
