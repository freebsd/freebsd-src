# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random
.PATH: ${.CURDIR}/../../crypto/blowfish

KMOD=	random
SRCS=	randomdev.c yarrow.c hash.c
SRCS+=	bf_cbc.c bf_skey.c bf_enc.c
SRCS+=	bus_if.h device_if.h vnode_if.h
NOMAN=	yes

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
