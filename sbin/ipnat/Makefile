# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN=	ipnat.1 ipnat.4 ipnat.5
MLINKS=	ipnat.5 ipnat.conf.5
SRCS=	ipnat.c kmem.c natparse.c common.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
