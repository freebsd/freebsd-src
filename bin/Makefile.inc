#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
#	$Id: Makefile.inc,v 1.7 1996/09/22 21:51:04 wosch Exp $

BINDIR?=	/bin
NOSHARED?=	YES

.if exists (${.CURDIR}/../../secure)

.if exists(${.CURDIR}/../../secure/lib/libcipher/obj)
CIPHEROBJDIR=    ${.CURDIR}/../../secure/lib/libcipher/obj
.else
CIPHEROBJDIR=    ${.CURDIR}/../../secure/lib/libcipher
.endif

.endif
