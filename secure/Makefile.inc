# $FreeBSD$

DISTRIBUTION?=crypto

TELNETDIR=	${.CURDIR}/../../../crypto/telnet

.if exists(${.OBJDIR}/../../lib/libtelnet)
TELNETOBJDIR=	${.OBJDIR}/../../lib/libtelnet
.else
TELNETOBJDIR=	${.CURDIR}/../../lib/libtelnet
.endif

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if !defined(MAKE_IDEA) || ${MAKE_IDEA} != YES
CFLAGS+= -DNO_IDEA
.endif

.if !defined(NO_OPENSSH)
SSHDIR=                ${.CURDIR}/../../../crypto/openssh
.if exists(${.OBJDIR}/../../lib/libssh)
LIBSSH=                ${.OBJDIR}/../../lib/libssh/libssh.a
.else
LIBSSH=                ${.CURDIR}/../../lib/libssh/libssh.a
.endif
.endif
