# $RISS: if_arl/modules/if_arl/Makefile,v 1.3 2003/01/13 08:05:29 frol Exp $
# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/arl

KMOD=	if_arl
SRCS=	if_arl.c if_arl_isa.c device_if.h bus_if.h isa_if.h
CFLAGS=	-I${.CURDIR}/../..

.include <bsd.kmod.mk>
