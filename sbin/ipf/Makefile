# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipf
MAN=	ipf.4 ipf.5 ipf.8
SRCS=	ipf.c parse.c opt.c facpri.c common.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\"
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter/netinet
CFLAGS+=-I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter
WARNS=	0

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
