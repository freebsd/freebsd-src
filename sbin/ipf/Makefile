# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipf
MAN=	ipf.4 ipf.5 ipf.8
SRCS=	ipf.c parse.c opt.c facpri.c common.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I- -I${.CURDIR}/../../sys/netinet
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
