#	@(#)Makefile	8.4 (Berkeley) 5/5/95
#	$Id: Makefile,v 1.13 1996/09/01 10:19:18 peter Exp $

PROG=	sh
SHSRCS=	alias.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c parser.c printf.c redir.c show.c \
	trap.c output.c var.c

GENSRCS=arith.c arith_lex.c builtins.c init.c nodes.c syntax.c

SRCS=	${GENSRCS} ${SHSRCS}

DPADD+= ${LIBL} ${LIBEDIT} ${LIBTERMCAP}
LDADD+= -ll -ledit -ltermcap

LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# CFLAGS+= -g -DDEBUG=2

.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf

CLEANFILES+=	builtins.h mkinit mknodes mksyntax \
		nodes.h syntax.h token.h y.tab.h
CLEANFILES+=	${GENSRCS}

beforedepend: token.h

token.h: mktokens
	sh ${.CURDIR}/mktokens

builtins.h builtins.c: mkbuiltins builtins.def
	cd ${.CURDIR}; sh mkbuiltins ${.OBJDIR}

init.c: mkinit ${SHSRCS}
	./mkinit ${.ALLSRC:S/^mkinit$//}

nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

syntax.c syntax.h: mksyntax
	./mksyntax

parser.o: token.h

.include <bsd.prog.mk>
