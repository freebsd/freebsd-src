# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipmon
MAN1=	ipmon.8
SRCS=	ipmon.c

CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
