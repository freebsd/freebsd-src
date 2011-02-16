#	$NetBSD: Makefile.inc,v 1.8 2002/02/04 00:18:32 thorpej Exp $
# $FreeBSD: src/usr.bin/xlint/Makefile.inc,v 1.5.34.1 2010/12/21 17:10:29 kensmith Exp $

WARNS?=		0

.PATH:		${.CURDIR}/../common

TARGET_ARCH?=	${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../arch/${TARGET_ARCH}
CFLAGS+=	-I${.CURDIR}/../common

OBJECT_FMT=	ELF
