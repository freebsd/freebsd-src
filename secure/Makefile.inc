# $FreeBSD: src/secure/Makefile.inc,v 1.25.6.1 2008/11/25 02:59:29 kensmith Exp $

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
