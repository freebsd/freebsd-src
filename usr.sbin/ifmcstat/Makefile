#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

PROG=	ifmcstat
MAN=	ifmcstat.8
BINMODE= 550

WARNS?=	2

.if !defined(MK_INET6_SUPPORT) || ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif

.if !defined(MK_KVM_SUPPORT) || ${MK_KVM_SUPPORT} != "no"
CFLAGS+=-DWITH_KVM
DPADD=	${LIBKVM}
LDADD=	-lkvm
.endif

.include <bsd.prog.mk>
