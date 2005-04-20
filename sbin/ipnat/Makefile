# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN=	ipnat.1 ipnat.4 ipnat.5
MLINKS=	ipnat.5 ipnat.conf.5
SRCS=	ipnat.c kmem.c natparse.c common.c printnat.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter
WARNS?=	0

DPADD=	${LIBKVM}
LDADD=	-lkvm

MAINTAINER=	darrenr@freebsd.org

.include <bsd.prog.mk>
