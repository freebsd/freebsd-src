#	@(#)Makefile	8.1 (Berkeley) 6/8/93

PROG=	sh
SRCS=	alias.c builtins.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c nodes.c options.c parser.c printf.c redir.c show.c \
	signames.c syntax.c trap.c output.c var.c
OBJS+=	init.o arith.o arith_lex.o
LDADD=	-ll -ledit -ltermcap -lcompat
LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf
CLEANFILES+=\
	builtins.c builtins.h init.c mkinit mknodes mksignames mksyntax \
	nodes.c nodes.h signames.c signames.h syntax.c syntax.h token.def \
	y.tab.h

.depend parser.o: token.def
token.def: mktokens
	sh ${.CURDIR}/mktokens

builtins.h builtins.c: ${.CURDIR}/mkbuiltins ${.CURDIR}/builtins
	cd ${.CURDIR}; sh mkbuiltins

init.c: mkinit ${SRCS}
	./mkinit '${CC} -c ${CFLAGS} init.c' ${.ALLSRC}

mkinit: ${.CURDIR}/mkinit.c
	${CC} ${CFLAGS} ${.CURDIR}/mkinit.c -o $@

nodes.c nodes.h: mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

mknodes: ${.CURDIR}/mknodes.c
	${CC} ${CFLAGS} ${.CURDIR}/mknodes.c -o $@

signames.c signames.h: mksignames
	./mksignames

mksignames: ${.CURDIR}/mksignames.c
	${CC} ${CFLAGS} ${.CURDIR}/mksignames.c -o $@

syntax.c syntax.h: mksyntax
	./mksyntax

mksyntax: ${.CURDIR}/mksyntax.c ${.CURDIR}/parser.h
	${CC} ${CFLAGS} ${.CURDIR}/mksyntax.c -o $@

.include <bsd.prog.mk>
