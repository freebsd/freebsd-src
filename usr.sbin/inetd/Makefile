#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

CONFS=	inetd.conf
PROG=	inetd
MAN=	inetd.8
MLINKS=	inetd.8 inetd.conf.5
SRCS=	inetd.c builtins.c

CFLAGS+= -DLOGIN_CAP
#CFLAGS+= -DSANITY_CHECK

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

LIBADD=	util

.if ${MK_TCP_WRAPPERS} != "no"
CFLAGS+=	-DLIBWRAP
LIBADD+=	wrap
.endif

CFLAGS+= -DIPSEC
LIBADD+=	ipsec

.include <bsd.prog.mk>
