# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipf
MAN1=	ipf.1
MAN4=	ipf.4
MAN5=	ipf.5
SRCS=	ipf.c parse.c opt.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
