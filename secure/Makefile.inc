#	$Id: Makefile.inc,v 1.6.4.1 1995/09/16 03:13:19 gibbs Exp $

DISTRIBUTION?=des

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
