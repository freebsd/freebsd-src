# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	nfs

PROG=	nfsdumpstate
MAN=	nfsdumpstate.8

.if ${MK_INET_SUPPORT} != "no"
CFLAGS+= -DINET
.endif
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
