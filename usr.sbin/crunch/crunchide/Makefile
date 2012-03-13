# $FreeBSD$

PROG=   crunchide
SRCS=	crunchide.c

TARGET_ARCH?=	${MACHINE_ARCH}

.if ${TARGET_ARCH} == i386 && ${MACHINE_ARCH} == i386
CFLAGS+=-DNLIST_AOUT
SRCS+=	exec_aout.c
.endif

.if ${TARGET_ARCH} == ia64 || ${TARGET_ARCH} == powerpc64 || \
    ${TARGET_ARCH} == sparc64 || ${TARGET_ARCH} == amd64 || \
    ${TARGET_ARCH:Mmips64*}
CFLAGS+=-DNLIST_ELF64
SRCS+=	exec_elf64.c
exec_elf64.o: exec_elf32.c
.else
CFLAGS+=-DNLIST_ELF32
SRCS+=	exec_elf32.c
.endif

.include <bsd.prog.mk>
