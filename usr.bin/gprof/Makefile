#	@(#)Makefile	8.1 (Berkeley) 6/29/93
#	$FreeBSD$

PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c elf.c lookup.c hertz.c \
	printgprof.c printlist.c kernel.c

.if ${MACHINE_ARCH} != "aarch64" && ${MACHINE_ARCH} != "riscv" && \
    ${MACHINE_ARCH} != "s390x"
SRCS+=	aout.c
CFLAGS+=	-DWITH_AOUT
.endif
FILES=	gprof.flat gprof.callg
FILESDIR=	${SHAREDIR}/misc

WARNS?=	1

.include <bsd.prog.mk>
