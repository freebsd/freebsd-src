# $FreeBSD: src/sbin/ipnat/Makefile,v 1.4.2.1 2000/07/20 05:46:06 kris Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN1=	ipnat.1
MAN4=	ipnat.4
MAN5=	ipnat.5
SRCS=	ipnat.c kmem.c natparse.c common.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
