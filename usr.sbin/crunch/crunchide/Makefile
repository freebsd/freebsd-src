# $FreeBSD$

PROG=   crunchide
SRCS=	crunchide.c exec_elf32.c exec_elf64.c

TARGET_ARCH?=	${MACHINE_ARCH}
.if ${TARGET_ARCH} == i386 && ${MACHINE_ARCH} == i386
SRCS+=	exec_aout.c
.endif
CFLAGS+=-Darch_${TARGET_ARCH}

.include <bsd.prog.mk>
