#	@(#)Makefile	8.1 (Berkeley) 6/29/93
#	$FreeBSD: src/usr.bin/gprof/Makefile,v 1.7 2001/12/17 16:04:40 ru Exp $

PROG=	gprof
SRCS=	gprof.c aout.c arcs.c dfn.c elf.c lookup.c ${MACHINE_ARCH}.c hertz.c \
	printgprof.c printlist.c kernel.c
FILES=	gprof.flat gprof.callg
FILESDIR=	${SHAREDIR}/misc

.include <bsd.prog.mk>
