#	@(#)Makefile	8.1 (Berkeley) 6/29/93
#	$FreeBSD: src/usr.bin/gprof/Makefile,v 1.4.6.1 2002/02/18 16:16:56 ru Exp $

PROG=	gprof
SRCS=	gprof.c aout.c arcs.c dfn.c elf.c lookup.c ${MACHINE_ARCH}.c hertz.c \
	printgprof.c printlist.c
FILES=	gprof.flat gprof.callg
FILESDIR=	${SHAREDIR}/misc

.include <bsd.prog.mk>
