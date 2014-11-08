# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random
.PATH: ${.CURDIR}/../../crypto/rijndael
.PATH: ${.CURDIR}/../../crypto/sha2

KMOD=	random
SRCS=	randomdev_soft.c
SRCS+=	yarrow.c fortuna.c hash.c
SRCS+=	rijndael-alg-fst.c rijndael-api-fst.c sha2.c sha256c.c
SRCS+=	bus_if.h device_if.h vnode_if.h opt_cpu.h opt_random.h

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
