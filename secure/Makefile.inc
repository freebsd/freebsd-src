# $FreeBSD$

DISTRIBUTION?=crypto

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if !defined(MAKE_IDEA) || ${MAKE_IDEA} != YES
CFLAGS+= -DNO_IDEA
.endif

.if !defined(NO_OPENSSH)
SSHDIR=		${.CURDIR}/../../../crypto/openssh
.endif
