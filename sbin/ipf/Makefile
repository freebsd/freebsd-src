# $FreeBSD: src/sbin/ipf/Makefile,v 1.1 1998/03/21 13:52:51 peter Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipf
MAN1=	ipf.1
MAN4=	ipf.4
MAN5=	ipf.5
SRCS=	ipf.c parse.c opt.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
