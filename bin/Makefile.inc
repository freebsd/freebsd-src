#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

BINDIR?=	/bin
NOSHARED?=	YES

.if exists (${.CURDIR}/../../secure)

.if exists(${.CURDIR}/../../secure/lib/libcrypt/obj)
CRYPTOBJDIR=	${.CURDIR}/../../secure/lib/libcrypt/obj
.else
CRYPTOBJDIR=	${.CURDIR}/../../secure/lib/libcrypt
.endif

.endif
