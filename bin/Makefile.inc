#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/Makefile.inc,v 1.11 1999/08/27 23:12:21 peter Exp $

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
