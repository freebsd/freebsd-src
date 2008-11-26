#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/inetd/Makefile,v 1.29.22.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	inetd
MAN=	inetd.8
MLINKS=	inetd.8 inetd.conf.5
SRCS=	inetd.c builtins.c

WARNS?=	2
CFLAGS+= -DLOGIN_CAP
#CFLAGS+= -DSANITY_CHECK

DPADD=	${LIBUTIL} ${LIBWRAP}
LDADD=	-lutil -lwrap

.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DINET6 -DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
