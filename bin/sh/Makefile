#	@(#)Makefile	8.4 (Berkeley) 5/5/95
#	$Id: Makefile,v 1.17 1997/02/22 13:58:19 peter Exp $

PROG=	sh
SHSRCS=	alias.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c printf.c redir.c show.c \
	trap.c var.c
GENSRCS= arith.c arith_lex.c builtins.c init.c nodes.c syntax.c
SRCS= ${SHSRCS} ${GENSRCS}

LDADD+= -ll -ledit -ltermcap
DPADD+= ${LIBL} ${LIBEDIT} ${LIBTERMCAP}

LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# CFLAGS+= -g -DDEBUG=2

.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf

CLEANFILES+= builtins.h mkinit mkinit.o mknodes mknodes.o \
	mksyntax mksyntax.o \
	nodes.h syntax.h token.h y.tab.h
CLEANFILES+= ${GENSRCS}

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

${OBJS}: builtins.h nodes.h syntax.h
