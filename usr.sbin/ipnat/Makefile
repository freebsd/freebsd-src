# $FreeBSD: src/usr.sbin/ipnat/Makefile,v 1.1 1998/03/21 13:53:06 peter Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN1=	ipnat.1
MAN4=	ipnat.4
MAN5=	ipnat.5
SRCS=	ipnat.c kmem.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
