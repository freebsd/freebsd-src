# $FreeBSD: src/sbin/ipfstat/Makefile,v 1.4.2.1 2000/07/20 05:15:19 kris Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN8=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
