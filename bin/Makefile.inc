#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
#	$Id: Makefile.inc,v 1.9 1997/02/22 14:01:23 peter Exp $

BINDIR?=	/bin
CFLAGS+=	-Wall -Wformat
NOSHARED?=	YES

.if exists (${.CURDIR}/../../secure)

.if exists(${.CURDIR}/../../secure/lib/libcipher/obj)
CIPHEROBJDIR=    ${.CURDIR}/../../secure/lib/libcipher/obj
.else
CIPHEROBJDIR=    ${.CURDIR}/../../secure/lib/libcipher
.endif

.endif
