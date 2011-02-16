# $FreeBSD: src/sys/modules/netgraph/atm/uni/Makefile,v 1.1.36.1 2010/12/21 17:10:29 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>
#

LIBBASE= ${.CURDIR}/../../../../contrib/ngatm

.PATH: ${.CURDIR}/../../../../netgraph/atm/uni
.PATH: ${LIBBASE}/netnatm/sig

KMOD=	ng_uni
SRCS=	ng_uni.c sig_call.c sig_uni.c sig_party.c sig_coord.c		\
	sig_reset.c sig_verify.c sig_print.c sig_unimsgcpy.c

CFLAGS+= -I${LIBBASE} -I${LIBBASE}/netnatm/sig # -DUNI_DEBUG

.include <bsd.kmod.mk>
