# $FreeBSD: src/sys/modules/ipfw/Makefile,v 1.11 1999/08/28 00:47:21 peter Exp $

.PATH:	${.CURDIR}/../../netinet
KMOD=	ipfw
SRCS=	ip_fw.c
NOMAN=
CFLAGS+= -DIPFIREWALL
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#CFLAGS+= -DIPFIREWALL_VERBOSE_LIMIT=100
#
#If you want it to pass all packets by default
#CFLAGS+= -DIPFIREWALL_DEFAULT_TO_ACCEPT
#

.include <bsd.kmod.mk>
