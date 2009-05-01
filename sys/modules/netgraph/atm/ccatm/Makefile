# $FreeBSD: src/sys/modules/netgraph/atm/ccatm/Makefile,v 1.1.28.1 2009/04/15 03:14:26 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/ccatm
.PATH: ${LIBBASE}/netnatm/api

KMOD=	ng_ccatm
SRCS=	ng_ccatm.c cc_conn.c cc_data.c cc_dump.c cc_port.c cc_sig.c	\
	cc_user.c unisap.c

CFLAGS+= -I${LIBBASE} -I${LIBBASE}/netnatm/ccatm -DCCATM_DEBUG

.include <bsd.kmod.mk>
