# $FreeBSD: src/sbin/ipfstat/Makefile,v 1.4.2.6 2001/11/14 14:15:00 guido Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DSTATETOP
CFLAGS+=-I- -I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
