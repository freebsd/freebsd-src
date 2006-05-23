#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/inetd/Makefile,v 1.29 2003/10/30 22:56:44 peter Exp $

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
