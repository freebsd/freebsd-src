# $FreeBSD: src/sys/modules/harp/Makefile,v 1.1 2003/07/21 13:56:22 harti Exp $
#
# Author: Harti Brandt <brandt@fokus.gmd.de>
#
.PATH: ${.CURDIR}/../../dev/harp

KMOD=	if_harp
SRCS=	if_harp.c device_if.h

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../include
CFLAGS+= -DDEBUG -DINVARIANTS -g
LDFLAGS+= -g

.include <bsd.kmod.mk>
