#	$NetBSD: Makefile.inc,v 1.8 2002/02/04 00:18:32 thorpej Exp $
# $FreeBSD: src/usr.bin/xlint/Makefile.inc,v 1.5.24.1 2008/10/02 02:57:24 kensmith Exp $

WARNS?=		0

.PATH:		${.CURDIR}/../common

TARGET_ARCH?=	${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../arch/${TARGET_ARCH}
CFLAGS+=	-I${.CURDIR}/../common

OBJECT_FMT=	ELF
