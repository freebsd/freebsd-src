# $FreeBSD: src/sys/modules/netgraph/atm/atmbase/Makefile,v 1.2.32.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm
.PATH: ${LIBBASE}/netnatm/misc
.PATH: ${LIBBASE}/netnatm/msg

KMOD=	ngatmbase
SRCS= 	ngatmbase.c unimsg_common.c straddr.c				\
	traffic.c uni_ie.c uni_msg.c

CFLAGS+= -I${LIBBASE} # -DNGATM_DEBUG

.include <bsd.kmod.mk>
