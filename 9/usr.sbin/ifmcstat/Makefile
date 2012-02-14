#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	ifmcstat
SRCS=	ifmcstat.c printb.c

MAN=	ifmcstat.8
BINMODE= 555

WARNS?=	2

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

.if ${MK_KVM_SUPPORT} != "no"
CFLAGS+=-DWITH_KVM
DPADD=	${LIBKVM}
LDADD=	-lkvm
.endif

.include <bsd.prog.mk>
