# $FreeBSD: src/sbin/ipnat/Makefile,v 1.4 1999/12/15 19:48:02 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN1=	ipnat.1
MAN4=	ipnat.4
MAN5=	ipnat.5
SRCS=	ipnat.c kmem.c natparse.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
