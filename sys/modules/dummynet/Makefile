# $FreeBSD: src/sys/modules/dummynet/Makefile,v 1.1.2.1 2001/11/03 00:42:01 luigi Exp $

.PATH:  ${.CURDIR}/../../netinet
KMOD=   dummynet
SRCS=   ip_dummynet.c opt_bdg.h
NOMAN=
KMODDEPS= ipfw

.include <bsd.kmod.mk>
