# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipf
MAN4=	ipf.4
MAN5=	ipf.5
MAN8=	ipf.8
SRCS=	ipf.c parse.c opt.c facpri.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
