#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
#	$Id: Makefile.inc,v 1.4.6.3 1996/11/24 09:30:28 joerg Exp $

BINDIR?=	/bin
NOSHARED?=	YES

.if exists (${.CURDIR}/../../secure)

.if exists(${.CURDIR}/../../secure/lib/libcipher/obj)
CIPHEROBJDIR=    ${.CURDIR}/../../secure/lib/libcipher/obj
.else
CIPHEROBJDIR=    ${.CURDIR}/../../secure/lib/libcipher
.endif

.endif
