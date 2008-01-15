# $FreeBSD: src/sys/modules/netgraph/atm/sscop/Makefile,v 1.1 2003/10/24 07:42:08 harti Exp $
#
# Author: Harti Brandt <harti@freebsd.org>

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/sscop ${LIBBASE}/netnatm/saal

KMOD=	ng_sscop
SRCS= 	ng_sscop.c saal_sscop.c
CFLAGS+= -I${LIBBASE} # -DSSCOP_DEBUG

.include <bsd.kmod.mk>
