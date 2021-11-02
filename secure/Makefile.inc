# $FreeBSD$

.if exists(${.CURDIR:H:H}/lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR:H:H}/lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR:H:H}/lib/libcrypt
.endif

WARNS?=		0
