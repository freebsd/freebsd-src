#	@(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.11 1996/06/02 17:06:39 phk Exp $

PROG=	sh
SRCS=	alias.c builtins.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c nodes.c options.c parser.c printf.c redir.c show.c \
	signames.c syntax.c trap.c output.c var.c
OBJS+=	init.o arith.o arith_lex.o
DPADD=  ${LIBL} ${LIBEDIT} ${LIBTERMCAP}
LDADD=  -ll -ledit -ltermcap
LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debugging:
#CFLAGS+=-g -DDEBUG=2
.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf
CLEANFILES+=\
	builtins.c builtins.h init.c mkinit mknodes mksignames mksyntax \
	nodes.c nodes.h signames.c signames.h syntax.c syntax.h token.def \
	y.tab.h

.depend parser.o: token.def
token.def: mktokens
	sh ${.CURDIR}/mktokens

${.CURDIR}/builtins:
	# just override the default rule

builtins.h builtins.c: ${.CURDIR}/mkbuiltins ${.CURDIR}/builtins
	cd ${.CURDIR} && env OBJDIR=${.OBJDIR} sh mkbuiltins

init.c: mkinit ${SRCS}
	./mkinit '${CC} -c ${CFLAGS} ${LDFLAGS} init.c' ${.ALLSRC}
	touch ${.TARGET}

mkinit: ${.CURDIR}/mkinit.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mkinit.c -o $@

nodes.c nodes.h: mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

mknodes: ${.CURDIR}/mknodes.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mknodes.c -o $@

signames.c signames.h: mksignames
	./mksignames

mksignames: ${.CURDIR}/mksignames.c
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mksignames.c -o $@

syntax.c syntax.h: mksyntax
	./mksyntax

mksyntax: ${.CURDIR}/mksyntax.c ${.CURDIR}/parser.h
	${CC} ${CFLAGS} ${LDFLAGS} ${.CURDIR}/mksyntax.c -o $@

.include <bsd.prog.mk>
