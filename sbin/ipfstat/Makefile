# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c printstate.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DSTATETOP
CFLAGS+=-I- -I${.CURDIR}/../../sys/contrib/ipfilter
CFLAGS+=-I${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../../contrib/ipfilter

DPADD+=	${LIBNCURSES} ${LIBKVM}
LDADD+=	-lncurses -lkvm

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
