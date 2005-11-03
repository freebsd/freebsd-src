# $RISS: if_arl/modules/if_arl/Makefile,v 1.3 2003/01/13 08:05:29 frol Exp $
# $FreeBSD: src/sys/modules/arl/Makefile,v 1.3 2005/02/09 11:50:43 ru Exp $

.PATH:	${.CURDIR}/../../dev/arl

KMOD=	if_arl
SRCS=	if_arl.c if_arl_isa.c
SRCS+=	opt_inet.h device_if.h bus_if.h isa_if.h
CFLAGS+= -I${.CURDIR}/../..

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
