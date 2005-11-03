# $FreeBSD: src/sys/modules/ipfw/Makefile,v 1.21 2004/10/25 20:02:33 andre Exp $

.PATH: ${.CURDIR}/../../netinet

KMOD=	ipfw
SRCS=	ip_fw2.c ip_fw_pfil.c

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
