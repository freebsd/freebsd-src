# From:	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

PACKAGE=rcmds

.include <src.opts.mk>

PROG=	rshd
MAN=	rshd.8

PACKAGE=rcmds

WARNS?=	3
WFORMAT=0

LIBADD=	util pam

.if ${MK_BLACKLIST_SUPPORT} != "no"
CFLAGS+= -DUSE_BLACKLIST -I${SRCTOP}/contrib/blacklist/include
LIBADD+= blacklist
LDFLAGS+=-L${LIBBLACKLISTDIR}
.endif

.include <bsd.prog.mk>
