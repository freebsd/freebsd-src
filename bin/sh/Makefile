#	@(#)Makefile	8.4 (Berkeley) 5/5/95
# $FreeBSD$

PROG=	sh
SHSRCS=	alias.c arith.y arith_lex.l cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c redir.c show.c \
	test.c trap.c var.c
GENSRCS= builtins.c init.c nodes.c syntax.c
GENHDRS= builtins.h nodes.h syntax.h token.h y.tab.h
SRCS= ${SHSRCS} ${GENSRCS} ${GENHDRS} y.tab.h

# MLINKS for Shell built in commands for which there are no userland
# utilities of the same name are handled with the associated manpage,
# builtin.1 in share/man/man1/.  

DPADD+= ${LIBL} ${LIBEDIT} ${LIBTERMCAP}
LDADD+= -ll -ledit -ltermcap

LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# CFLAGS+= -g -DDEBUG=2

.PATH:	${.CURDIR}/bltin \
	${.CURDIR}/../../bin/test

CLEANFILES+= mkinit mkinit.o mknodes mknodes.o \
	mksyntax mksyntax.o
CLEANFILES+= ${GENSRCS} ${GENHDRS}

build-tools: mkinit mknodes mksyntax

.ORDER: builtins.c builtins.h
builtins.c builtins.h: mkbuiltins builtins.def
	cd ${.CURDIR}; sh mkbuiltins ${.OBJDIR}

init.c: mkinit alias.c eval.c exec.c input.c jobs.c options.c parser.c \
	redir.c trap.c var.c
	./mkinit ${.ALLSRC:S/^mkinit$//}

# XXX this is just to stop the default .c rule being used, so that the
# intermediate object has a fixed name.
# XXX we have a default .c rule, but no default .o rule.
.o:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
mkinit: mkinit.o
mknodes: mknodes.o
mksyntax: mksyntax.o

.ORDER: nodes.c nodes.h
nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

.ORDER: syntax.c syntax.h
syntax.c syntax.h: mksyntax
	./mksyntax

token.h: mktokens
	sh ${.CURDIR}/mktokens

.include <bsd.prog.mk>
