# $FreeBSD: src/usr.sbin/crunch/crunchide/Makefile,v 1.2.6.1 2002/07/25 09:33:17 ru Exp $

PROG=   crunchide
SRCS=	crunchide.c exec_elf32.c exec_elf64.c

TARGET_ARCH?=	${MACHINE_ARCH}
.if ${TARGET_ARCH} == i386 && ${MACHINE_ARCH} == i386
SRCS+=	exec_aout.c
.endif
CFLAGS+=-Darch_${TARGET_ARCH}

.include <bsd.prog.mk>
