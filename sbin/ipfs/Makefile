# $FreeBSD: src/sbin/ipfs/Makefile,v 1.3 2002/03/26 10:09:09 darrenr Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfs
MAN=	ipfs.8
SRCS=	ipfs.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter
WARNS=	0

MAINTAINER=	darrenr@freebsd.org

.include <bsd.prog.mk>
