# $Id$
#
# By default, flex will be configured to generate 8-bit scanners only if the
# -8 flag is given.  If you want it to always generate 8-bit scanners, add
# "-DDEFAULT_CSIZE=256" to CFLAGS.  Note that doing so will double the size
# of all uncompressed scanners.
#
# Bootstrapping of lex is handled automatically.
# ALso note that flex.skel no longer gets installed.
#
# XXX Todo:
#	Install as lex++, and install FlexLexer.h

PROG=		lex
LINKS=		${BINDIR}/lex ${BINDIR}/flex 
#LINKS+=	${BINDIR}/lex ${BINDIR}/lex++ ${BINDIR}/flex ${BINDIR}/flex++

SRCS=		ccl.c dfa.c ecs.c gen.c main.c misc.c nfa.c parse.y \
		skel.c sym.c tblcmp.c yylex.c
OBJS+=		scan.o
LFLAGS+=	-is
CFLAGS+=	-I. -I${.CURDIR}
MAN1=		flex.1 flexdoc.1
MLINKS=		flex.1 lex.1 flexdoc.1 lexdoc.1

CLEANFILES+=	parse.c parse.h scan.c y.tab.h


SUBDIR=		lib

.depend: parse.h

parse.c parse.h: parse.y
	$(YACC) -d $(.CURDIR)/parse.y
	mv y.tab.c parse.c
	mv y.tab.h parse.h

.if exists(/usr/bin/lex)
scan.o:	parse.c
.else
# We must bootstrap
scan.o: scan.c parse.h

scan.c:
	@echo "Bootstrapping flex"
	@rm -f scan.c
	@cp -pf ${.CURDIR}/initscan.c scan.c
.endif

test: check
check: $(PROG)
	./$(PROG) $(LFLAGS) -t $(COMPRESSION) $(.CURDIR)/scan.l \
	| sed s,\"$(.CURDIR)/scan.l",\"scan.l", \
	| diff $(.CURDIR)/initscan.c -
	@echo "Check successful"

.include <bsd.prog.mk>
