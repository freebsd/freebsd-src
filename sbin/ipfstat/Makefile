# $FreeBSD$

.PATH:	${.CURDIR}/../../contrib/ipfilter ${.CURDIR}/../../contrib/ipfilter/man

PROG=	ipfstat
MAN8=	ipfstat.8
SRCS=	fils.c parse.c opt.c kmem.c facpri.c common.c
CFLAGS+=-DUSE_INET6 -DIPL_NAME=\"/dev/ipl\" -DSTATETOP -I- -I${.CURDIR}/../../sys/netinet -I${.CURDIR}/../../sys -I${.CURDIR}/../../contrib/ipfilter

DPADD= ${NCURSES}
LDADD= -lncurses

MAINTAINER=	guido@freebsd.org

.include <bsd.prog.mk>
