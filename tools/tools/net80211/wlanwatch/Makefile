# $FreeBSD$

PROG=	wlanwatch
BINDIR=	/usr/local/bin
MAN=

.include <bsd.own.mk>

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6
.endif

.include <bsd.prog.mk>
