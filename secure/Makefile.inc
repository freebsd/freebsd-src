# $FreeBSD: src/secure/Makefile.inc,v 1.25.12.1 2010/02/10 00:26:20 kensmith Exp $

.include <bsd.own.mk>

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if ${MK_IDEA} == "no"
CFLAGS+= -DNO_IDEA
.endif

.if ${MK_OPENSSH} != "no"
SSHDIR=		${.CURDIR}/../../../crypto/openssh
.endif
