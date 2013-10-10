#	@(#)Makefile	8.4 (Berkeley) 5/5/95
# $FreeBSD$

PROG=	sh
INSTALLFLAGS= -S
SHSRCS=	alias.c arith_yacc.c arith_yylex.c cd.c echo.c error.c eval.c \
	exec.c expand.c \
	histedit.c input.c jobs.c kill.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c printf.c redir.c show.c \
	test.c trap.c var.c
GENSRCS= builtins.c nodes.c syntax.c
GENHDRS= builtins.h nodes.h syntax.h token.h
SRCS= ${SHSRCS} ${GENSRCS} ${GENHDRS}

# MLINKS for Shell built in commands for which there are no userland
# utilities of the same name are handled with the associated manpage,
# builtin.1 in share/man/man1/.

DPADD= ${LIBEDIT} ${LIBTERMCAP}
LDADD= -ledit -ltermcap

CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# DEBUG_FLAGS+= -g -DDEBUG=2 -fno-inline
WARNS?=	2
WFORMAT=0

.PATH:	${.CURDIR}/bltin \
	${.CURDIR}/../kill \
	${.CURDIR}/../test \
	${.CURDIR}/../../usr.bin/printf

CLEANFILES+= mknodes mknodes.o \
	mksyntax mksyntax.o
CLEANFILES+= ${GENSRCS} ${GENHDRS}

build-tools: mknodes mksyntax

.ORDER: builtins.c builtins.h
builtins.c builtins.h: mkbuiltins builtins.def
	sh ${.CURDIR}/mkbuiltins ${.CURDIR}

# XXX this is just to stop the default .c rule being used, so that the
# intermediate object has a fixed name.
# XXX we have a default .c rule, but no default .o rule.
.o:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
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

regress:
	cd ${.CURDIR}/../../tools/regression/bin/sh && ${MAKE} SH=${.OBJDIR}/sh

.include <bsd.prog.mk>
