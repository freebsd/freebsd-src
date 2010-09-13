# $FreeBSD$

PROG=   crunchide
SRCS=	crunchide.c

# These assignments duplicate much of the functionality of
# MACHINE_CPUARCH, but there's no easy way to export make functions...

.if defined(TARGET_ARCH)
TARGET_CPUARCH=${TARGET_ARCH:C/mipse[bl]/mips/:C/armeb/arm/}
.else
TARGET_CPUARCH=${MACHINE_CPUARCH}
.endif
TARGET_ARCH?=	${MACHINE_ARCH}

.if ${TARGET_ARCH} == i386 && ${MACHINE_ARCH} == i386
CFLAGS+=-DNLIST_AOUT
SRCS+=	exec_aout.c
.endif

# nb: TARGET_ARCH for powerpc64 is correct here
.if ${TARGET_CPUARCH} == ia64 || ${TARGET_ARCH} == powerpc64 || \
    ${TARGET_CPUARCH} == sparc64 || ${TARGET_CPUARCH} == amd64
CFLAGS+=-DNLIST_ELF64
SRCS+=	exec_elf64.c
exec_elf64.o: exec_elf32.c
.else
CFLAGS+=-DNLIST_ELF32
SRCS+=	exec_elf32.c
.endif

.include <bsd.prog.mk>
