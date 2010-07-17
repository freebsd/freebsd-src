# $FreeBSD: src/sys/modules/netgraph/atm/sscop/Makefile,v 1.1.32.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/sscop ${LIBBASE}/netnatm/saal

KMOD=	ng_sscop
SRCS= 	ng_sscop.c saal_sscop.c
CFLAGS+= -I${LIBBASE} # -DSSCOP_DEBUG

.include <bsd.kmod.mk>
