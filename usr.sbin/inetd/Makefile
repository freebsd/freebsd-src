#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

MAINTAINER=	dwmalone

PROG=	inetd
SRCS=	inetd.c builtins.c
MAN=	inetd.8
MLINKS=	inetd.8 inetd.conf.5

COPTS+=	-Wall -DLOGIN_CAP
#COPTS+=	-DSANITY_CHECK

DPADD+=	${LIBUTIL} ${LIBWRAP}
LDADD+=	-lutil -lwrap
.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DINET6 -DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif
.include <bsd.prog.mk>
