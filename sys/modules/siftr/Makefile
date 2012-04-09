# $FreeBSD: src/sys/modules/siftr/Makefile,v 1.1.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../netinet
KMOD=	siftr
SRCS=	siftr.c

# Uncomment to add IPv6 support
#CFLAGS+=-DSIFTR_IPV6

.include <bsd.kmod.mk>
