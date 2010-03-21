# $FreeBSD: src/sys/modules/netgraph/atm/sscfu/Makefile,v 1.1.34.1 2010/02/10 00:26:20 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/sscfu ${LIBBASE}/netnatm/saal

KMOD=	ng_sscfu
SRCS= 	ng_sscfu.c saal_sscfu.c
CFLAGS+= -I${LIBBASE} # -DSSCFU_DEBUG

.include <bsd.kmod.mk>
