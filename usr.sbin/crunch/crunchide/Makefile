# $FreeBSD$

PROG=   crunchide
SRCS=	crunchide.c exec_elf32.c exec_elf64.c

.if ${MACHINE_ARCH} == i386
SRCS+=	exec_aout.c
.endif

.include <bsd.prog.mk>
