# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipnat
MAN1=	ipnat.1
MAN4=	ipnat.4
MAN5=	ipnat.5
SRCS=	ipnat.c kmem.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
