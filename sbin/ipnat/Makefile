# $FreeBSD: src/sbin/ipnat/Makefile,v 1.4.2.6 2002/04/27 18:12:33 darrenr Exp $

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN=	ipnat.8 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c natparse.c common.c printnat.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I- -I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

DPADD+=	${LIBKVM}
LDADD+=  -lkvm

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
