# $FreeBSD: src/sys/modules/dummynet/Makefile,v 1.1.2.2 2003/04/08 10:18:00 maxim Exp $

.PATH:  ${.CURDIR}/../../netinet
KMOD=   dummynet
SRCS=   ip_dummynet.c opt_bdg.h
NOMAN=
.if defined(IPFW2)
CFLAGS+= -DIPFW2
.endif
KMODDEPS= ipfw

.include <bsd.kmod.mk>
