# $FreeBSD: src/usr.sbin/ipmon/Makefile,v 1.2 1998/08/30 04:48:00 jkoshy Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN8=	ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
