# $FreeBSD: src/secure/Makefile.inc,v 1.24 2004/08/06 07:27:05 cperciva Exp $

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if !defined(MAKE_IDEA)
CFLAGS+= -DNO_IDEA
.endif

.if !defined(NO_OPENSSH)
SSHDIR=		${.CURDIR}/../../../crypto/openssh
.endif
