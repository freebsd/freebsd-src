# $FreeBSD: src/sbin/ipfstat/Makefile,v 1.4.2.4 2001/08/01 06:55:39 obrien Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -DSTATETOP
CFLAGS+=-I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
