# $FreeBSD: src/sys/modules/siftr/Makefile,v 1.1.4.2.2.1 2010/12/21 17:10:29 kensmith Exp $

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../netinet
KMOD=	siftr
SRCS=	siftr.c

# Uncomment to add IPv6 support
#CFLAGS+=-DSIFTR_IPV6

.include <bsd.kmod.mk>
