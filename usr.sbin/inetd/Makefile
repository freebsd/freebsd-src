#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/inetd/Makefile,v 1.18.2.1 2000/10/24 19:18:29 dwmalone Exp $

MAINTAINER=	dwmalone

PROG=	inetd
SRCS=	inetd.c builtins.c
MAN8=	inetd.8
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
