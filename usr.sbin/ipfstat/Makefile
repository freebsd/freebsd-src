# $FreeBSD: src/usr.sbin/ipfstat/Makefile,v 1.1 1998/03/21 13:52:55 peter Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN8=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
