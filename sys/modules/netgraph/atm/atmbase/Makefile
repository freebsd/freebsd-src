# $FreeBSD: src/sys/modules/netgraph/atm/atmbase/Makefile,v 1.2.28.1 2008/11/25 02:59:29 kensmith Exp $
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
