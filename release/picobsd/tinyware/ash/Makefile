#	$NetBSD: Makefile,v 1.33 1997/07/04 21:40:55 christos Exp $
#	@(#)Makefile	8.4 (Berkeley) 5/5/95

WARNS=	1
PROG=	sh
SHSRCS=	alias.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c parser.c redir.c show.c trap.c output.c var.c
GENSRCS=arith.c arith_lex.c builtins.c builtins.h init.c nodes.c nodes.h \
	syntax.c syntax.h token.h
SRCS=	${SHSRCS} ${GENSRCS}

LDADD+=	-ll -ledit -ltermcap
DPADD+=	${LIBL} ${LIBEDIT} ${LIBTERMCAP}

LFLAGS= -8	# 8-bit lex scanner for arithmetic

CFLAGS+=-DSHELL -I. -I${.CURDIR}


.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf

CLEANFILES+= mkinit mknodes mksyntax
CLEANFILES+= mkinit.o mknodes.o mksyntax.o
CLEANFILES+= ${GENSRCS} y.tab.h

token.h: mktokens
	sh ${.CURDIR}/mktokens

builtins.c builtins.h: mkbuiltins builtins.def
	cd ${.CURDIR}; sh mkbuiltins ${.OBJDIR}

init.c: mkinit ${SHSRCS}
	./mkinit ${.ALLSRC:S/^mkinit$//}

nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

syntax.c syntax.h: mksyntax
	./mksyntax

.include <bsd.prog.mk>

${OBJS}: builtins.h nodes.h syntax.h token.h
