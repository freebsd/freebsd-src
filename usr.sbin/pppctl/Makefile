#	$Id: Makefile,v 1.2 1997/11/07 20:20:12 brian Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}

OPSYS!=	uname -s
.if (${OPSYS} == "OpenBSD")
MAN=	pppctl.8
.else
MAN8=	pppctl.8
.endif

.include <bsd.prog.mk>
