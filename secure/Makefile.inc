#	$Id: Makefile.inc,v 1.8 1996/11/07 14:41:20 markm Exp $

DISTRIBUTION=des

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif
