#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
#	$Id: Makefile.inc,v 1.10 1998/12/07 12:44:12 bde Exp $

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
