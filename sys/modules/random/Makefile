# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random
.PATH: ${.CURDIR}/../../crypto/rijndael

KMOD=	random
SRCS=	randomdev.c yarrow.c hash.c
SRCS+=	rijndael-alg-fst.c rijndael-api-fst.c
SRCS+=	bus_if.h device_if.h vnode_if.h

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
