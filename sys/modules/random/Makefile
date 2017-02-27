# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random
.PATH: ${.CURDIR}/../../crypto/rijndael
.PATH: ${.CURDIR}/../../crypto/sha2

KMOD=	random
SRCS=	randomdev.c
.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
SRCS+=	nehemiah.c
SRCS+=	ivy.c
.endif
SRCS+=	randomdev_soft.c yarrow.c hash.c
SRCS+=	random_harvestq.c live_entropy_sources.c rwfile.c
SRCS+=	rijndael-alg-fst.c rijndael-api-fst.c sha256c.c sha512c.c
SRCS+=	bus_if.h device_if.h vnode_if.h opt_cpu.h opt_random.h

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
