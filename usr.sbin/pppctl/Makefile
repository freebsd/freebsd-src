#	$Id: Makefile,v 1.1.2.1 1997/06/28 01:08:56 brian Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall -Wmissing-prototypes
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN8=	pppctl.8

.include <bsd.prog.mk>
