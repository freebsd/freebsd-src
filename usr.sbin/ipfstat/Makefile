# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN8=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

.include <bsd.prog.mk>
