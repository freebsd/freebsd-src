# $FreeBSD$

.PATH:  ${.CURDIR}/../../netinet
KMOD=   dummynet
SRCS=   ip_dummynet.c opt_bdg.h
NOMAN=
.if defined(IPFW2)
CFLAGS+= -DIPFW2
.endif
KMODDEPS= ipfw

.include <bsd.kmod.mk>
