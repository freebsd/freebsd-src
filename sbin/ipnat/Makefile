# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN1=	ipnat.1
MAN4=	ipnat.4
MAN5=	ipnat.5
MLINKS=	ipnat.5 ipnat.conf.5
SRCS=	ipnat.c kmem.c natparse.c common.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
