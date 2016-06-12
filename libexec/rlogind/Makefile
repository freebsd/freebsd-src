#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <src.opts.mk>

PACKAGE=rcmds
PROG=	rlogind
MAN=	rlogind.8
PACKAGE=rcmds
LIBADD=	util
WARNS?=	2

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.if ${MK_BLACKLIST_SUPPORT} != "no"
CFLAGS+= -DUSE_BLACKLIST -I${SRCTOP}/contrib/blacklist/include
LIBADD+= blacklist
LDFLAGS+=-L${LIBBLACKLISTDIR}
.endif

.include <bsd.prog.mk>
