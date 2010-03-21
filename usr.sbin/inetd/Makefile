#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/inetd/Makefile,v 1.37.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

PROG=	inetd
MAN=	inetd.8
MLINKS=	inetd.8 inetd.conf.5
SRCS=	inetd.c builtins.c

WARNS?=	2
CFLAGS+= -DLOGIN_CAP
#CFLAGS+= -DSANITY_CHECK

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

DPADD=	${LIBUTIL} ${LIBWRAP}
LDADD=	-lutil -lwrap

# XXX for src/release/picobsd
.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
