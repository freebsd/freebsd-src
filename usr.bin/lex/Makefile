#	@(#)Makefile	5.4 (Berkeley) 6/24/90
#
# By default, flex will be configured to generate 8-bit scanners only if the
# -8 flag is given.  If you want it to always generate 8-bit scanners, add
# "-DDEFAULT_CSIZE=256" to CFLAGS.  Note that doing so will double the size
# of all uncompressed scanners.
# 
# If on your system you have trouble building flex due to 8-bit character
# problems, remove the -8 from FLEX_FLAGS and the "#define FLEX_8_BIT_CHARS"
# from the beginning of flexdef.h.
#
# To bootstrap lex, cp initscan.c to scan.c and run make.

PROG=		lex
SRCS=		ccl.c dfa.c ecs.c gen.c main.c misc.c nfa.c parse.c sym.c \
		tblcmp.c yylex.c
OBJS+=		scan.o
LFLAGS=		-is8
SUBDIR=		lib
CFLAGS+=	-I. -I${.CURDIR}
CLEANFILES+=	parse.c parse.h scan.c y.tab.h

parse.h parse.c:
	${YACC} -d ${.IMPSRC}
	mv y.tab.c parse.c
	mv y.tab.h parse.h

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/lex.skel \
	    ${DESTDIR}/usr/share/misc

scan.o: parse.c

.include <bsd.prog.mk>
