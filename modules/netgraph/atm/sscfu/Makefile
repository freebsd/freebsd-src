# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/sscfu ${LIBBASE}/netnatm/saal

KMOD=	ng_sscfu
SRCS= 	ng_sscfu.c saal_sscfu.c
CFLAGS+= -I${LIBBASE} # -DSSCFU_DEBUG

.include <bsd.kmod.mk>
