# $FreeBSD: src/sys/modules/harp/Makefile,v 1.2.24.1 2008/10/02 02:57:24 kensmith Exp $
#
# Author: Harti Brandt <brandt@fokus.gmd.de>
#
.PATH: ${.CURDIR}/../../dev/harp

KMOD=	if_harp
SRCS=	if_harp.c device_if.h

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../include
CFLAGS+= -DDEBUG -DINVARIANTS

.include <bsd.kmod.mk>
