#	$Id: Makefile.inc,v 1.6 1995/05/11 22:07:49 jkh Exp $

DISTRIBUTION=des

.if exists(${.CURDIR}/../../lib/libtelnet/obj)
TELNETOBJDIR=	${.CURDIR}/../../lib/libtelnet/obj
.else
TELNETOBJDIR=	${.CURDIR}/../../lib/libtelnet
.endif

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif
