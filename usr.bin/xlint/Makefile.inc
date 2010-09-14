#	$NetBSD: Makefile.inc,v 1.8 2002/02/04 00:18:32 thorpej Exp $
# $FreeBSD$

WARNS?=		0

.PATH:		${.CURDIR}/../common

# These assignments duplicate much of the functionality of
# MACHINE_CPUARCH, but there's no easy way to export make functions...
.if defined(TARGET_ARCH)
TARGET_CPUARCH=	${TARGET_ARCH:C/mipse[bl]/mips/:C/armeb/arm/}
.else
TARGET_CPUARCH=	${MACHINE_CPUARCH}
TARGET_ARCH=	${MACHINE_ARCH}
.endif
.if exists(${.CURDIR}/../arch/${TARGET_ARCH})
CFLAGS+=	-I${.CURDIR}/../arch/${TARGET_ARCH}
.else
CFLAGS+=	-I${.CURDIR}/../arch/${TARGET_CPUARCH}
.endif
CFLAGS+=	-I${.CURDIR}/../common

OBJECT_FMT=	ELF
