#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93

BINDIR?=	/sbin
NOSHARED?=	YES

.if exists(${.CURDIR}/../../lib/libcrypt/obj)
SCRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt/obj
.else
SCRYPTOBJDIR=	${.CURDIR}/../../lib/libcrypt
.endif

.if exists (${.CURDIR}/../../secure)

.if exists(${.CURDIR}/../../secure/lib/libcrypt/obj)
DESCRYPTOBJDIR=	${.CURDIR}/../../secure/lib/libcrypt/obj
.else
DESCRYPTOBJDIR=	${.CURDIR}/../../secure/lib/libcrypt
.endif

.endif
