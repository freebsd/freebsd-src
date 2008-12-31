# $FreeBSD: src/sys/modules/netgraph/atm/sscop/Makefile,v 1.1.28.1 2008/11/25 02:59:29 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/sscop ${LIBBASE}/netnatm/saal

KMOD=	ng_sscop
SRCS= 	ng_sscop.c saal_sscop.c
CFLAGS+= -I${LIBBASE} # -DSSCOP_DEBUG

.include <bsd.kmod.mk>
