# $FreeBSD$
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm
.PATH: ${LIBBASE}/netnatm/misc

KMOD=	ngatmbase
SRCS= 	ngatmbase.c unimsg_common.c

CFLAGS+= -I${LIBBASE} # -DNGATM_DEBUG

.include <bsd.kmod.mk>
