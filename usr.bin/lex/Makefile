# $Id: Makefile,v 1.6 1996/08/07 13:25:59 peter Exp $
#
# By default, flex will be configured to generate 8-bit scanners only if the
# -8 flag is given.  If you want it to always generate 8-bit scanners, add
# "-DDEFAULT_CSIZE=256" to CFLAGS.  Note that doing so will double the size
# of all uncompressed scanners.
#
# Bootstrapping of lex is handled automatically.
# Also note that flex.skel no longer gets installed.
#

PROG=		lex
LINKS+=	${BINDIR}/lex ${BINDIR}/lex++
LINKS+=	${BINDIR}/lex ${BINDIR}/flex
LINKS+=	${BINDIR}/lex ${BINDIR}/flex++

SRCS=		scan.c ccl.c dfa.c ecs.c gen.c main.c misc.c nfa.c parse.y \
		skel.c sym.c tblcmp.c yylex.c
LFLAGS+=	-is
CFLAGS+=	-I. -I${.CURDIR}
MAN1=		lex.1
MLINKS+=	lex.1 flex.1
MLINKS+=	lex.1 flex++.1
MLINKS+=	lex.1 lex++.1

CLEANFILES+=	parse.c parse.h scan.c y.tab.h y.tab.c

.if !defined(NOLIB)
SUBDIR=		lib
.endif

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 644 \
		${.CURDIR}/FlexLexer.h ${DESTDIR}/usr/include/g++

.depend: parse.h

parse.c parse.h: parse.y
	$(YACC) -d $(.CURDIR)/parse.y
	mv -f y.tab.c parse.c
	mv -f y.tab.h parse.h

bootstrap: initscan.c
	@cmp -s ${.CURDIR}/initscan.c scan.c || { \
		echo "Bootstrapping flex" ; \
		rm -f scan.c ; \
		cp -f ${.CURDIR}/initscan.c scan.c ; \
	}

scan.o:	parse.c

test: check
check: $(PROG)
	./$(PROG) $(LFLAGS) -t $(COMPRESSION) $(.CURDIR)/scan.l \
	| sed s,\"$(.CURDIR)/scan.l",\"scan.l", \
	| diff $(.CURDIR)/initscan.c -
	@echo "Check successful"

.include <bsd.prog.mk>
