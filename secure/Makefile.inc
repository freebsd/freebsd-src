# $FreeBSD$

.include <bsd.own.mk>

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if ${MK_OPENSSH} != "no"
SSHDIR=		${.CURDIR}/../../../crypto/openssh
.endif

WARNS?=		0
