# $FreeBSD$

.include <src.opts.mk>

PACKAGE=ipfw
PROG=	ipfw
SRCS=	ipfw2.c dummynet.c ipv6.c main.c nat.c tables.c
WARNS?=	2

.if ${MK_PF} != "no"
SRCS+=	altq.c
CFLAGS+=-DPF
.endif

LIBADD=	util
MAN=	ipfw.8

.include <bsd.prog.mk>
